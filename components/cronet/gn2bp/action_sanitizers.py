# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import copy
import operator
import shlex
from typing import List, Dict, Set, Union

import gn_utils
import soong_ast
import common

from arguments import CommandLineUtility


class BaseActionSanitizer():

    def __init__(self, target, arch, context):
        self.context = context
        # Just to be on the safe side, create a deep-copy.
        self.target = copy.deepcopy(target)
        if arch:
            # Merge arch specific attributes
            self.target.common.sources |= arch.sources
            self.target.common.inputs |= arch.inputs
            self.target.common.outputs |= arch.outputs
            self.target.script = self.target.script or arch.script
            self.target.common.args = self.target.common.args or arch.args
            self.target.common.response_file_contents = \
              self.target.common.response_file_contents or arch.response_file_contents
        self.args = CommandLineUtility(self.target.common.args or [])

    def get_name(self):
        return soong_ast.label_to_module_name(self.target.name, self.context)

    def _sanitize_filepath_with_location_tag(self, arg):
        if arg.startswith('../../'):
            arg = self._sanitize_filepath(arg)
            arg = self._add_location_tag(arg)
        return arg

    # wrap filename in location tag.
    def _add_location_tag(self, filename):
        return '$(location %s)' % filename

    # applies common directory transformation that *should* be universally applicable.
    # TODO: verify if it actually *is* universally applicable.
    def _sanitize_filepath(self, filepath):
        # Careful, order matters!
        # delete all leading ../
        filepath = re.sub('^(\.\./)+', '', filepath)
        filepath = re.sub('^gen/jni_headers', '$(genDir)', filepath)
        filepath = re.sub('^gen', '$(genDir)', filepath)
        return filepath

    def get_pre_cmd(self):
        pre_cmd = []
        out_dirs = [
            out[:out.rfind("/")] for out in self.target.common.outputs
            if "/" in out
        ]
        # Sort the list to make the output deterministic.
        for out_dir in sorted(set(out_dirs)):
            pre_cmd.append("mkdir -p $(genDir)/{} && ".format(out_dir))
        return pre_cmd

    def get_base_cmd(self):
        # TODO: most sanitizer logic does not really handle "$" characters very
        # well, and will likely do the wrong thing if the GN target contains args
        # with literal "$" characters in them. Also, if a sanitizer deliberately
        # shoves a $() macro in an arg, we still run that through shell quoting,
        # which does preserve the "$" but that's mostly luck. We should design
        # a better mechanism for handling "$" and $() macros.
        return (([
            f"echo {shlex.quote(self.target.common.response_file_contents)} |"
        ] if self.target.common.response_file_contents else []) +
                [f"$(location {gn_utils.label_to_path(self.target.script)})"] +
                [shlex.quote(arg) for arg in self.args.get_args()])

    def get_cmd(self):
        # Note: don't be confused by the return type. This function returns a list,
        # but the list is *NOT* an argv array, it's a list of lines for Blueprint
        # file formatting for cosmetic purposes. The actual command is the list
        # elements concatenated together into a single string, which is ultimately
        # fed as a shell command at build time. This means what we are returning
        # here is expected to have been properly shell-escaped beforehand.
        return self.get_pre_cmd() + self.get_base_cmd()

    def get_outputs(self):
        return self.target.common.outputs

    def get_srcs(self):
        # gn treats inputs and sources for actions equally.
        # soong only supports source files inside srcs, non-source files are added as
        # tool_files dependency.
        files = self.target.common.sources.union(self.target.common.inputs)
        return {
            gn_utils.label_to_path(file)
            for file in files if common.is_supported_source_file(file)
        }

    def get_tools(self):
        return set()

    def get_tool_files(self):
        # gn treats inputs and sources for actions equally.
        # soong only supports source files inside srcs, non-source files are added as
        # tool_files dependency.
        files = self.target.common.sources.union(self.target.common.inputs)
        tool_files = {
            gn_utils.label_to_path(file)
            # Files that starts with "out/" are usually an output of another action.
            # This is under the assumption that we generate the desc files in an
            # out/ directory usually.
            for file in files if not common.is_supported_source_file(file)
            and not file.startswith("//out/")
        }
        tool_files.add(gn_utils.label_to_path(self.target.script))
        # Make sure there is no duplication between `srcs` and `tool_files` - Soong
        # fails with a "multiple locations for label" error otherwise.
        tool_files -= self.get_srcs()
        return tool_files

    def _sanitize_args(self):
        # Handle passing parameters via response file by piping them into the script
        # and reading them from /dev/stdin.

        use_response_file = self.args.has_arg(gn_utils.RESPONSE_FILE)
        if use_response_file:
            # Replace {{response_file_contents}} with /dev/stdin
            self.args.update_all_args(lambda it: '/dev/stdin'
                                      if it == gn_utils.RESPONSE_FILE else it)

    def _sanitize_inputs(self):
        pass

    def get_deps(self):
        return self.target.common.deps

    def sanitize(self):
        self._sanitize_args()
        self._sanitize_inputs()

    # Whether this target generates header files
    def is_header_generated(self):
        return any(
            os.path.splitext(it)[1] == '.h' for it in self.get_outputs())


class GenerateCanonicalLocalesListSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.set_arg_at(0, '$(out)')
        super()._sanitize_args()

    def is_header_generated(self):
        return True


class GenerateKnownBcp47SubtagsSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.set_arg_at(0, '$(out)')
        super()._sanitize_args()

    def is_header_generated(self):
        return True


class WriteBuildDateHeaderSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.set_arg_at(0, '$(out)')
        super()._sanitize_args()


class WriteGenerateAllowlistFromHistogramsFileSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.set_flag_value('--output_dir', '.')
        self.args.set_flag_value('--file', '$(out)')
        self.args.update_flag_value('--input',
                                    self._sanitize_filepath_with_location_tag)
        super()._sanitize_args()


class WriteBuildFlagHeaderSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.set_flag_value('--gen-dir', '.')
        self.args.set_flag_value('--output', '$(out)')
        super()._sanitize_args()


class PerfettoWriteBuildFlagHeaderSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.set_flag_value('--out', '$(out)')
        super()._sanitize_args()


class GnRunBinarySanitizer(BaseActionSanitizer):

    def __init__(self, target, arch, context):
        super().__init__(target, arch, context)
        self.binary_to_target = {
            "clang_x64/transport_security_state_generator":
            f"{context.module_prefix}net_tools_transport_security_state_generator_transport_security_state_generator__toolchain_clang__testing",
        }
        self.binary = self.binary_to_target[self.args.get_args()[0]]

    def _replace_gen_with_location_tag(self, arg):
        if arg.startswith("gen/"):
            return "$(location %s)" % arg.replace("gen/", "")
        return arg

    def _replace_binary(self, arg):
        if arg in self.binary_to_target:
            return '$(location %s)' % self.binary
        return arg

    def _remove_python_args(self):
        self.args.set_args(
            [arg for arg in self.args.get_args() if "python3" not in arg])

    def _sanitize_args(self):
        self.args.update_all_args(self._sanitize_filepath_with_location_tag)
        self.args.update_all_args(self._replace_gen_with_location_tag)
        self.args.update_all_args(self._replace_binary)
        self._remove_python_args()
        super()._sanitize_args()

    def get_tools(self):
        tools = super().get_tools()
        tools.add(self.binary)
        return tools

    def get_cmd(self):
        # Remove the script and use the binary right away
        return self.get_pre_cmd() + [
            shlex.quote(arg) for arg in self.args.get_args()
        ]


