# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import enum
import hashlib
import os
import re
from typing import List, Dict, Set, Union

import gn_utils
import targets as gn2bp_targets

NEWLINE = ' " +\n         "'


class JniZeroTargetType(enum.Enum):
    GENERATOR = enum.auto()
    REGISTRATION_GENERATOR = enum.auto()


def get_jni_zero_target_type(target):
    if target.script != '//third_party/jni_zero/jni_zero.py':
        return None
    if target.common.args[0] == 'generate-final':
        return JniZeroTargetType.REGISTRATION_GENERATOR
    return JniZeroTargetType.GENERATOR


def write_blueprint_key_value(output,
                              name,
                              value,
                              sort=True,
                              list_to_multiline_string=False):
    """Writes a Blueprint key-value pair to the output.

  If list_to_multiline_string is set, and the value is a list, then the output
  value will be the list elements concatenated into a single Blueprint string,
  formatted such that each list element appears on its own line. This is a
  purely cosmetic feature to make the Blueprint file more readable.
  """

    def escape(s):
        return str(s).replace('\\', '\\\\').replace('"', '\\"')

    if isinstance(value, bool):
        if value:
            output.append('    %s: true,' % name)
        else:
            output.append('    %s: false,' % name)
        return
    if not value:
        return
    if isinstance(value, set):
        value = sorted(value)
    if isinstance(value, list) and not list_to_multiline_string:
        output.append('    %s: [' % name)
        for item in sorted(value) if sort else value:
            output.append('        "%s",' % escape(item))
        output.append('    ],')
        return
    if isinstance(value, Target):
        value.to_string(output)
        return
    if isinstance(value, dict):
        kv_output = []
        for k, v in value.items():
            write_blueprint_key_value(kv_output, k, v)

        output.append('    %s: {' % name)
        for line in kv_output:
            output.append('    %s' % line)
        output.append('    },')
        return
    output.append(
        '    %s: "%s",' %
        (name,
         NEWLINE.join(
             escape(line)
             for line in (value if isinstance(value, list) else [value]))))


def label_to_module_name(label, context, short=False):
    """Turn a GN label (e.g., //:perfetto_tests) into a module name."""
    module = re.sub(r'^//:?', '', label)

    if short:
        # We want the module name to be short, but we still need it to be unique and
        # somewhat readable. To do this we replace just the path by a short hash.
        parts = module.rsplit('/', maxsplit=1)
        if len(parts) > 1 and len(parts[0]) > 10:
            module = hashlib.sha256(
                parts[0].encode('utf-8')).hexdigest()[:8] + '_' + parts[1]

    module = re.sub(r'[^a-zA-Z0-9_]', '_', module)

    if not module.startswith(context.module_prefix):
        return context.module_prefix + module
    return module


def get_protoc_module_name(gn, context):
    protoc_gn_target_name = gn.get_target(
        '//third_party/protobuf:protoc__toolchain_clang').name
    return label_to_module_name(protoc_gn_target_name, context)


