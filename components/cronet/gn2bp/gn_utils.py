# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A collection of utilities for extracting build rule information from GN
# projects.

import copy
import json
import logging as log
import os
import re
import sys
import shlex

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
sys.path.insert(0, REPOSITORY_ROOT)
import components.cronet.gn2bp.common as gn2bp_common  # pylint: disable=wrong-import-position

LINKER_UNIT_TYPES = ('executable', 'shared_library', 'static_library',
                     'source_set')
RESPONSE_FILE = '{{response_file_name}}'
TESTING_SUFFIX = "__testing"
AIDL_INCLUDE_DIRS_REGEX = r'--includes=\[(.*)\]'
PROTO_IMPORT_DIRS_REGEX = r'--import-dir=(.*)'


def repo_root():
  """Returns an absolute path to the repository root."""
  return os.path.join(os.path.realpath(os.path.dirname(__file__)),
                      os.path.pardir)


def _get_build_path_from_label(target_name: str) -> str:
  """Returns the path to the BUILD file for which this target was declared."""
  return target_name[2:].split(":")[0]


def _clean_string(string):
  return string.replace('\\', '').replace('../../', '').replace('"', '').strip()


def _extract_rust_package_version(env_args):
  for arg in env_args:
    is_match = re.match(r'CARGO_PKG_VERSION=(.+)', arg)
    if is_match:
      return is_match.group(1)
  return None

def _extract_includes_from_aidl_args(args):
  for arg in args:
    is_match = re.match(AIDL_INCLUDE_DIRS_REGEX, arg)
    if is_match:
      local_includes = is_match.group(1).split(",")
      return [_clean_string(local_include) for local_include in local_includes]
  return []


def _get_jni_registration_deps(gn_target_name, gn_desc):
  # the dependencies are stored within another target with the same name
  # and a __java_sources suffix, see
  # https://source.chromium.org/chromium/chromium/src/+/main:third_party/jni_zero/jni_zero.gni;l=117;drc=78e8e27142ed3fddf04fbcd122507517a87cb9ad
  # for the auto-generated target name.
  jni_registration_java_target = f'{gn_target_name}__java_sources'
  if jni_registration_java_target in gn_desc.keys():
    return gn_desc[jni_registration_java_target]["deps"]
  return set()


def label_to_path(label):
  """Turn a GN output label (e.g., //some_dir/file.cc) into a path."""
  assert label.startswith('//')
  return label[2:] or ""


def label_without_toolchain(label):
  """Strips the toolchain from a GN label.

    Return a GN label (e.g //buildtools:protobuf(//gn/standalone/toolchain:
    gcc_like_host) without the parenthesised toolchain part.
    """
  return label.split('(')[0]


def _is_java_source(src):
  return os.path.splitext(src)[1] == '.java' and not src.startswith("//out/")


def _remove_out_prefix(label):
  return re.sub('^//out/.+?/(gen|obj)/', '', label)


def _filter_defines(defines):
  # These C++ defines are not actually used in code; Chromium only uses them to
  # force rebuilds on rolls of certain dependencies. They don't hurt, per se,
  # but they do create annoying diff noise on Android.bp files, so we drop them
  # for aesthetic/convenience reasons.
  EXCLUDED_DEFINES = {
      "CR_CLANG_REVISION", "CR_LIBCXX_REVISION", "ANDROID_NDK_VERSION_ROLL"
  }
  return (define for define in defines if not any(
      define.startswith(f"{excluded_define}=")
      for excluded_define in EXCLUDED_DEFINES))