class JniGeneratorSanitizer(BaseActionSanitizer):

    def __init__(self, target, arch, is_test_target, context):
        self.is_test_target = is_test_target
        super().__init__(target, arch, context)

    def get_srcs(self):
        all_srcs = super().get_srcs()
        all_srcs.update({
            gn_utils.label_to_path(file)
            for file in self.target.transitive_jni_java_sources
            if common.is_supported_source_file(file)
        })
        return set(src for src in all_srcs if src.endswith(".java"))

    def _add_location_tag_to_filepath(self, arg):
        if not arg.endswith('.class'):
            # --input_file supports both .class specifiers or source files as arguments.
            # Only source files need to be wrapped inside a $(location <label>) tag.
            arg = self._add_location_tag(arg)
        return arg

    def _sanitize_args(self):
        self.args.set_flag_value('--jar-file',
                                 '$(location :current_android_jar)', False)
        if self.args.has_arg('--jar-file'):
            self.args.set_flag_value('--javap', '$(location :javap)')
        self.args.update_flag_value('--srcjar-path', self._sanitize_filepath,
                                    False)
        self.args.update_flag_value('--output-dir', self._sanitize_filepath)
        self.args.update_flag_value('--extra-include', self._sanitize_filepath,
                                    False)
        self.args.update_flag_value('--placeholder-srcjar-path',
                                    self._sanitize_filepath, False)
        self.args.update_list_arg('--input-file', self._sanitize_filepath)
        self.args.update_list_arg('--input-file',
                                  self._add_location_tag_to_filepath)

        self.args.remove_flag('--package-prefix', throw_if_absent=False)
        self.args.remove_flag('--package-prefix-filter', throw_if_absent=False)
        if not self.is_test_target and not self.args.has_arg('--jar-file'):
            # Don't jarjar classes that already exists within the java SDK. The headers generated
            # from those genrule can simply call into the original class as it exists outside
            # of cronet's jar.
            # Only jarjar platform code
            self.args.append_flag_value('--package-prefix',
                                        'android.net.http.internal')
        super()._sanitize_args()

    def get_outputs(self):
        outputs = set()
        for out in super().get_outputs():
            # fix target.output directory to match #include statements.
            outputs.add(re.sub('^jni_headers/', '', out))
        return outputs

    def get_tool_files(self):
        tool_files = super().get_tool_files()

        # Filter android.jar and add :current_android_jar
        tool_files = {
            file
            if not file.endswith('android.jar') else ':current_android_jar'
            for file in tool_files
        }
        # Filter bin/javap
        tool_files = {
            file
            for file in tool_files if not file.endswith('bin/javap')
        }

        # TODO: Remove once https://chromium-review.googlesource.com/c/chromium/src/+/5370266 has made
        #       its way to AOSP
        # Files not specified in anywhere but jni_generator.py imports this file
        tool_files.add('third_party/jni_zero/codegen/header_common.py')
        tool_files.add('third_party/jni_zero/codegen/placeholder_java_type.py')

        return tool_files

    def get_tools(self):
        tools = super().get_tools()
        if self.args.has_arg('--jar-file'):
            tools.add(":javap")
        return tools


class JavaJniGeneratorSanitizer(JniGeneratorSanitizer):

    def __init__(self, target, arch, is_test_target, context):
        self.is_test_target = is_test_target
        super().__init__(target, arch, is_test_target, context)

    def get_outputs(self):
        # fix target.output directory to match #include statements.
        outputs = {
            re.sub('^jni_headers/', '', out)
            for out in super().get_outputs()
        }
        self.target.common.outputs = [
            out for out in outputs if out.endswith(".srcjar")
        ]
        return outputs

    def get_deps(self):
        return {}

    def get_name(self):
        name = super().get_name() + "__java"
        return name