class Target:
    """A target-scoped part of a module"""
    COMMON_FIELDS = {'compile_multilib', 'srcs'}
    SUPPORTED_FIELDS = set()
    _allowed_fields = SUPPORTED_FIELDS.union(COMMON_FIELDS)

    def __init__(self, arch, parent=None):
        self._arch = arch
        self._parent = parent
        self.compile_multilib = None
        self.srcs = set()

    def __setattr__(self, name, value):
        if name.startswith('_'):
            super().__setattr__(name, value)
            return
        if name not in self._allowed_fields:
            raise AttributeError(
                f"Target '{self._arch}' (parent type '{self._parent.type if self._parent else None}') "
                f"does not support attribute '{name}'")
        super().__setattr__(name, value)

    def to_string(self, output):
        nested_out = []
        for field in sorted(self._allowed_fields):
            # While sorting is a requirement for a deterministic output, sorting these flags correctly is
            # challenging. Sorting requires knowing the boundaries of each flag, but we cannot simply
            # assume flags are defined by whitespace, a leading - character, or something else.
            # With that in mind, we choose instead not to sort and instead we rely on GN's ordering of
            # these flags (and we assume that that ordering is deterministic).
            #
            # TODO: we should be able to get rid of this hardcoded list if
            # we are more consistent with how we use lists vs. sets
            # (list means order matters so should not be sorted, set
            # means order does not matter so should be sorted for
            # determinism)
            #
            # TODO: this logic is duplicated in `Module`.
            self._output_field(nested_out,
                               field,
                               sort=field not in ('cflags', 'cppflags',
                                                  'ldflags', 'flags'))

        if nested_out:
            output.append('    %s: {' % self._arch)
            for line in nested_out:
                output.append('    %s' % line)
            output.append('    },')

    def _output_field(self,
                      output,
                      name,
                      sort=True,
                      list_to_multiline_string=False):
        return write_blueprint_key_value(
            output,
            name,
            getattr(self, name),
            sort=sort,
            list_to_multiline_string=list_to_multiline_string)


class CcTarget(Target):
    SUPPORTED_FIELDS = {
        'shared_libs', 'static_libs', 'whole_static_libs', 'header_libs',
        'cflags', 'stl', 'cppflags', 'include_dirs', 'generated_headers',
        'export_generated_headers', 'ldflags', 'linker_scripts', 'stem'
    }
    _allowed_fields = SUPPORTED_FIELDS.union(Target.COMMON_FIELDS)

    def __init__(self, name, parent=None):
        super().__init__(name, parent)
        self.shared_libs = set()
        self.static_libs = set()
        self.whole_static_libs = set()
        self.header_libs = set()
        self.cflags = list()
        self.stl = None
        self.cppflags = list()
        self.include_dirs = set()
        self.generated_headers = set()
        self.export_generated_headers = set()
        self.ldflags = list()
        self.linker_scripts = set()
        self.stem = ""


class RustTarget(Target):
    SUPPORTED_FIELDS = {
        'edition', 'features', 'cfgs', 'flags', 'rustlibs', 'proc_macros',
        'shared_libs', 'static_libs', 'whole_static_libs'
    }
    _allowed_fields = SUPPORTED_FIELDS.union(Target.COMMON_FIELDS)

    def __init__(self, name, parent=None):
        super().__init__(name, parent)
        self.edition = ""
        self.features = set()
        self.cfgs = set()
        self.flags = list()
        self.rustlibs = set()
        self.proc_macros = set()
        self.shared_libs = set()
        self.static_libs = set()
        self.whole_static_libs = set()


COMMON_FIELDS = {
    'context', 'type', 'gn_target', 'name', 'comment', 'visibility',
    'default_applicable_licenses', 'default_visibility', 'build_file_path',
    'allow_rebasing', 'gn_type', 'target', 'jni_zero_target_type',
    'java_unfiltered_module', 'genrule_headers', 'genrule_srcs',
    'genrule_shared_libs', 'genrule_header_libs', 'include_build_directory',
    'apex_available', 'host_supported', 'host_cross_supported',
    'device_supported', 'defaults', 'srcs', 'role'
}

SKIP_IF_FALSY = {
    'host_supported',
    'whole_program_vtables',
    'afdo',
    'rtti',
}

SKIP_IF_TRUTHY = {
    'host_cross_supported',
    'device_supported',
}

ARCH_NAMES = {
    'android', 'android_x86', 'android_x86_64', 'android_arm', 'android_arm64',
    'android_riscv64', 'host', 'glibc'
}

COMMON_SOONG_FIELDS = {
    'name',
    'visibility',
    'default_applicable_licenses',
    'default_visibility',
    'include_build_directory',
    'apex_available',
    'host_supported',
    'host_cross_supported',
    'device_supported',
    'defaults',
}