class GnParser:
  """A parser with some cleverness for GN json desc files

    The main goals of this parser are:
    1) Deal with the fact that other build systems don't have an equivalent
       notion to GN's source_set. Conversely to Bazel's and Soong's filegroups,
       GN source_sets expect that dependencies, cflags and other source_set
       properties propagate up to the linker unit (static_library, executable or
       shared_library). This parser simulates the same behavior: when a
       source_set is encountered, some of its variables (cflags and such) are
       copied up to the dependent targets. This is to allow gen_xxx to create
       one filegroup for each source_set and then squash all the other flags
       onto the linker unit.
    2) Detect and special-case protobuf targets, figuring out the protoc-plugin
       being used.
    """

  class Target:
    """Reperesents A GN target.

        Maked properties are propagated up the dependency chain when a
        source_set dependency is encountered.
        """

    class Arch():
      """Architecture-dependent properties
        """

      def __init__(self):
        self.sources = set()
        self.cflags = []
        self.defines = set()
        self.include_dirs = set()
        self.deps = set()
        self.transitive_static_libs_deps = set()
        self.ldflags = set()

        # These are valid only for type == 'action'
        self.inputs = set()
        self.outputs = set()
        self.args = []
        self.response_file_contents = ''
        self.rust_flags = list()
        self.libs = set()

    def __init__(self, name, gn_type):
      self.name = name  # e.g. //src/ipc:ipc

      VALID_TYPES = ('static_library', 'shared_library', 'executable', 'group',
                     'action', 'source_set', 'proto_library', 'copy',
                     'action_foreach', 'generated_file', "rust_library",
                     "rust_proc_macro")
      assert (
          gn_type
          in VALID_TYPES), f"Unable to parse target {name} with type {gn_type}."
      self.type = gn_type
      self.testonly = False
      self.toolchain = None

      # These are valid only for type == proto_library.
      self.proto_paths = set()
      self.proto_exports = set()
      self.proto_in_dir = ""

      # TODO(primiano): consider whether the public section should be part of
      # bubbled-up sources.
      self.public_headers = set()  # 'public'

      # These are valid only for gn_type == 'action'
      self.script = ''

      self.proto_deps = set()
      self.rtti = False

      # TODO: come up with a better way to only run this once.
      # is_finalized tracks whether finalize() was called on this target.
      self.is_finalized = False
      # 'common' is a pseudo-architecture used to store common architecture dependent properties (to
      # make handling of common vs architecture-specific arguments more consistent).
      self.arch = {'common': self.Arch()}

      # This is used to get the name/version of libcronet
      self.output_name = None
      # Local Includes used for AIDL
      self.aidl_includes = set()
      # Each java_target will contain the transitive java sources found
      # in generate_jni gn_type target.
      self.transitive_jni_java_sources = set()
      # Deps for JNI Registration. Those are not added to deps so that
      # the generated module would not depend on those deps.
      self.jni_registration_java_deps = set()
      self.sdk_version = ""
      self.build_file_path = ""
      self.crate_name = None
      self.crate_root = None

      self.java_jar_excluded_patterns = []
      self.java_jar_included_patterns = []
      # This is only populated for build script actions. It refers to the directory for which
      # the original source files are.
      self.rust_source_dir = None
      self.rust_package_version = None

    # Properties to forward access to common arch.
    # TODO: delete these after the transition has been completed.
    @property
    def sources(self):
      return self.arch['common'].sources

    @sources.setter
    def sources(self, val):
      self.arch['common'].sources = val

    @property
    def inputs(self):
      return self.arch['common'].inputs

    @inputs.setter
    def inputs(self, val):
      self.arch['common'].inputs = val

    @property
    def outputs(self):
      return self.arch['common'].outputs

    @property
    def libs(self):
      return self.arch['common'].libs

    @outputs.setter
    def outputs(self, val):
      self.arch['common'].outputs = val

    @property
    def args(self):
      return self.arch['common'].args

    @args.setter
    def args(self, val):
      self.arch['common'].args = val

    @property
    def response_file_contents(self):
      return self.arch['common'].response_file_contents

    @response_file_contents.setter
    def response_file_contents(self, val):
      self.arch['common'].response_file_contents = val

    @property
    def cflags(self):
      return self.arch['common'].cflags

    @cflags.setter
    def cflags(self, val):
      self.arch['common'].cflags = val

    @property
    def defines(self):
      return self.arch['common'].defines

    @property
    def deps(self):
      return self.arch['common'].deps

    @deps.setter
    def deps(self, val):
      self.arch['common'].deps = val

    @property
    def rust_flags(self):
      return self.arch['common'].rust_flags

    @rust_flags.setter
    def rust_flags(self, val):
      self.arch['common'].rust_flags = val

    @property
    def include_dirs(self):
      return self.arch['common'].include_dirs

    @property
    def ldflags(self):
      return self.arch['common'].ldflags

    def host_supported(self):
      return 'host' in self.arch

    def device_supported(self):
      return any(name.startswith('android') for name in self.arch)

    def is_linker_unit_type(self):
      return self.type in LINKER_UNIT_TYPES

    def __lt__(self, other):
      if isinstance(other, self.__class__):
        return self.name < other.name
      raise TypeError(
          '\'<\' not supported between instances of \'%s\' and \'%s\'' %
          (type(self).__name__, type(other).__name__))

    def __repr__(self):
      return json.dumps(
          {
              k: (list(sorted(v)) if isinstance(v, set) else v)
              for (k, v) in self.__dict__.items()
          },
          indent=4,
          sort_keys=True)

    def update(self, other, arch):
      for key in ('cflags', 'defines', 'deps', 'include_dirs', 'ldflags',
                  'proto_deps', 'proto_paths'):
        val = getattr(self, key)
        if isinstance(val, set):
          # The pylint is confused as it does not understand that this line is protected
          # behind a type-check via `isinstance`
          # pylint: disable=no-member
          val.update(getattr(other, key, ()))
        elif isinstance(val, list):
          val.extend(getattr(other, key, []))

      for key_in_arch in ('cflags', 'defines', 'include_dirs', 'deps',
                          'ldflags', 'libs'):
        val = getattr(self.arch[arch], key_in_arch)
        if isinstance(val, set):
          val.update(getattr(other.arch[arch], key_in_arch, []))
        elif isinstance(val, list):
          val.extend(getattr(other.arch[arch], key_in_arch, []))


    def get_archs(self):
      """ Returns a dict of archs without the common arch """
      return {arch: val for arch, val in self.arch.items() if arch != 'common'}

    def _finalize_set_attribute(self, key):
      # Target contains the intersection of arch-dependent properties
      getattr(self, key).update(
          set.intersection(
              *[getattr(arch, key) for arch in self.get_archs().values()]))

      # Deduplicate arch-dependent properties
      for arch in self.get_archs().values():
        getattr(arch, key).difference_update(getattr(self, key))

    def _finalize_non_set_attribute(self, key):
      # Only when all the arch has the same non empty value, move the value to the target common
      val = getattr(list(self.get_archs().values())[0], key)
      if val and all(val == getattr(arch, key)
                     for arch in self.get_archs().values()):
        setattr(self, key, copy.deepcopy(val))

    def _finalize_attribute(self, key):
      val = getattr(self, key)
      if isinstance(val, set):
        self._finalize_set_attribute(key)
      elif isinstance(val, (list, str)):
        self._finalize_non_set_attribute(key)
      else:
        raise TypeError(f'Unsupported type: {type(val)}')

    def finalize(self):
      """Move common properties out of arch-dependent subobjects to Target object.

        TODO: find a better name for this function.
        """
      if self.is_finalized:
        return
      self.is_finalized = True

      if len(self.arch) == 1:
        return

      for key in ('sources', 'cflags', 'defines', 'include_dirs', 'deps',
                  'inputs', 'outputs', 'args', 'response_file_contents',
                  'ldflags', 'rust_flags', 'libs'):
        self._finalize_attribute(key)

    def get_target_name(self):
      return self.name[self.name.find(":") + 1:]

  def __init__(self, builtin_deps, build_script_outputs):
    self.builtin_deps = builtin_deps
    self.build_script_outputs = build_script_outputs
    self.all_targets = {}
    self.jni_java_sources = set()

  def _get_response_file_contents(self, action_desc):
    # GN response_file_contents docs state "The response file contents will
    # always be quoted and escaped according to Unix shell rules". Reproduce
    # GN's behavior.
    return ' '.join(
        shlex.quote(arg)
        for arg in action_desc.get('response_file_contents', []))

  def _is_java_group(self, type_, target_name):
    # Per https://chromium.googlesource.com/chromium/src/build/+/HEAD/android/docs/java_toolchain.md
    # java target names must end in "_java".
    # TODO: There are some other possible variations we might need to support.
    return type_ == 'group' and target_name.endswith('_java')

  def _get_arch(self, toolchain):
    if toolchain == '//build/toolchain/android:android_clang_x86':
      return 'android_x86', 'x86'
    if toolchain == '//build/toolchain/android:android_clang_x64':
      return 'android_x86_64', 'x64'
    if toolchain == '//build/toolchain/android:android_clang_arm':
      return 'android_arm', 'arm'
    if toolchain == '//build/toolchain/android:android_clang_arm64':
      return 'android_arm64', 'arm64'
    if toolchain == '//build/toolchain/android:android_clang_riscv64':
      return 'android_riscv64', 'riscv64'
    return 'host', 'host'

  def get_target(self, gn_target_name):
    """Returns a Target object from the fully qualified GN target name.

      get_target() requires that parse_gn_desc() has already been called.
      """
    # Run this every time as parse_gn_desc can be called at any time.
    for target in self.all_targets.values():
      target.finalize()

    return self.all_targets[label_without_toolchain(gn_target_name)]

  def parse_gn_desc(self,
                    gn_desc,
                    gn_target_name,
                    is_test_target=False,
                    custom_processor=None,
                    override_deps=None):
    """Parses a gn desc tree and resolves all target dependencies.

        It bubbles up variables from source_set dependencies as described in the
        class-level comments.
        """
    # Use name without toolchain for targets to support targets built for
    # multiple archs.
    target_name = label_without_toolchain(gn_target_name)
    desc = gn_desc[gn_target_name]
    type_ = desc['type']
    arch, chromium_arch = self._get_arch(desc['toolchain'])
    metadata = desc.get("metadata", {})

    if is_test_target:
      target_name += TESTING_SUFFIX

    target = self.all_targets.get(target_name)
    if target is None:
      target = GnParser.Target(target_name, type_)
      self.all_targets[target_name] = target

    if arch not in target.arch:
      target.arch[arch] = GnParser.Target.Arch()
    else:
      return target  # Target already processed.

    def turn_into_java_library(java_target):
      java_target.type = 'java_library'
      java_target.sdk_version = 'current'
      # Assume the target is unfiltered by default. This may be reassigned
      # later.
      java_target.unfiltered_java_target = java_target

    # In GN, java libraries are actually a hierarchy of targets with a `group`
    # target at the root. We surface the target as a `java_library` for clarity.
    # See below for more details on how we handle java_library.
    #
    # The reason why we do this now and not alongside the java_library logic is
    # so that, if this target is a builtin (see below), it is still returned as
    # a java_library, not as a group (which would just get ignored).
    if any(metadata_key in metadata
           for metadata_key in ("java_library_deps", "java_library_sources")):
      turn_into_java_library(target)

    if target.name in self.builtin_deps:
      # return early, no need to parse any further as the module is a builtin.
      return target

    if target_name.startswith("//build/rust/std"):
      # We intentionally don't parse build/rust/std as we use AOSP's stdlib.
      return target

    target.testonly = desc.get('testonly', False)

    deps = desc.get("deps", [])
    build_only_deps = []
    if custom_processor is not None:
      custom_processor(target, desc, deps, build_only_deps)
    elif desc.get("script", "") == "//tools/protoc_wrapper/protoc_wrapper.py":
      target.type = 'proto_library'
      target.proto_paths.update(self.get_proto_paths(desc))
      target.proto_exports.update(self.get_proto_exports(desc))
      target.proto_in_dir = self.get_proto_in_dir(desc)
      target.arch[arch].sources.update(desc.get('sources', []))
      target.arch[arch].inputs.update(desc.get('inputs', []))
      target.arch[arch].outputs.update(
          _remove_out_prefix(output) for output in desc['outputs'])
      target.arch[arch].args = desc['args']
    elif target.type == 'source_set':
      target.arch[arch].sources.update(source
                                       for source in desc.get('sources', [])
                                       if not source.startswith("//out"))
    elif target.type == "rust_executable":
      target.arch[arch].sources.update(source
                                       for source in desc.get('sources', [])
                                       if not source.startswith("//out"))
    elif target.is_linker_unit_type():
      target.arch[arch].sources.update(source
                                       for source in desc.get('sources', [])
                                       if not source.startswith("//out"))
    elif target.script == "//build/android/gyp/aidl.py":
      target.type = "aidl_interface"
      # It's assumed that all of AIDLs' attributes are not arch-specific.
      target.sources.update(desc.get('sources', {}))
      target.outputs.update([_remove_out_prefix(x) for x in desc['outputs']])
      target.aidl_includes = _extract_includes_from_aidl_args(
          desc.get('args', ''))
    elif target.type == "java_library":
      log.info('Found Java Target %s', target.name)

      # Java GN targets are... complicated. The relevant GN rules are in
      # //build/config/android/internal_rules.gni, specifically
      # java_library_impl. What makes this challenging is this GN rule is
      # extremely flexible and has tons of extra features - it's not just
      # running javac like Soong's java_library does. We don't support all of
      # GN's java_library features, but hopefully we support the subset that
      # matters to get things to work.
      #
      # The `java_library` GN rule generates not just one GN target, but a whole
      # hierarchy of subtargets (`_java__compile_java`, `_java__dex`, etc.)
      # behind a top-level `group` target.
      #
      # One approach could be to look at the various subtargets and piece
      # together the information we need (i.e. the Java source file paths, the
      # Java deps, the jar filtering rules, etc.). But that would require
      # non-trivial business logic and runs the risk of breakage when changes
      # are made to the internals of GN `java_library` rules (e.g.
      # https://crbug.com/412984664).
      #
      # Another approach could be to closely replicate the subtarget structure
      # in Soong (i.e. generate one module per subtarget), but that means we
      # would basically end up generating genrules that indirectly call javac
      # instead of generating `java_library` modules, which feels extremely
      # awkward, impractical and unlikely to work.
      #
      # Instead, we rely entirely on GN target metadata that the `java_library`
      # GN rule helpfully attaches to the top-level group target. This makes the
      # analysis trivial and completely decouples this code from the internal
      # structure of the `java_library` GN subtargets.

      inputs = metadata.get("java_library_inputs", [])
      target.sources.update(input for input in inputs
                            if not input.startswith('//out/'))
      target.inputs.update(_remove_out_prefix(input) for input in inputs)

      deps.clear()
      deps.extend(metadata.get("java_library_deps", []))

      target.java_jar_excluded_patterns = metadata.get(
          "java_library_jar_excluded_patterns", [])
      target.java_jar_included_patterns = metadata.get(
          "java_library_jar_included_patterns", [])

      android_sdk_dep = metadata.get("java_library_android_sdk_dep", None)
      if android_sdk_dep is not None:
        assert len(android_sdk_dep) == 1, target.name
        android_sdk_dep = android_sdk_dep[0]
        if android_sdk_dep == "//third_party/android_sdk:android_sdk_java":
          target.sdk_version = "current"
        elif android_sdk_dep == "//third_party/android_sdk:public_framework_system_java":
          target.sdk_version = "system_current"
        else:
          raise ValueError(
              f"Unexpected android_sdk_dep: {android_sdk_dep} for target {target.name}"
          )
    elif target.script == "//build/rust/gni_impl/run_bindgen.py":
      # rust_bindgen is a supported module in Soong but GN depend on actions
      # so we need to copy the action fields (sources, outputs and args) in
      # order to correctly generate the `rust_bindgen` module.
      target.sources.update(desc.get('sources', []))
      outs = [_remove_out_prefix(x) for x in desc['outputs']]
      target.outputs.update(outs)
      target.args = desc['args']
      target.type = "rust_bindgen"
    elif (target.type in [
        'action', 'action_foreach'
        # GN's copy is translated to Soong by making it look like a GN's action
        # with a special //cp script. This works well for its only usage:
        # //base:build_date_header. As the list of supported copy target grows, we might
        # need to revisit this decision.
    ]) or (desc['type'] == 'copy' and target.name
           in ['//base:build_date_header', '//base:build_date_header__testing']):
      target.arch[arch].inputs.update(desc.get('inputs', []))
      target.arch[arch].sources.update(desc.get('sources', []))
      outs = [_remove_out_prefix(x) for x in desc['outputs']]
      target.arch[arch].outputs.update(outs)
      # We need to check desc['type'], not target.type: targets go through
      # this code multiple times. If we checked for target.type, the second
      # time we parsed a copy target, we would take the else branch.
      if desc['type'] == 'copy':
        target.type = 'action'
        target.script = '//cp'
      else:
        # While the arguments might differ, an action should always use the same script for every
        # architecture. (gen_android_bp's get_action_sanitizer actually relies on this fact.
        target.script = desc['script']
        target.arch[arch].args = desc['args']
      target.arch[
          arch].response_file_contents = self._get_response_file_contents(desc)
      # _get_jni_registration_deps will return the dependencies of a target if
      # the target is of type `generate_jni_registration` otherwise it will
      # return an empty set.
      target.jni_registration_java_deps.update(
          _get_jni_registration_deps(gn_target_name, gn_desc))
      # JNI java sources are embedded as metadata inside `jni_headers` targets.
      # See https://source.chromium.org/chromium/chromium/src/+/main:third_party/jni_zero/jni_zero.gni;l=421;drc=78e8e27142ed3fddf04fbcd122507517a87cb9ad
      # for more details
      target.transitive_jni_java_sources.update(
          metadata.get("jni_source_files", set()))
      self.jni_java_sources.update(metadata.get("jni_source_files", set()))
      if gn2bp_common.is_rust_build_script(target.script):

        def _extract_crate_path(args):
          return args[args.index("--src-dir") + 1].replace("../../", "")

        target.rust_source_dir = _extract_crate_path(desc['args'])
        # Don't continue the dependencies exploration.
        return target
    elif target.type == 'group':
      # Group targets are bubbled upward without creating an equivalent GN target.
      pass
    elif target.type == 'copy':
      # Copy targets, except for a few exception (see handling of action
      # targets above), are bubbled upward without creating an equivalent
      # GN target.
      pass
    elif target.type in ["rust_library", "rust_proc_macro"]:
      target.arch[arch].sources.update(source
                                       for source in desc.get('sources', [])
                                       if not source.startswith("//out"))
      target.rust_package_version = _extract_rust_package_version(
          desc['rustenv'])
    else:
      raise Exception(
          f"Encountered GN target with unknown type\nCulprit target: {gn_target_name}\ntype: {target.type}"
      )

    # Default for 'public' is //* - all headers in 'sources' are public.
    # TODO(primiano): if a 'public' section is specified (even if empty), then
    # the rest of 'sources' is considered inaccessible by gn. Consider
    # emulating that, so that generated build files don't end up with overly
    # accessible headers.
    public_headers = [x for x in desc.get('public', []) if x != '*']
    target.public_headers.update(public_headers)
    target.build_file_path = _get_build_path_from_label(target_name)
    target.arch[arch].cflags.extend(
        desc.get('cflags', []) + desc.get('cflags_cc', []))
    target.arch[arch].libs.update(desc.get('libs', []))
    target.arch[arch].ldflags.update(desc.get('ldflags', []))
    target.arch[arch].defines.update(_filter_defines(desc.get('defines', [])))
    target.arch[arch].include_dirs.update(desc.get('include_dirs', []))
    target.output_name = desc.get('output_name', None)
    target.crate_name = desc.get("crate_name", None)
    target.crate_root = desc.get("crate_root", None)
    target.arch[arch].rust_flags = desc.get("rustflags", list())
    target.arch[arch].rust_flags.extend(
        self.build_script_outputs.get(label_without_toolchain(gn_target_name),
                                      {}).get(chromium_arch, list()))
    if target.type == "executable" and target.crate_root:
      # Find a more decisive way to figure out that this is a rust executable.
      # TODO: Add a metadata to the executable from Chromium side.
      target.type = "rust_executable"
    if "-frtti" in target.arch[arch].cflags:
      target.rtti = True

    for gn_dep_name in set(target.jni_registration_java_deps):
      dep = self.parse_gn_desc(gn_desc, gn_dep_name, is_test_target)
      target.transitive_jni_java_sources.update(dep.transitive_jni_java_sources)

    if override_deps is not None:
      deps = override_deps

    # Recurse in dependencies.
    for gn_dep_name in set(deps) | set(build_only_deps):
      dep = self.parse_gn_desc(gn_desc, gn_dep_name, is_test_target)

      if dep.type == 'proto_library':
        target.proto_deps.add(dep.name)
      elif dep.type == 'copy':
        target.update(dep, arch)
      elif dep.type == 'group':
        target.update(dep, arch)  # Bubble up groups's cflags/ldflags etc.
        target.transitive_jni_java_sources.update(
            dep.transitive_jni_java_sources)
      elif dep.type in ['action', 'action_foreach']:
        target.arch[arch].deps.add(dep.name)
        target.transitive_jni_java_sources.update(
            dep.transitive_jni_java_sources)
      elif dep.is_linker_unit_type():
        target.arch[arch].deps.add(dep.name)
      elif dep.type == 'aidl_interface':
        target.arch[arch].deps.add(dep.name)
      elif dep.type == "rust_executable":
        target.arch[arch].deps.add(dep.name)
      elif dep.type == 'java_library':
        if gn_dep_name in build_only_deps:
          # Chromium builds Java code against the unfiltered dependencies
          # (_java__header). This reproduces this behavior.
          target.build_only_deps.add(dep.unfiltered_java_target.name)
        else:
          target.deps.add(dep.name)
        target.transitive_jni_java_sources.update(
            dep.transitive_jni_java_sources)
      elif dep.type in [
          'rust_binary', "rust_library", "rust_proc_macro", "rust_bindgen"
      ]:
        target.arch[arch].deps.add(dep.name)
      if dep.type in ['static_library', 'source_set', 'rust_library']:
        # Bubble up static_libs and source_set. Necessary, since soong does not propagate
        # static_libs up the build tree.
        # Source sets are later translated to static_libraries, so it makes sense
        # to reuse transitive_static_libs_deps.
        target.arch[arch].transitive_static_libs_deps.add(dep.name)

      if arch in dep.arch:
        target.arch[arch].transitive_static_libs_deps.update(
            dep.arch[arch].transitive_static_libs_deps)
        target.arch[arch].deps.update(
            target.arch[arch].transitive_static_libs_deps)

    return target

  def get_proto_exports(self, proto_desc):
    # exports in metadata will be available for source_set targets.
    metadata = proto_desc.get('metadata', {})
    return metadata.get('exports', [])

  def get_proto_paths(self, proto_desc):
    args = proto_desc.get('args')
    proto_paths = set()
    for arg in args:
      is_match = re.match(PROTO_IMPORT_DIRS_REGEX, arg)
      if is_match:
        proto_paths.add(re.sub('^\.\./\.\./', '', is_match.group(1)))
    return proto_paths

  def get_proto_in_dir(self, proto_desc):
    args = proto_desc.get('args')
    return re.sub('^\.\./\.\./', '', args[args.index('--proto-in-dir') + 1])