class JniRegistrationGeneratorSanitizer(BaseActionSanitizer):

    def __init__(self, target, arch, is_test_target, context, mode=None):
        self.is_test_target = is_test_target
        self.mode = mode
        super().__init__(target, arch, context)

    def get_name(self):
        name = super().get_name()
        if self.mode == 'headers':
            name += "_headers"
        return name

    def get_srcs(self):
        all_srcs = super().get_srcs()
        all_srcs.update({
            gn_utils.label_to_path(file)
            for file in self.target.transitive_jni_java_sources
            if common.is_supported_source_file(file)
        })
        return set(src for src in all_srcs if src.endswith(".java"))

    def _sanitize_inputs(self):
        self.target.common.inputs = [
            file for file in self.target.common.inputs
            if not file.startswith('//out/')
        ]

    def get_outputs(self):
        outputs = set()
        for out in super().get_outputs():
            # placeholder.srcjar contains empty placeholder classes used to compile generated java files
            # without any other deps. This is not used in aosp.
            if out.endswith("_placeholder.srcjar"):
                continue
            if self._should_exclude_output(out):
                continue
            # fix target.output directory to match #include statements.
            outputs.add(re.sub('^jni_headers/', '', out))
        return outputs

    def _should_exclude_output(self, out):
        if self.mode == 'headers':
            return not out.endswith(".h")
        if self.mode == 'source':
            return not out.endswith(".cc")
        return out.endswith(".srcjar")

    def _sanitize_args(self):
        self.args.update_flag_value('--depfile', self._sanitize_filepath)
        self.args.update_flag_value('--srcjar-path', self._sanitize_filepath)
        self.args.update_flag_value('--header-path',
                                    self._sanitize_filepath,
                                    throw_if_absent=False)
        self.args.update_flag_value('--impl-path',
                                    self._sanitize_filepath,
                                    throw_if_absent=False)
        self.args.update_flag_value('--placeholder-srcjar-path',
                                    self._sanitize_filepath, False)
        self.args.remove_flag('--depfile', False)
        self.args.set_flag_value('--java-sources-file',
                                 '$(genDir)/java_sources.json')

        self.args.remove_flag('--package-prefix', throw_if_absent=False)
        self.args.remove_flag('--package-prefix-filter', throw_if_absent=False)
        if not self.is_test_target:
            # Only jarjar platform code
            self.args.append_flag_value('--package-prefix',
                                        'android.net.http.internal')
        super()._sanitize_args()

    def get_cmd(self):
        base_cmd = super().get_base_cmd()
        # Path in the original sources file does not work in genrule.
        # So creating sources file in cmd based on the srcs of this target.
        # Adding ../$(current_dir)/ to the head because jni_registration_generator.py uses the files
        # whose path startswith(..)
        module_name = ''
        if self.args.has_arg('--module-name'):
            module_name = self.args.get_flag_value('--module-name')

        # This script is used to generate a `java_sources.json` file which is
        # required by the `jni_registration_generator.py` script.
        # It takes the current directory name as the first argument, and the list
        # of java source files as the remaining arguments.
        # It outputs a JSON structure representing the input sources, prepending
        # '../<current_dir>/' to each file path because the generator expects
        # paths relative to the build directory.
        jni_registration_helper_script = """
import json
import sys
d = {"java_files": [f"../{sys.argv[1]}/{f}" for f in sys.argv[2:]]}
"""
        if module_name:
            jni_registration_helper_script += f'd["module_name"] = "{module_name}"\n'
        jni_registration_helper_script += "print(json.dumps([d]))"

        # Convert the multi-line script into a single semicolon-separated line.
        # This is necessary because Soong's `cmd` field format doesn't support
        # literal newlines inside string literals without breaking.
        python_script = '; '.join(
            line.strip()
            for line in jni_registration_helper_script.strip().split('\n')
            if line.strip())

        base_cmd = ([
            "current_dir=`basename \\`pwd\\``;",
            f"python3 -c '{python_script}' $$current_dir $(in) > $(genDir)/java_sources.json;",
            f"python3 {base_cmd[0]}"
        ] + base_cmd[1:])

        return self.get_pre_cmd() + base_cmd

    def get_tool_files(self):
        tool_files = super().get_tool_files()
        # TODO: Remove once https://chromium-review.googlesource.com/c/chromium/src/+/5370266 has made
        #       its way to AOSP
        # Files not specified in anywhere but jni_generator.py imports this file
        tool_files.add('third_party/jni_zero/codegen/header_common.py')
        tool_files.add('third_party/jni_zero/codegen/placeholder_java_type.py')
        return tool_files


class JavaJniRegistrationGeneratorSanitizer(JniRegistrationGeneratorSanitizer):

    def get_name(self):
        name = super().get_name() + "__java"
        return name

    def _should_exclude_output(self, out):
        return not out.endswith(".srcjar")

    def get_deps(self):
        return {}


class VersionSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.set_flag_value('-o', '$(out)')
        # args for the version.py contain file path without leading --arg key. So apply sanitize
        # function for all the args.
        self.args.update_all_args(self._sanitize_filepath_with_location_tag)
        super()._sanitize_args()

    def get_tool_files(self):
        tool_files = super().get_tool_files()
        # android_chrome_version.py is not specified in anywhere but version.py imports this file
        tool_files.add('build/util/android_chrome_version.py')
        return tool_files


class JavaCppEnumSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.update_all_args(self._sanitize_filepath_with_location_tag)
        self.args.set_flag_value('--srcjar', '$(out)')
        super()._sanitize_args()


class MakeDafsaSanitizer(BaseActionSanitizer):

    def is_header_generated(self):
        # This script generates .cc files but they are #included by other sources
        # (e.g. registry_controlled_domain.cc)
        return True

    def _sanitize_args(self):
        self.args.update_all_args(self._sanitize_filepath_with_location_tag)
        self.args.update_all_args(self._sanitize_filepath)
        super()._sanitize_args()


class JavaCppFeatureSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.update_all_args(self._sanitize_filepath_with_location_tag)
        self.args.set_flag_value('--srcjar', '$(out)')
        super()._sanitize_args()


class JavaCppStringSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.update_all_args(self._sanitize_filepath_with_location_tag)
        self.args.set_flag_value('--srcjar', '$(out)')
        super()._sanitize_args()


class WriteNativeLibrariesJavaSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self.args.set_flag_value('--output', '$(out)')
        super()._sanitize_args()


class CopyActionSanitizer(BaseActionSanitizer):

    def get_tool_files(self):
        # CopyAction makes use of no tools, it simply relies on cp.
        return set()

    def get_cmd(self):
        return (super().get_pre_cmd() + ['cp'] +
                [shlex.quote(arg) for arg in self.args.get_args()])

    def get_srcs(self):
        srcs = super().get_srcs()
        if srcs:
            raise Exception(
                f'CopyAction {self.target.name} specifies {srcs=}. Only deps are supported'
            )
        deps = self.get_deps()
        if len(deps) > 1:
            raise Exception(
                f'CopyAction {self.target.name} specifies multiple {deps=}. Only a single dep is supported'
            )
        return set(f':{soong_ast.label_to_module_name(dep, self.context)}'
                   for dep in deps)

    def sanitize(self):
        # By convention, copy targets use their deps as args for the copy (see get_srcs).
        if len(self.args.get_args()) > 1:
            raise Exception(
                f'CopyAction {self.target.name} specifies {self.args.get_args()=}. Only deps are supported'
            )
        self.args.set_args([f'$(location {src})' for src in self.get_srcs()])
        self.args.append_arg('$(out)')
        super().sanitize()


class ProtocJavaSanitizer(BaseActionSanitizer):

    def __init__(self, target, arch, gn, context):
        super().__init__(target, arch, context)
        self._protoc = soong_ast.get_protoc_module_name(gn, context)

    def _sanitize_proto_path(self, arg):
        arg = self._sanitize_filepath(arg)
        return self.context.tree_path + '/' + arg

    def _sanitize_args(self):
        super()._sanitize_args()
        self.args.remove_flag('--depfile')
        self.args.set_flag_value('--protoc', '$(location %s)' % self._protoc)
        self.args.update_flag_value('--proto-path', self._sanitize_proto_path)
        self.args.set_flag_value('--srcjar', '$(out)')
        args_list = self.args.get_args()
        for i, arg in enumerate(args_list):
            if arg == '--import-dir':
                self.args.set_arg_at(
                    i + 1,
                    f"{self.context.tree_path}/{args_list[i+1].removeprefix('../../')}"
                )
            elif arg.startswith('../../') and arg.removeprefix(
                    '../../') in self.get_srcs():
                self.args.set_arg_at(
                    i, self._sanitize_filepath_with_location_tag(arg))

    def _sanitize_inputs(self):
        super()._sanitize_inputs()
        # https://crrev.com/c/5840231 adds
        #   //third_party/android_build_tools/protoc/cipd/protoc
        # to the input list. We don't import that protoc prebuilt binary; instead we
        # build protoc from source from //third_party/protobuf:protoc. We don't
        # need to add that as an input because it's already a tool dependency in
        # the generated module.
        self.target.common.inputs.discard(
            "//third_party/android_build_tools/protoc/cipd/protoc")

    def get_tools(self):
        tools = super().get_tools()
        tools.add(self._protoc)
        return tools