class Module:
    """Base class for Soong modules."""
    SUPPORTED_FIELDS = set()

    def __init__(self,
                 mod_type,
                 name,
                 gn_target,
                 context,
                 is_test=False,
                 role=None):
        self.context = context
        self.type = mod_type
        self.gn_target = gn_target
        self.name = name
        self.role = role
        self._is_test = is_test
        self.comment = 'GN: ' + gn_target
        self.visibility = set()
        self.default_applicable_licenses = set()
        self.default_visibility = []
        self.gn_type = None
        self.build_file_path = None
        self.allow_rebasing = False
        self.jni_zero_target_type = None
        self.java_unfiltered_module = None
        self.include_build_directory = None
        self.apex_available = set()
        self.host_supported = False
        self.host_cross_supported = True
        self.device_supported = True
        self.defaults = set()
        self.srcs = set()
        self.target = dict()
        self._initialize_targets(Target)
        self.genrule_headers = set()
        self.genrule_srcs = set()
        self.genrule_shared_libs = set()
        self.genrule_header_libs = set()

    def _initialize_targets(self, target_class=Target):
        for arch in [
                'android', 'android_x86', 'android_x86_64', 'android_arm',
                'android_arm64', 'android_riscv64', 'host', 'glibc'
        ]:
            self.target[arch] = target_class(arch, self)

    def __setattr__(self, name, value):
        if name.startswith('_'):
            super().__setattr__(name, value)
            return
        allowed = self.SUPPORTED_FIELDS.union(COMMON_FIELDS).union(ARCH_NAMES)
        if name not in allowed:
            raise AttributeError(
                f"Module '{self.name}' (type '{self.type}') does not support attribute '{name}'"
            )
        super().__setattr__(name, value)

    def variant(self, arch_name):
        return self if arch_name == 'common' else self.target.get(arch_name)

    def to_string(self, output):
        if self.comment:
            output.append('// %s' % self.comment)
        output.append('%s {' % self.type)

        allowed_fields = self.SUPPORTED_FIELDS.union(COMMON_FIELDS)

        fields_to_output = self.SUPPORTED_FIELDS.union(COMMON_SOONG_FIELDS)

        for field in sorted(fields_to_output):
            # TODO: stop hardcoding behavior for these fields. This function
            # should not care about individual fields.
            value = getattr(self, field, None)
            if field in SKIP_IF_FALSY and not value:
                continue
            if field in SKIP_IF_TRUTHY and value:
                continue

            if field == 'cmd':
                self._output_field(output,
                                   'cmd',
                                   sort=False,
                                   list_to_multiline_string=True)
            elif field in ('cflags', 'cppflags', 'ldflags', 'flags'):
                # While sorting is a requirement for a deterministic output, sorting these flags correctly is
                # challenging. Sorting requires knowing the boundaries of each flag, but we cannot simply
                # assume flags are defined by whitespace, a leading - character, or something else.
                # With that in mind, we choose instead not to sort and instead we rely on GN's ordering of
                # these flags (and we assume that that ordering is deterministic).
                #
                # TODO: we should be able to get rid of this hardcoded list if
                # we are more consistent with how we use lists vs. sets
                # (list means order matters so should not be sorted, set
                # means order does not matter so should be sorted for
                # determinism)
                #
                # TODO: this logic is duplicated in `Target`.
                self._output_field(output, field, sort=False)
            else:
                self._output_field(output, field)

        if 'target' in allowed_fields and self.target:
            target_out = []
            for arch, target in sorted(self.target.items()):
                write_blueprint_key_value(target_out, arch, target)

            if target_out:
                output.append('    target: {')
                for line in target_out:
                    output.append('    %s' % line)
                output.append('    },')

        output.append('}')
        output.append('')

    def add_android_shared_lib(self, lib):
        if self.type.startswith('java'):
            raise Exception(
                'Adding Android shared lib for java_* targets is unsupported')
        if self.type == 'cc_binary_host':
            raise Exception(
                'Adding Android shared lib for host tool is unsupported')

        if self.host_supported:
            self.target['android'].shared_libs.add(lib)
        else:
            shared_libs = getattr(self, 'shared_libs', None)
            if shared_libs is not None:
                shared_libs.add(lib)
            else:
                raise AttributeError(
                    f"Module '{self.name}' of type '{self.type}' does not support shared_libs"
                )

    def is_test(self):
        if gn_utils.TESTING_SUFFIX in self.name:
            name_without_prefix = self.name[:self.name.find(gn_utils.
                                                            TESTING_SUFFIX)]
            return any(name_without_prefix == label_to_module_name(
                target, self.context)
                       for target in gn2bp_targets.DEFAULT_TESTS)
        return False

    def _output_field(self,
                      output,
                      name,
                      sort=True,
                      list_to_multiline_string=False):
        return write_blueprint_key_value(
            output,
            name,
            getattr(self, name),
            sort=sort,
            list_to_multiline_string=list_to_multiline_string)

    def is_compiled(self):
        return self.type not in ('cc_genrule', 'filegroup', 'java_genrule')

    def is_genrule(self):
        return self.type == "cc_genrule"

    def has_input_files(self):
        return self.type in [
            "java_library", "java_import", "rust_bindgen"
        ] or (self.srcs and len(self.srcs) > 0) or (self.target and any(
            len(target.srcs) > 0 for target in self.target.values()))

    def is_java_top_level_module(self):
        return self.java_unfiltered_module is not None


