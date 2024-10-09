# Copyright (C) 2022 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# A collection of utilities for extracting build rule information from GN
# projects.

import copy
import json
import logging as log
import os
import re
import collections

LINKER_UNIT_TYPES = ('executable', 'shared_library', 'static_library',
                     'source_set')
# This is a list of java files that should not be collected
# as they don't exist right now downstream (eg: apihelpers, cronetEngineBuilderTest).
# This is temporary solution until they are up-streamed.
JAVA_FILES_TO_IGNORE = (
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/ByteArrayCronetCallback.java",
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/ContentTypeParametersParser.java",
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/CronetRequestCompletionListener.java",
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/CronetResponse.java",
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/ImplicitFlowControlCallback.java",
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/InMemoryTransformCronetCallback.java",
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/JsonCronetCallback.java",
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/RedirectHandler.java",
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/RedirectHandlers.java",
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/StringCronetCallback.java",
    "//components/cronet/android/api/src/org/chromium/net/apihelpers/UrlRequestCallbacks.java",
    "//components/cronet/android/test/javatests/src/org/chromium/net/CronetEngineBuilderTest.java",
    # Api helpers does not exist downstream, hence the tests shouldn't be collected.
    "//components/cronet/android/test/javatests/src/org/chromium/net/apihelpers/ContentTypeParametersParserTest.java",
    # androidx-multidex is disabled on unbundled branches.
    "//base/test/android/java/src/org/chromium/base/multidex/ChromiumMultiDexInstaller.java",
    # This file is not used in aosp and depends on newer accessibility_test_framework.
    "//base/test/android/javatests/src/org/chromium/base/test/BaseActivityTestRule.java",
)
RESPONSE_FILE = '{{response_file_name}}'
TESTING_SUFFIX = "__testing"
AIDL_INCLUDE_DIRS_REGEX = r'--includes=\[(.*)\]'
AIDL_IMPORT_DIRS_REGEX = r'--imports=\[(.*)\]'
PROTO_IMPORT_DIRS_REGEX = r'--import-dir=(.*)'

def repo_root():
    """Returns an absolute path to the repository root."""
    return os.path.join(os.path.realpath(os.path.dirname(__file__)),
                        os.path.pardir)

def _get_build_path_from_label(target_name: str) -> str:
    """Returns the path to the BUILD file for which this target was declared."""
    return target_name[2:].split(":")[0]

def _clean_string(str):
    return str.replace('\\', '').replace('../../', '').replace('"', '').strip()

def _clean_aidl_import(orig_str):
    str = _clean_string(orig_str)
    src_idx = str.find("src/")
    if src_idx == -1:
        raise ValueError(f"Unable to clean aidl import {orig_str}")
    return str[:src_idx + len("src")]

def _extract_includes_from_aidl_args(args):
    ret = []
    for arg in args:
        is_match = re.match(AIDL_INCLUDE_DIRS_REGEX, arg)
        if is_match:
            local_includes = is_match.group(1).split(",")
            ret += [
                _clean_string(local_include)
                for local_include in local_includes
            ]
        # Treat imports like include for aidl by removing the package suffix.
        is_match = re.match(AIDL_IMPORT_DIRS_REGEX, arg)
        if is_match:
            local_imports = is_match.group(1).split(",")
            # Skip "third_party/android_sdk/public/platforms/android-34/framework.aidl" because Soong
            # already links against the AIDL framework implicitly.
            ret += [
                _clean_aidl_import(local_import)
                for local_import in local_imports
                if "framework.aidl" not in local_import
            ]
    return ret

def contains_aidl(sources):
    return any([src.endswith(".aidl") for src in sources])

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