def get_action_sanitizer(gn,
                         target,
                         gn_type,
                         arch,
                         is_test_target,
                         context,
                         mode=None):
    if target.script == "//build/write_buildflag_header.py" or target.script == "//base/allocator/partition_allocator/src/partition_alloc/write_buildflag_header.py":
        # PartitionAlloc has forked the same write_buildflag_header.py script from
        # Chromium to break its dependency on //build.
        return WriteBuildFlagHeaderSanitizer(target, arch, context)
    if target.script == "//third_party/perfetto/gn/write_buildflag_header.py":
        return PerfettoWriteBuildFlagHeaderSanitizer(target, arch, context)
    if target.script == "//base/write_build_date_header.py":
        return WriteBuildDateHeaderSanitizer(target, arch, context)
    if target.script == "//tools/i18n/generate_canonical_locales_list.py":
        return GenerateCanonicalLocalesListSanitizer(target, arch, context)
    if target.script == "//tools/i18n/generate_known_bcp47_subtags.py":
        return GenerateKnownBcp47SubtagsSanitizer(target, arch, context)
    if target.script == "//tools/metrics/histograms/generate_allowlist_from_histograms_file.py":
        return WriteGenerateAllowlistFromHistogramsFileSanitizer(
            target, arch, context)
    if target.script == "//build/util/version.py":
        return VersionSanitizer(target, arch, context)
    if target.script == "//build/android/gyp/java_cpp_enum.py":
        return JavaCppEnumSanitizer(target, arch, context)
    if target.script == "//net/tools/dafsa/make_dafsa.py":
        return MakeDafsaSanitizer(target, arch, context)
    if target.script == '//build/android/gyp/java_cpp_features.py':
        return JavaCppFeatureSanitizer(target, arch, context)
    if target.script == '//build/android/gyp/java_cpp_strings.py':
        return JavaCppStringSanitizer(target, arch, context)
    if target.script == '//build/android/gyp/write_native_libraries_java.py':
        return WriteNativeLibrariesJavaSanitizer(target, arch, context)
    if target.script == '//cp':
        return CopyActionSanitizer(target, arch, context)
    if target.script == '//build/gn_run_binary.py':
        return GnRunBinarySanitizer(target, arch, context)
    if target.script == '//build/protoc_java.py':
        return ProtocJavaSanitizer(target, arch, gn, context)
    if jni_zero_target_type := soong_ast.get_jni_zero_target_type(target):
        if jni_zero_target_type == soong_ast.JniZeroTargetType.REGISTRATION_GENERATOR:
            if gn_type == 'java_genrule':
                # Fill up the sources of the target for JniRegistrationGenerator
                # actions with all the java sources found under targets of type
                # `generate_jni`. Note 1: Only do this for the java part in order to
                # generate a complete GEN_JNI. The C++ part MUST only include java
                # source files that are listed explicitly in `generate_jni` targets
                # in the transitive dependency, this is handled inside the action
                # sanitizer itself (See `get_srcs`). Adding java sources that are not
                # listed to the C++ version of JniRegistrationGenerator will result
                # in undefined symbols as the C++ part generates declarations that
                # would have no definitions. Note 2: This is only done for the
                # testing targets because their JniRegistration is not complete,
                # Chromium generates Jni files for testing targets implicitly (See
                # https://source.chromium.org/chromium/chromium/src/+/main:testing
                # /test.gni;l=422;bpv=1;bpt=0;drc
                # =02820c1b362c3a00f426d7c4eab18703d89cda03) to avoid having to
                # replicate the same setup, just fill up the java JniRegistration
                # with all  java sources found under `generate_jni` targets and fill
                # the C++ version with the exact files.
                if is_test_target:
                    target.common.sources.update(gn.jni_java_sources)
                return JavaJniRegistrationGeneratorSanitizer(
                    target, arch, is_test_target, context)
            return JniRegistrationGeneratorSanitizer(target,
                                                     arch,
                                                     is_test_target,
                                                     context,
                                                     mode=mode)
        if gn_type == 'cc_genrule':
            return JniGeneratorSanitizer(target, arch, is_test_target, context)
        return JavaJniGeneratorSanitizer(target, arch, is_test_target, context)
    raise Exception('Unsupported action %s from %s' %
                    (target.script, target.name))