class CcModule(Module):
    SUPPORTED_FIELDS = {
        'srcs', 'shared_libs', 'static_libs', 'whole_static_libs', 'init_rc',
        'export_include_dirs', 'generated_headers', 'export_generated_headers',
        'export_static_lib_headers', 'export_header_lib_headers', 'cflags',
        'include_dirs', 'local_include_dirs', 'header_libs', 'strip', 'stl',
        'cpp_std', 'min_sdk_version', 'version_script', 'test_suites',
        'test_config', 'proto', 'linker_scripts', 'ldflags', 'cppflags',
        'stem', 'compile_multilib', 'c_std', 'whole_program_vtables', 'afdo',
        'rtti'
    }

    def __init__(self,
                 mod_type,
                 name,
                 gn_target,
                 context,
                 is_test=False,
                 role=None):
        super().__init__(mod_type, name, gn_target, context, is_test, role)
        self.shared_libs = set()
        self.static_libs = set()
        self.whole_static_libs = set()
        self.init_rc = set()
        self.export_include_dirs = set()
        self.generated_headers = set()
        self.export_generated_headers = set()
        self.export_static_lib_headers = set()
        self.export_header_lib_headers = set()
        self.cflags = list()
        self.include_dirs = set()
        self.local_include_dirs = set()
        self.header_libs = set()
        self.strip = dict()
        self.stl = None
        self.cpp_std = None
        self.min_sdk_version = None
        self.version_script = None
        self.test_suites = set()
        self.test_config = None
        self.proto = dict()
        self.linker_scripts = set()
        self.ldflags = list()
        self.cppflags = list()
        self.stem = None
        self.compile_multilib = None
        self.c_std = None
        self.whole_program_vtables = False
        self.afdo = None
        self.rtti = False
        self._initialize_targets(CcTarget)

    def has_input_files(self):
        return super().has_input_files() or bool(self.export_generated_headers
                                                 or self.generated_headers)