class GnParser(object):
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

    class Target(object):
        """Reperesents A GN target.

        Maked properties are propagated up the dependency chain when a
        source_set dependency is encountered.
        """

        class Arch():
            """Architecture-dependent properties
        """

            def __init__(self):
                self.sources = set()
                self.cflags = set()
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

        def __init__(self, name, type):
            self.name = name  # e.g. //src/ipc:ipc

            VALID_TYPES = ('static_library', 'shared_library', 'executable',
                           'group', 'action', 'source_set', 'proto_library',
                           'copy', 'action_foreach', 'generated_file',
                           "rust_library", "rust_proc_macro")
            assert (type in VALID_TYPES
                    ), f"Unable to parse target {name} with type {type}."
            self.type = type
            self.testonly = False
            self.toolchain = None

            # These are valid only for type == proto_library.
            # This is typically: 'proto', 'protozero', 'ipc'.
            self.proto_plugin = None
            self.proto_paths = set()
            self.proto_exports = set()
            self.proto_in_dir = ""

            # TODO(primiano): consider whether the public section should be part of
            # bubbled-up sources.
            self.public_headers = set()  # 'public'

            # These are valid only for type == 'action'
            self.script = ''

            # These variables are propagated up when encountering a dependency
            # on a source_set target.
            self.libs = set()
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
            self.local_aidl_includes = set()
            # Each java_target will contain the transitive java sources found
            # in generate_jni type target.
            self.transitive_jni_java_sources = set()
            # Deps for JNI Registration. Those are not added to deps so that
            # the generated module would not depend on those deps.
            self.jni_registration_java_deps = set()
            # Path to the java jar path. This is used if the java library is
            # an import of a JAR like `android_java_prebuilt` targets in GN
            self.jar_path = ""
            self.sdk_version = ""
            self.build_file_path = ""
            self.crate_name = None
            self.crate_root = None

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
            return any(
                [name.startswith('android') for name in self.arch.keys()])

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
                        'proto_deps', 'libs', 'proto_paths'):
                getattr(self, key).update(getattr(other, key, []))

            for key_in_arch in ('cflags', 'defines', 'include_dirs', 'deps',
                                'ldflags'):
                getattr(self.arch[arch], key_in_arch).update(
                    getattr(other.arch[arch], key_in_arch, []))

        def get_archs(self):
            """ Returns a dict of archs without the common arch """
            return {
                arch: val
                for arch, val in self.arch.items() if arch != 'common'
            }

        def _finalize_set_attribute(self, key):
            # Target contains the intersection of arch-dependent properties
            getattr(self, key).update(
                set.intersection(
                    *
                    [getattr(arch, key)
                     for arch in self.get_archs().values()]))

            # Deduplicate arch-dependent properties
            for arch in self.get_archs().values():
                getattr(arch, key).difference_update(getattr(self, key))

        def _finalize_non_set_attribute(self, key):
            # Only when all the arch has the same non empty value, move the value to the target common
            val = getattr(list(self.get_archs().values())[0], key)
            if val and all([
                    val == getattr(arch, key)
                    for arch in self.get_archs().values()
            ]):
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
                        'ldflags', 'rust_flags'):
                self._finalize_attribute(key)

        def get_target_name(self):
            return self.name[self.name.find(":") + 1:]

    def __init__(self, builtin_deps, build_script_outputs):
        self.builtin_deps = builtin_deps
        self.build_script_outputs = build_script_outputs
        self.all_targets = {}
        self.jni_java_sources = set()

    def _get_response_file_contents(self, action_desc):
        # response_file_contents are formatted as:
        # ['--flags', '--flag=true && false'] and need to be formatted as:
        # '--flags --flag=\"true && false\"'
        flags = action_desc.get('response_file_contents', [])
        formatted_flags = []
        for flag in flags:
            if '=' in flag:
                key, val = flag.split('=')
                formatted_flags.append('%s=\\"%s\\"' % (key, val))
            else:
                formatted_flags.append(flag)

        return ' '.join(formatted_flags)

    def _is_java_group(self, type_, target_name):
        # Per https://chromium.googlesource.com/chromium/src/build/+/HEAD/android/docs/java_toolchain.md
        # java target names must end in "_java".
        # TODO: There are some other possible variations we might need to support.
        return type_ == 'group' and target_name.endswith('_java')

    def _get_arch(self, toolchain):
        if toolchain == '//build/toolchain/android:android_clang_x86':
            return 'android_x86', 'x86'
        elif toolchain == '//build/toolchain/android:android_clang_x64':
            return 'android_x86_64', 'x64'
        elif toolchain == '//build/toolchain/android:android_clang_arm':
            return 'android_arm', 'arm'
        elif toolchain == '//build/toolchain/android:android_clang_arm64':
            return 'android_arm64', 'arm64'
        elif toolchain == '//build/toolchain/android:android_clang_riscv64':
            return 'android_riscv64', 'riscv64'
        else:
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
                      java_group_name=None,
                      is_test_target=False):
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

        if 'target_type' in metadata.keys(
        ) and metadata["target_type"][0] == 'java_library':
            target.type = 'java_library'

        if target.name in self.builtin_deps:
            # return early, no need to parse any further as the module is a builtin.
            return target

        if (target_name.startswith("//build/rust/std")
                or desc.get("crate_name", "").endswith("_build_script")):
            # We intentionally don't parse build/rust/std as we use AOSP's stdlib.
            # Don't parse build_script as we can't execute them in AOSP, we use a different
            # source of truth.
            return target

        target.testonly = desc.get('testonly', False)

        deps = desc.get("deps", {})
        if desc.get("script",
                    "") == "//tools/protoc_wrapper/protoc_wrapper.py":
            target.type = 'proto_library'
            target.proto_plugin = "proto"
            target.proto_paths.update(self.get_proto_paths(desc))
            target.proto_exports.update(self.get_proto_exports(desc))
            target.proto_in_dir = self.get_proto_in_dir(desc)
            target.arch[arch].sources.update(desc.get('sources', []))
            target.arch[arch].inputs.update(desc.get('inputs', []))
        elif target.type == 'source_set':
            target.arch[arch].sources.update(
                source for source in desc.get('sources', [])
                if not source.startswith("//out"))
        elif target.type == "rust_executable":
            target.arch[arch].sources.update(
                source for source in desc.get('sources', [])
                if not source.startswith("//out"))
        elif target.is_linker_unit_type():
            target.arch[arch].sources.update(
                source for source in desc.get('sources', [])
                if not source.startswith("//out"))
        elif target.type == 'java_library':
            sources = set()
            for java_source in metadata.get("source_files", []):
                if not java_source.startswith(
                        "//out") and java_source not in JAVA_FILES_TO_IGNORE:
                    sources.add(java_source)
            target.sources.update(sources)
            # Metadata attributes must be list, for jar_path, it is always a list
            # of size one, the first element is an empty string if `jar_path` is not
            # defined otherwise it is a path.
            if metadata.get("jar_path", [""])[0]:
                target.jar_path = label_to_path(metadata["jar_path"][0])
            target.sdk_version = metadata.get('sdk_version', ['current'])[0]
            deps = metadata.get("all_deps", {})
            log.info('Found Java Target %s', target.name)
        elif target.script == "//build/android/gyp/aidl.py":
            target.type = "java_library"
            target.sources.update(desc.get('sources', {}))
            target.local_aidl_includes = _extract_includes_from_aidl_args(
                desc.get('args', ''))
        elif target.type in ['action', 'action_foreach']:
            target.arch[arch].inputs.update(desc.get('inputs', []))
            target.arch[arch].sources.update(desc.get('sources', []))
            outs = [re.sub('^//out/.+?/gen/', '', x) for x in desc['outputs']]
            target.arch[arch].outputs.update(outs)
            # While the arguments might differ, an action should always use the same script for every
            # architecture. (gen_android_bp's get_action_sanitizer actually relies on this fact.
            target.script = desc['script']
            target.arch[arch].args = desc['args']
            target.arch[
                arch].response_file_contents = self._get_response_file_contents(
                    desc)
            # _get_jni_registration_deps will return the dependencies of a target if
            # the target is of type `generate_jni_registration` otherwise it will
            # return an empty set.
            target.jni_registration_java_deps.update(
                _get_jni_registration_deps(gn_target_name, gn_desc))
            # JNI java sources are embedded as metadata inside `jni_headers` targets.
            # See https://source.chromium.org/chromium/chromium/src/+/main:third_party/jni_zero/jni_zero.gni;l=421;drc=78e8e27142ed3fddf04fbcd122507517a87cb9ad
            # for more details
            target.transitive_jni_java_sources.update(
                metadata.get("jni_source_files_abs", set()))
            self.jni_java_sources.update(
                metadata.get("jni_source_files_abs", set()))
        elif target.type == 'copy':
            # TODO: copy rules are not currently implemented.
            pass
        elif target.type == 'group':
            # Groups are bubbled upward without creating an equivalent GN target.
            pass
        elif target.type in ["rust_library", "rust_proc_macro"]:
            target.arch[arch].sources.update(
                source for source in desc.get('sources', [])
                if not source.startswith("//out"))
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
        target.arch[arch].cflags.update(
            desc.get('cflags', []) + desc.get('cflags_cc', []))
        target.libs.update(desc.get('libs', []))
        target.arch[arch].ldflags.update(desc.get('ldflags', []))
        target.arch[arch].defines.update(desc.get('defines', []))
        target.arch[arch].include_dirs.update(desc.get('include_dirs', []))
        target.output_name = desc.get('output_name', None)
        target.crate_name = desc.get("crate_name", None)
        target.crate_root = desc.get("crate_root", None)
        target.arch[arch].rust_flags = desc.get("rustflags", list())
        target.arch[arch].rust_flags.extend(
            self.build_script_outputs.get(
                label_without_toolchain(gn_target_name),
                {}).get(chromium_arch, list()))
        if target.type == "executable" and target.crate_root:
            # Find a more decisive way to figure out that this is a rust executable.
            # TODO: Add a metadata to the executable from Chromium side.
            target.type = "rust_executable"
        if "-frtti" in target.arch[arch].cflags:
            target.rtti = True

        for gn_dep_name in set(target.jni_registration_java_deps):
            dep = self.parse_gn_desc(gn_desc, gn_dep_name, java_group_name,
                                     is_test_target)
            target.transitive_jni_java_sources.update(
                dep.transitive_jni_java_sources)

        # Recurse in dependencies.
        for gn_dep_name in set(deps):
            dep = self.parse_gn_desc(gn_desc, gn_dep_name, java_group_name,
                                     is_test_target)

            if dep.type == 'proto_library':
                target.proto_deps.add(dep.name)
            elif dep.type == 'group':
                target.update(dep,
                              arch)  # Bubble up groups's cflags/ldflags etc.
                target.transitive_jni_java_sources.update(
                    dep.transitive_jni_java_sources)
            elif dep.type in ['action', 'action_foreach', 'copy']:
                target.arch[arch].deps.add(dep.name)
                target.transitive_jni_java_sources.update(
                    dep.transitive_jni_java_sources)
            elif dep.is_linker_unit_type():
                target.arch[arch].deps.add(dep.name)
            elif dep.type == "rust_executable":
                target.arch[arch].deps.add(dep.name)
            elif dep.type == 'java_library':
                target.deps.add(dep.name)
                target.transitive_jni_java_sources.update(
                    dep.transitive_jni_java_sources)
            elif dep.type in [
                    'rust_binary', "rust_library", "rust_proc_macro"
            ]:
                target.arch[arch].deps.add(dep.name)
            if dep.type in ['static_library', 'source_set']:
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
        return re.sub('^\.\./\.\./', '',
                      args[args.index('--proto-in-dir') + 1])