class JavaModule(Module):
    SUPPORTED_FIELDS = {
        'srcs', 'plugins', 'processor_class', 'sdk_version', 'javacflags',
        'jarjar_rules', 'jars', 'errorprone', 'libs', 'static_libs',
        'min_sdk_version'
    }

    def __init__(self,
                 mod_type,
                 name,
                 gn_target,
                 context,
                 is_test=False,
                 role=None):
        super().__init__(mod_type, name, gn_target, context, is_test, role)
        self.plugins = set()
        self.processor_class = None
        self.sdk_version = None
        self.javacflags = set()
        self.jarjar_rules = ""
        self.jars = set()
        self.errorprone = dict()
        self.libs = set()
        self.static_libs = set()
        self.min_sdk_version = None

    def add_java_dependency(self, dep_module):
        if self.java_unfiltered_module and dep_module.java_unfiltered_module:
            # Soong does not bubble up `libs` dependencies, so we have to
            # recurse and collect all the transitive dependencies ourselves. This
            # is not necessary when using `static_libs` as Soong does that for us
            # at build time.
            #
            # (You may wonder: "wait, doesn't Chromium already enforce that a
            # Java target list all the classes it refers to in its direct
            # dependencies? Why do we need to pull indirect dependencies then?"
            # Well the problem is javac needs to see some of the indirect
            # dependencies in some cases - see
            # https://crbug.com/400952169#comment4 - which means the direct
            # dependencies may not be enough.)
            self.java_unfiltered_module.libs.add(
                dep_module.java_unfiltered_module.name)
            self.java_unfiltered_module.libs.update(
                dep_module.java_unfiltered_module.libs)


class RustModule(Module):
    SUPPORTED_FIELDS = {
        'srcs', 'min_sdk_version', 'crate_name', 'crate_root', 'edition',
        'rustlibs', 'proc_macros', 'cargo_env_compat', 'cargo_pkg_version',
        'shared_libs', 'static_libs', 'header_libs', 'whole_static_libs'
    }

    def __init__(self,
                 mod_type,
                 name,
                 gn_target,
                 context,
                 is_test=False,
                 role=None):
        super().__init__(mod_type, name, gn_target, context, is_test, role)
        self.min_sdk_version = None
        self.shared_libs = set()
        self.static_libs = set()
        self.header_libs = set()
        self.whole_static_libs = set()
        self.crate_name = None
        self.crate_root = None
        self.edition = None
        self.rustlibs = set()
        self.proc_macros = set()
        self.cargo_env_compat = None
        self.cargo_pkg_version = None
        self._initialize_targets(RustTarget)


class RustBindgenModule(Module):
    SUPPORTED_FIELDS = {
        'srcs', 'min_sdk_version', 'crate_name', 'crate_root', 'wrapper_src',
        'source_stem', 'bindgen_flags', 'handle_static_inline',
        'static_inline_library', 'shared_libs', 'static_libs', 'cpp_std',
        'c_std', 'header_libs', 'whole_static_libs'
    }

    def __init__(self,
                 mod_type,
                 name,
                 gn_target,
                 context,
                 is_test=False,
                 role=None):
        super().__init__(mod_type, name, gn_target, context, is_test, role)
        self.min_sdk_version = None
        self.shared_libs = set()
        self.static_libs = set()
        self.header_libs = set()
        self.whole_static_libs = set()
        self.cpp_std = None
        self.c_std = None
        self.crate_name = None
        self.crate_root = None
        self.wrapper_src = ""
        self.source_stem = ""
        self.bindgen_flags = set()
        self.handle_static_inline = None
        self.static_inline_library = ""


class GenruleModule(Module):
    SUPPORTED_FIELDS = {
        'srcs', 'tools', 'cmd', 'out', 'tool_files', 'export_include_dirs'
    }

    def __init__(self,
                 mod_type,
                 name,
                 gn_target,
                 context,
                 is_test=False,
                 role=None):
        super().__init__(mod_type, name, gn_target, context, is_test, role)
        self.tools = set()
        self.cmd = None
        self.out = set()
        self.tool_files = set()
        self.export_include_dirs = set()


class AidlModule(Module):
    SUPPORTED_FIELDS = {'srcs', 'unstable', 'include_dirs'}

    def __init__(self,
                 mod_type,
                 name,
                 gn_target,
                 context,
                 is_test=False,
                 role=None):
        super().__init__(mod_type, name, gn_target, context, is_test, role)
        self.unstable = ""
        self.include_dirs = []


class FilegroupModule(Module):
    SUPPORTED_FIELDS = {'srcs', 'path'}

    def __init__(self,
                 mod_type,
                 name,
                 gn_target,
                 context,
                 is_test=False,
                 role=None):
        super().__init__(mod_type, name, gn_target, context, is_test, role)
        self.path = ""


class LicenseModule(Module):
    SUPPORTED_FIELDS = {'license_kinds', 'license_text'}

    def __init__(self,
                 mod_type,
                 name,
                 gn_target,
                 context,
                 is_test=False,
                 role=None):
        super().__init__(mod_type, name, gn_target, context, is_test, role)
        self.license_kinds = set()
        self.license_text = set()


class PackageModule(Module):
    SUPPORTED_FIELDS = set()


def create_module(mod_type,
                  name,
                  gn_target,
                  context,
                  is_test=False,
                  role=None):
    if mod_type in ('cc_library_static', 'cc_library_shared', 'cc_binary',
                    'cc_test', 'cc_defaults', 'cc_library_headers',
                    'cc_preprocess_no_configuration'):
        return CcModule(mod_type, name, gn_target, context, is_test, role)
    if mod_type in ('java_library', 'java_import', 'java_defaults'):
        return JavaModule(mod_type, name, gn_target, context, is_test, role)
    if mod_type in ('rust_ffi_static', 'rust_binary', 'rust_proc_macro'):
        return RustModule(mod_type, name, gn_target, context, is_test, role)
    if mod_type == 'rust_bindgen':
        return RustBindgenModule(mod_type, name, gn_target, context, is_test,
                                 role)
    if mod_type in ('cc_genrule', 'java_genrule', 'genrule'):
        return GenruleModule(mod_type, name, gn_target, context, is_test, role)
    if mod_type == 'aidl_interface':
        return AidlModule(mod_type, name, gn_target, context, is_test, role)
    if mod_type == 'filegroup':
        return FilegroupModule(mod_type, name, gn_target, context, is_test,
                               role)
    if mod_type == 'license':
        return LicenseModule(mod_type, name, gn_target, context, is_test, role)
    if mod_type == 'package':
        return PackageModule(mod_type, name, gn_target, context, is_test, role)
    raise ValueError(f"Unknown module type: {mod_type}")


class Blueprint:
    """In-memory representation of an Android.bp file."""

    def __init__(self, buildgn_directory_path: str = ""):
        self.modules = {}
        # Holds the BUILD.gn path which resulted in the creation of this Android.bp.
        self._buildgn_directory_path = buildgn_directory_path
        self._readme_location = buildgn_directory_path
        self._package_module = None
        self._license_module = None

    def add_module(self, module):
        """Adds a new module to the blueprint, replacing any existing module
        with the same name.

        Args:
            module: Module instance.
        """
        self.modules[module.name] = module

    def set_package_module(self, module):
        self._package_module = module

    def set_license_module(self, module):
        self._license_module = module

    def get_license_module(self):
        return self._license_module

    def set_readme_location(self, readme_path: str):
        self._readme_location = readme_path

    def get_readme_location(self):
        return self._readme_location

    def get_buildgn_location(self):
        return self._buildgn_directory_path

    def to_string(self, exclude_gn_targets=None):
        ret = []
        if self._package_module:
            self._package_module.to_string(ret)
        if self._license_module:
            self._license_module.to_string(ret)
        for m in sorted(self.modules.values(), key=lambda m: m.name):
            if (m.type != "cc_library_static" or m.has_input_files()) and (
                    exclude_gn_targets is None
                    or m.gn_target not in exclude_gn_targets):
                # Don't print cc_library_static with empty srcs. These attributes are already
                # propagated up the tree. Printing them messes the presubmits because
                # every module is compiled while those targets are not reachable in
                # a normal compilation path.
                m.to_string(ret)
        return ret
