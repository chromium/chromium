# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import copy
import shlex
import hashlib
from pathlib import Path
from typing import List, Dict, Set, Union, Iterable

import gn_utils
import targets as gn2bp_targets
import build.gn_helpers
import components.cronet.tools.utils as cronet_utils
import soong_ast
import common
import action_sanitizers
import context as translation_context
import logging as log
import translation_config
from components.cronet.gn2bp.arguments import CommandLineUtility

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

# Shared libraries which are directly translated to Android system equivalents.
shared_library_allowlist = [
    'android',
    'log',
]
_FEATURE_REGEX = "feature=\\\"(.+)\\\""
_RUST_FLAGS_TO_REMOVE = [
    "--target",  # Added by Soong
    "--color",  # Added by Soong.
    "--edition",  # Added to the appropriate field, must be removed from flags.
    "--sysroot",  # Use AOSP's stdlib so we don't need any hacks for sysroot.
    "-Cembed-bitcode=no",  # Not compatible with Thin-LTO which is added by Soong.
    "-Clinker-plugin-lto",  # Not compatible with AOSP due to Clang and Rust version difference.
    "--cfg",  # Added to the appropriate field.
    "--extern",  # Soong automatically adds that for us when we use proc_macro
    "@",  # Used by build_script outputs to have rustc load flags from a file.
    "-Z",  # Those are unstable features, completely remove those.
]


def _parse_pydeps(repo_path):
    '''
  Parses a .pydeps file, returning a list of paths relative to REPOSITORY_ROOT.
  '''
    repo_dir_path = os.path.dirname(repo_path)
    with open(f"{REPOSITORY_ROOT}/{repo_path}", encoding='utf-8') as file:
        return [
            os.path.normpath(f"{repo_dir_path}/{line.strip()}")
            for line in file if not line.startswith('#')
        ]


def create_rust_cxx_modules(blueprint, gn, target, is_test_target, context):
    """Generate genrules for a CXX GN target

    GN actions are used to dynamically generate files during the build. The
    Soong equivalent is a genrule. Currently, Chromium GN targets generates
    both .cc and .h files in the same target, we have to split this up to be
    compatible with Soong.

    CXX bridge binary is used from AOSP instead of compiling Chromium's CXX bridge.

    Args:
        blueprint: soong_ast.Blueprint instance which is being generated.
        target: gn_utils.Target object.

    Returns:
        The source and headers genrule modules.
  """

    def _find_cxx_bridge_binary(deps: Set[str]) -> str:
        for dep in deps:
            if re.search(
                    r"^//third_party/rust/cxxbridge_cmd/v.*:cxxbridge__toolchain.*(__testing)?$",
                    dep):
                return dep
        raise Exception(
            f"Failed to find a dependency on cxxbridge host binary! Target name: {target.name}, deps: {deps}",
        )

    cxx_bridge_module_name = create_modules_from_target(
        blueprint, gn, _find_cxx_bridge_binary(target.common.deps),
        target.type, is_test_target, context)[0].name
    modules = []
    for (i, src) in enumerate(sorted(target.common.sources)):
        header_genrule = soong_ast.create_module(
            "cc_genrule",
            f"{soong_ast.label_to_module_name(target.name, context)}_header_{i}",
            target.name,
            context,
            is_test=is_test_target)
        header_genrule.tools = {cxx_bridge_module_name}
        header_genrule.cmd = f"$(location {cxx_bridge_module_name}) $(in) --header > $(out)"
        header_genrule.srcs = {gn_utils.label_to_path(src)}
        header_genrule.out = {f"{gn_utils.label_to_path(src)}.h"}

        cc_genrule = soong_ast.create_module(
            "cc_genrule",
            f"{soong_ast.label_to_module_name(target.name, context)}_{i}",
            target.name,
            context,
            is_test=is_test_target)
        cc_genrule.tools = {cxx_bridge_module_name}
        cc_genrule.cmd = f"$(location {cxx_bridge_module_name}) $(in) > $(out)"
        cc_genrule.srcs = {gn_utils.label_to_path(src)}
        cc_genrule.genrule_srcs = {f":{cc_genrule.name}"}
        cc_genrule.out = {f"{gn_utils.label_to_path(src)}.cc"}

        cc_genrule.genrule_headers.add(header_genrule.name)
        modules.extend([cc_genrule, header_genrule])
    return modules


def create_proto_modules(blueprint, gn, target, is_test_target, context):
    """Generate genrules for a proto GN target.

    GN actions are used to dynamically generate files during the build. The
    Soong equivalent is a genrule. This function turns a specific kind of
    genrule which turns .proto files into source and header files into a pair
    equivalent genrules.

    Args:
        blueprint: soong_ast.Blueprint instance which is being generated.
        target: gn_utils.Target object.

    Returns:
        The .h and .cc genrule modules.
    """
    assert (target.type == 'proto_library')

    if any(output.endswith('.descriptor') for output in target.common.outputs):
        # One example of a proto descriptor generator target is:
        #   //base/tracing/protos:chrome_track_event_gen
        # These targets require special logic since they generate a descriptor file
        # instead of C++ code. But it looks like Cronet works just fine without
        # them, so let's just ignore them to avoid the unnecessary complexity.
        return ()

    # TODO: proto modules being treated as "special snowflakes" instead of just
    # like any other action is doing more harm than good - it's weirdly
    # inconsistent and we end up missing out on concepts like cross-arch merging
    # and the action sanitizer arg handling framework. We should rewrite this
    # proto logic to be similar to how we handle any other GN action.

    # Retrieves the value of one of the command line arguments on the GN action,
    # or None if not found. The value is filtered through `sanitize()` if
    # provided. This function asserts that the sanitized value is the same across
    # all archs.
    def get_value_arg(arg_name, sanitize=None):
        arch_values = set()
        for arch_name, arch in target.arch.items():
            args = arch.args
            if not args:
                continue
            util = CommandLineUtility(args)
            if not util.has_arg(arg_name):
                arch_values.add(None)
                continue
            try:
                arch_value = util.get_flag_value(arg_name)
            except AssertionError as e:
                raise AssertionError(
                    f"Error getting {arg_name} for {target.name} ({arch_name}): {e}"
                ) from e
            if sanitize is not None:
                arch_value = sanitize(arch_value)
            arch_values.add(arch_value)
        assert len(arch_values) == 1, (target.name, arg_name, arch_values)
        (single_value, ) = arch_values
        return single_value

    protoc_module_name = soong_ast.get_protoc_module_name(
        gn, context) + (gn_utils.TESTING_SUFFIX if is_test_target else '')
    # Bring in any executable binary dependencies. Typically these would be protoc
    # plugins (more on plugins below).
    tools = {protoc_module_name} | {
        dep_module.name
        for dep_modules in (create_modules_from_target(
            blueprint, gn, dep, target.type, is_test_target, context)
                            for dep in target.common.deps)
        for dep_module in dep_modules if dep_module.type.endswith('_binary')
    }
    plugin = get_value_arg("--plugin")
    cpp_out_dir = get_value_arg(
        '--cc-out-dir' if plugin is None else '--plugin-out-dir',
        # Depending on the arch, sometimes the out dir starts with "gen/", sometimes
        # it starts with "clang_x64/gen/". We need to remove that prefix.
        sanitize=lambda value: re.sub('^([^/]+/)?gen/', '', value))
    assert cpp_out_dir is not None, target.name
    absolute_cpp_out_dir = f'$(genDir)/{cpp_out_dir}/'
    # We need to keep these module names short because the modules end up in
    # `generated_headers` which propagate throughout the build graph. If the names
    # are too long we can easily end up with long lists of generated headers with
    # long names, which in turn trigger "argument list too long" errors due to the
    # sheer size of `-I` include dir parameter lists being passed to the C++
    # compiler.
    target_module_name = soong_ast.label_to_module_name(target.name,
                                                        context,
                                                        short=True)

    # In GN builds the proto path is always relative to the output directory
    # (out/tmp.xxx).
    cmd = ['$(location %s)' % protoc_module_name]
    cmd += ['--proto_path=%s/%s' % (context.tree_path, target.proto_in_dir)]

    sorted_proto_paths = sorted(target.proto_paths)
    for proto_path in sorted_proto_paths:
        cmd += [f'--proto_path={context.tree_path}/{proto_path}']
    if translation_config.buildtools_protobuf_src in sorted_proto_paths:
        cmd += ['--proto_path=%s' % translation_config.android_protobuf_src]

    sources = {gn_utils.label_to_path(src) for src in target.common.sources}
    absolute_sources = sorted(
        [f"external/cronet/{context.import_channel}/{src}" for src in sources])

    # We create two genrules for each proto target: one for the headers and
    # another for the sources. This is because the module that depends on the
    # generated files needs to declare two different types of dependencies --
    # source files in 'srcs' and headers in 'generated_headers' -- and it's not
    # valid to generate .h files from a source dependency and vice versa.
    source_module_name = target_module_name
    source_module = soong_ast.create_module('cc_genrule',
                                            source_module_name,
                                            target.name,
                                            context,
                                            is_test=is_test_target)
    blueprint.add_module(source_module)
    source_module.srcs.update(sources)

    header_module = soong_ast.create_module('cc_genrule',
                                            source_module_name + '_h',
                                            target.name,
                                            context,
                                            is_test=is_test_target,
                                            role='h')
    blueprint.add_module(header_module)
    header_module.srcs = set(source_module.srcs)

    header_module.export_include_dirs = {'.', 'protos'}
    # Since the .cc file and .h get created by a different gerule target, they
    # are not put in the same intermediate path, so local includes do not work
    # without explictily exporting the include dir.
    header_module.export_include_dirs.add(cpp_out_dir)

    # This function does not return header_module so setting apex_available attribute here.
    header_module.apex_available.add(common.tethering_apex)

    source_module.genrule_srcs.add(':' + source_module.name)
    source_module.genrule_headers.add(header_module.name)

    cmd += [f'--cpp_out=lite=true:{absolute_cpp_out_dir}']

    cmd += absolute_sources

    # protoc supports "plugins", which are executable binaries it can call into
    # to customize code generation. In Chromium this feature is seldom used, but
    # there is one notable exception: Perfetto, which uses custom plugins all
    # over the place ("protozero", etc.).
    #
    # Another thing to keep in mind is the form of the plugin command line
    # options is a bit different between protoc and
    # //tools/protoc_wrapper/protoc_wrapper.py (which is what the GN action
    # calls), which is why we have to rearrange the args somewhat.
    # TODO: one could argue that it may be more robust to have the genrule call
    # protoc_wrapper.py instead of bypassing it and calling protoc directly.
    if plugin is not None:
        # The path to the plugin executable is quite different in AOSP vs. Chromium.
        # In AOSP, we assume the plugin is the only tool dependency (besides protoc
        # itself) and deduce the path from there.
        plugin_modules = tools - {protoc_module_name}
        assert len(plugin_modules) == 1, target.name
        (plugin_module, ) = plugin_modules
        cmd += [f"--plugin=protoc-gen-plugin=$(location {plugin_module})"]
    plugin_options = get_value_arg("--plugin-options")
    if plugin_options is not None:
        cmd += [f"--plugin_out={plugin_options}:{absolute_cpp_out_dir}"]

    source_module.cmd = cmd
    header_module.cmd = source_module.cmd
    source_module.tools = tools
    header_module.tools = tools

    source_module.out.update(output for output in target.common.outputs
                             if output.endswith('.cc'))
    header_module.out.update(output for output in target.common.outputs
                             if output.endswith('.h'))

    # This has proto files that will be used for reference resolution
    # but not compiled into cpp files. These additional sources has no output.
    proto_data_sources = sorted([
        gn_utils.label_to_path(proto_src) for proto_src in target.common.inputs
        if proto_src.endswith(".proto")
    ])
    source_module.srcs.update(proto_data_sources)
    header_module.srcs.update(proto_data_sources)

    # Allow rebasing proto genrules according to their proper path.
    source_module.allow_rebasing = True
    header_module.allow_rebasing = True
    header_module.build_file_path = target.build_file_path
    source_module.build_file_path = target.build_file_path
    return (header_module, source_module)


def create_gcc_preprocess_modules(blueprint, target, is_test_target, context):
    # gcc_preprocess.py internally execute host gcc which is not allowed in genrule.
    # So, this function create multiple modules and realize equivalent processing
    assert (len(target.common.sources) == 1)
    source = list(target.common.sources)[0]
    assert (Path(source).suffix == '.template')
    stem = Path(source).stem

    bp_module_name = soong_ast.label_to_module_name(target.name, context)

    # Rename .template to .cc since cc_preprocess_no_configuration does
    # not accept .template file as srcs
    rename_module = soong_ast.create_module('genrule',
                                            bp_module_name + '_rename',
                                            target.name,
                                            context,
                                            is_test=is_test_target)
    rename_module.srcs.add(gn_utils.label_to_path(source))
    rename_module.out.add(stem + '.cc')
    rename_module.cmd = 'cp $(in) $(out)'
    blueprint.add_module(rename_module)

    # Preprocess template file and generates java file
    preprocess_module = soong_ast.create_module(
        'cc_preprocess_no_configuration',
        bp_module_name + '_preprocess',
        target.name,
        context,
        is_test=is_test_target)
    # -E: stop after preprocessing.
    # -P: disable line markers, i.e. '#line 309'
    preprocess_module.cflags.extend(['-E', '-P', '-DANDROID'])
    preprocess_module.srcs.add(':' + rename_module.name)
    defines = [
        '-D' + target.common.args[i + 1]
        for i, arg in enumerate(target.common.args) if arg == '--define'
    ]
    preprocess_module.cflags.extend(defines)
    blueprint.add_module(preprocess_module)

    # Generates srcjar using soong_zip
    module = soong_ast.create_module('genrule',
                                     bp_module_name,
                                     target.name,
                                     context,
                                     is_test=is_test_target)
    module.srcs.add(':' + preprocess_module.name)
    module.out.add(stem + '.srcjar')
    module.cmd = [
        f'cp $(in) $(genDir)/{stem}.java &&',
        f'$(location soong_zip) -o $(out) -srcjar -C $(genDir) -f $(genDir)/{stem}.java'
    ]
    module.tools.add('soong_zip')
    blueprint.add_module(module)
    return module


def create_action_foreach_modules(blueprint, gn, target, is_test_target,
                                  context):
    """ The following assumes that rebase_path exists in the args.
  The args of an action_foreach contains hints about which output files are generated
  by which source files.
  This is copied directly from the args
  "gen/net/base/registry_controlled_domains/{{source_name_part}}-reversed-inc.cc"
  So each source file will generate an output whose name is the {source_name-reversed-inc.cc}
  """

    # We create one genrule per individual source, with numbered names (e.g.
    # "foo_0", "foo_1", etc.).
    # Note: currently we return the collection of the resulting genrules, instead
    # of a single module. Arguably this is a bit cumbersome. We could centralize
    # the outputs into a single "cp everything" genrule so that dependent modules
    # only have to depend on a single module.

    def create_subtarget(i, src):
        subtarget = copy.deepcopy(target)
        subtarget.name += f"_{i}"
        subtarget.common.sources = {src}
        new_args = []
        for arg in target.common.args:
            if '{{source}}' in arg:
                new_args.append('$(location %s)' %
                                (gn_utils.label_to_path(src)))
            elif '{{source_name_part}}' in arg:
                source_name_part = src.split("/")[-1]  # Get the file name only
                source_name_part = source_name_part.split(".")[
                    0]  # Remove the extension (Ex: .cc)
                file_name = arg.replace('{{source_name_part}}',
                                        source_name_part).split("/")[-1]
                # file_name represent the output file name. But we need the whole path
                # This can be found from target.outputs.
                for out in target.common.outputs:
                    if out.endswith(file_name):
                        new_args.append('$(location %s)' % out)
                        subtarget.common.outputs = {out}

                for file in (target.common.sources | target.common.inputs):
                    if file.endswith(file_name):
                        new_args.append('$(location %s)' %
                                        gn_utils.label_to_path(file))
            else:
                new_args.append(arg)
        subtarget.common.args = new_args
        return subtarget

    return [
        create_action_module(blueprint, gn, create_subtarget(i, src),
                             'cc_genrule', is_test_target, context)
        for i, src in enumerate(sorted(target.common.sources))
    ]


def create_action_module_internal(gn,
                                  target,
                                  gn_type,
                                  is_test_target,
                                  blueprint,
                                  context,
                                  arch=None,
                                  mode=None):
    if target.script == '//build/android/gyp/gcc_preprocess.py':
        return create_gcc_preprocess_modules(blueprint, target, is_test_target,
                                             context)
    sanitizer = action_sanitizers.get_action_sanitizer(gn,
                                                       target,
                                                       gn_type,
                                                       arch,
                                                       is_test_target,
                                                       context,
                                                       mode=mode)
    sanitizer.sanitize()

    module = soong_ast.create_module(gn_type,
                                     sanitizer.get_name(),
                                     target.name,
                                     context,
                                     is_test=is_test_target)
    module.cmd = sanitizer.get_cmd()
    module.out = sanitizer.get_outputs()
    if sanitizer.is_header_generated():
        module.genrule_headers.add(module.name)
    if gn_type == 'cc_genrule':
        for out in module.out:
            if out.endswith('.cc') or out.endswith('.cpp') or out.endswith(
                    '.c'):
                module.genrule_srcs.add(f":{module.name}")
                break
    module.srcs = sanitizer.get_srcs()
    module.tool_files = sanitizer.get_tool_files()
    module.tools = sanitizer.get_tools()
    target.common.deps = sanitizer.get_deps()

    return module


def get_cmd_condition(arch):
    '''
  :param arch: archtecture name e.g. android_x86_64, android_arm64
  :return: condition that can be used in cc_genrule cmd to switch the behavior based on arch
  '''
    if arch == "android_x86_64":
        return "( $$CC_ARCH == 'x86_64' && $$CC_OS == 'android' )"
    if arch == "android_x86":
        return "( $$CC_ARCH == 'x86' && $$CC_OS == 'android' )"
    if arch == "android_arm":
        return "( $$CC_ARCH == 'arm' && $$CC_OS == 'android' )"
    if arch == "android_arm64":
        return "( $$CC_ARCH == 'arm64' && $$CC_OS == 'android' )"
    if arch == "android_riscv64":
        return "( $$CC_ARCH == 'riscv64' && $$CC_OS == 'android' )"
    if arch == "host":
        return "$$CC_OS != 'android'"
    raise Exception(f'Unknown architecture type {arch}')


def merge_cmd(modules, genrule_type):
    '''
  :param modules: dictionary whose key is arch name and value is module
  :param genrule_type: cc_genrule or java_genrule
  :return: merged command or common command if all the archs have the same command.
  '''
    commands = list({"\n".join(module.cmd) for module in modules.values()})
    if len(commands) == 1:
        # If all the archs have the same command, return the command
        return list(modules.values())[0].cmd

    if genrule_type != 'cc_genrule':
        raise Exception(
            f'{genrule_type} can not have different cmd between archs')

    merged_cmd = []
    for arch, module in sorted(modules.items()):
        merged_cmd.append(f'if [[ {get_cmd_condition(arch)} ]];')
        merged_cmd.append('then')
        merged_cmd.extend(module.cmd)
        merged_cmd.append(';fi;')
    return merged_cmd


def merge_modules(modules, genrule_type):
    '''
  :param modules: dictionary whose key is arch name and value is module
  :param genrule_type: cc_genrule or java_genrule
  :return: merged module of input modules
  '''
    merged_module = list(modules.values())[0]

    # Following attributes must be the same between archs
    for key in ('genrule_headers', 'srcs', 'tool_files'):
        if any(
                getattr(merged_module, key) != getattr(module, key)
                for module in modules.values()):
            raise Exception(
                f'{merged_module.name} has different values for {key} between archs'
            )

    merged_module.cmd = merge_cmd(modules, genrule_type)
    return merged_module


def create_java_module(bp_module_name, target, blueprint, is_test_target,
                       context):

    def add_java_library_properties(module):
        module.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
        module.apex_available.add(common.tethering_apex)
        module.defaults.add(context.java_framework_defaults_module)
        module.build_file_path = target.build_file_path

    # As hinted in `parse_gn_desc()`, Java GN targets are... complicated.
    #
    # Here the main source of complexity is the need to support the
    # jar_excluded_patterns and jar_included_patterns options of Chromium's
    # `java_library()` GN rule. Most java_library targets don't really use this
    # feature, but there are notable exceptions: for example some jni_zero
    # generator targets rely on this to remove placeholder classes, which would
    # conflict with the new classes otherwise.
    #
    # Soong doesn't provide an equivalent to jar_excluded_patterns and
    # jar_included_patterns, so we need to implement that ourselves. To do so, we
    # introduce the concept of "filtering". Instead of generating just one module
    # for a given Java target, we generate 3:
    #
    # - A top-level `java_library` module, that doesn't build anything and only
    #   acts as a dependency holder;
    # - A `java_library` (or `java_import`) module with a `__unfiltered` suffix,
    #   that does the actual building;
    # - A `java_genrule` module with a `__filtered` prefix, that takes the output
    #   of the unfiltered module and applies the jar exclusion/inclusion rules.
    #
    # If you're wondering why the top-level module is needed (i.e. why can't we go
    # directly to the filter module), the reason is because otherwise there would
    # be no way to correctly set up parallel filtered vs. unfiltered dependency
    # trees. See the Java dependency handling logic in
    # create_modules_from_target() for details.
    #
    # For even more more background, see https://crbug.com/397396295.

    sources = target.common.sources
    source_is_jar = any(source.endswith('.jar') for source in sources)
    unfiltered_module = soong_ast.create_module(
        "java_import" if source_is_jar else "java_library",
        f"{bp_module_name}__unfiltered",
        target.name,
        context,
        is_test=is_test_target,
        role='unfiltered')
    add_java_library_properties(unfiltered_module)
    if source_is_jar:
        assert all(source.endswith('.jar') for source in sources), target.name
        unfiltered_module.jars = [
            gn_utils.label_to_path(source) for source in sources
        ]
    blueprint.add_module(unfiltered_module)

    # Potential optimization opportunity: we could skip the filtered module if
    # there are no jar exclusion/inclusion rules, and have the top module depend
    # on the unfiltered module directly. This would avoid a pointless call to
    # filter_zip.py with no rules. (But note that, even then, we would still need
    # a distinction between filtered and unfiltered modules: an unfiltered module
    # should not depend on *any* filtered module, even indirectly, so we need to
    # keep the dependency chains separate throughout the entire build tree no
    # matter what.)
    filtered_module = soong_ast.create_module("java_genrule",
                                              f"{bp_module_name}__filtered",
                                              target.name,
                                              context,
                                              is_test=is_test_target,
                                              role='filtered')
    filtered_module.srcs = [f":{unfiltered_module.name}"]

    jar_excluded_patterns = target.java_jar_excluded_patterns
    # HACK: don't strip the placeholder org.chromium.build.NativeLibraries from
    # //build/android:build_java, as we don't generate the real one.
    # TODO(https://crbug.com/405373567): generate a proper NativeLibraries
    # instead.
    if target.name in ("//build/android:build_java",
                       "//build/android:build_java__testing"):
        jar_excluded_patterns = [
            jar_excluded_pattern
            for jar_excluded_pattern in jar_excluded_patterns
            if jar_excluded_pattern != "*/NativeLibraries.class"
        ]

    def array_to_arg(array):
        return shlex.quote(
            # filter_zip.py array arguments expect "GN-string" syntax.
            build.gn_helpers.ToGNString(array)).replace('$', '$$')

    # Chromium conveniently provides a script, `filter_zip.py`, that we can use to
    # process the jar and apply the exclusion/inclusion rules.
    #
    # Note this is different to how Chromium does it. In Chromium the rules are
    # applied directly by `compile_java.py`. We can't do that here because we
    # don't use `compile_java.py` - instead we use Soong's `java_library` module
    # to compile Java code.
    #
    # That said, Chromium does use `filter_zip.py` to filter prebuilt jars, so
    # it's likely this script will keep working for the foreseeable future.
    FILTER_ZIP_PATH = "build/android/gyp/filter_zip.py"
    filtered_module.cmd = [
        f"$(location {FILTER_ZIP_PATH})",
        "--input",
        "'$(in)'",
        "--output",
        "'$(out)'",
        "--exclude-globs",
        array_to_arg(jar_excluded_patterns),
        "--include-globs",
        array_to_arg(target.java_jar_included_patterns),
    ]
    filtered_module.out = [f"{filtered_module.name}.jar"]
    # Normally we would get `tool_files` from the gn desc, but here we don't have
    # an action target to extract this from, so we compute it ourselves.
    filtered_module.tool_files = _parse_pydeps(f"{FILTER_ZIP_PATH}deps")
    filtered_module.visibility = {"//external/cronet:__subpackages__"}
    blueprint.add_module(filtered_module)

    top_module = soong_ast.create_module("java_library",
                                         bp_module_name,
                                         target.name,
                                         context,
                                         is_test=is_test_target)
    top_module.java_unfiltered_module = unfiltered_module
    add_java_library_properties(top_module)
    top_module.static_libs.add(filtered_module.name)
    return top_module


def get_bindgen_source_stem(outputs: List[str]) -> str:
    """Returns the appropriate source_stem for a bindgen module

  Args:
    outputs: The appropriate source stem to be used.

  Returns:
    source stem to be used for the bindgen module or raises
    ValueError if more than a single .rs file is found
  """
    rs_output = None
    for output in outputs:
        if output.endswith(".rs"):
            if rs_output:
                raise ValueError(
                    f"Expected a single rust file in the target output but found more than one! Outputs: {outputs}"
                )
            rs_output = output
    if not rs_output:
        raise ValueError(
            f"Expected a single rust file in the target output but found none! Outputs: {outputs}"
        )
    file_name = rs_output[:-3]
    if "/" in file_name:
        file_name = file_name.rsplit("/", 1)[1]
    return file_name


def get_bindgen_flags(args: List[str]) -> List[str]:
    """Gets the appropriate bindgen_flags from the GN target args

  Args:
    args: GN target args

  Raises:
    ValueError: If --bindgen-flags was found but no args followed it.

  Returns:
    Gets the appropriate bindgen_flags from the GN target args
  """
    util = CommandLineUtility(args)
    if not util.has_arg("--bindgen-flags"):
        return []

    bindgen_flags = []
    for arg in util.get_list_values("--bindgen-flags"):
        bindgen_flags.append("--" + arg)
    return bindgen_flags


def _create_extract_rust_files_target(bindgen_module, blueprint, context):
    module = soong_ast.create_module(
        "cc_genrule",
        bindgen_module.name + "__extract_rust_files",
        f"Extract rust files from {bindgen_module.name}",
        context,
        is_test=bindgen_module._is_test)
    module.srcs = [f":{bindgen_module.name}"]
    module.cmd = [
        f'for f in $(locations :{bindgen_module.name}); do',
        'if [[ "$$f" == *.rs ]]; then', 'cp "$$f" $(out);', 'fi;', 'done'
    ]
    module.out = [f"{bindgen_module.source_stem}.rs"]
    module.device_supported = bindgen_module.device_supported
    module.host_supported = bindgen_module.host_supported
    module.host_cross_supported = bindgen_module.host_cross_supported
    module.target['host'].compile_multilib = '64'
    module.apex_available.add(common.tethering_apex)
    blueprint.add_module(module)
    return module


def create_bindgen_module(
        blueprint: soong_ast.Blueprint, target, module_name: str,
        is_test_target: bool,
        context: translation_context.TranslationContext) -> soong_ast.Module:
    module = soong_ast.create_module("rust_bindgen",
                                     "lib" + module_name,
                                     target.name,
                                     context,
                                     is_test=is_test_target)
    if len(target.common.sources) > 1:
        raise ValueError(
            f"Expected a single source file for bindgen but found {target.common.sources}."
        )

    if len(target.common.outputs) > 2:
        raise ValueError(
            f"Expected at most two output files for bindgen but found {target.common.outputs}"
        )
    module.wrapper_src = gn_utils.label_to_path(list(target.common.sources)[0])
    module.crate_name = module_name

    if "c++" in target.common.args:
        # This is defined in the rust_bindgen templates where "C++" will
        # be added to the args if `cpp` field is defined. Soong depends
        # on `cpp_std` field to identify that this is a C++ header.
        module.cpp_std = common.CPP_VERSION

    module.source_stem = get_bindgen_source_stem(target.common.outputs)

    if "--wrap-static-fns" in target.common.args:
        module.handle_static_inline = True

    module.bindgen_flags = get_bindgen_flags(target.common.args)
    # This ensures that any CC file that is being processed through the
    # rust_bindgen module is able to #include files relative to the root of the
    # repository.
    #
    # Note: this module is not part of the generated build rules; it is expected
    # to already be present in AOSP (currently, in Android.extras.bp). See
    # https://r.android.com/3413202.
    module.header_libs = {
        f"{context.module_prefix}repository_root_include_dirs_anchor"
    }
    module.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
    module.apex_available.add(common.tethering_apex)
    blueprint.add_module(module)
    return module


def create_generated_headers_export_module(
        blueprint: soong_ast.Blueprint, cc_genrule_module: soong_ast.Module,
        context: translation_context.TranslationContext) -> soong_ast.Module:
    '''
  Creates a cc_library_headers module that merely re-exports headers that are
  generated by a cc_genrule module. This is useful in scenarios where a module
  has no way of directly depending on generated headers.
  '''
    cc_genrule_module_name = cc_genrule_module.name
    module = soong_ast.create_module(
        "cc_library_headers",
        f"{cc_genrule_module_name}_export_generated_headers",
        cc_genrule_module.gn_target,
        context,
        is_test=cc_genrule_module._is_test)
    module.export_generated_headers = module.generated_headers = [
        cc_genrule_module_name
    ]
    module.build_file_path = cc_genrule_module.build_file_path
    module.defaults.add(context.cc_defaults_module)
    module.host_supported = cc_genrule_module.host_supported
    module.host_cross_supported = cc_genrule_module.host_cross_supported
    module.device_supported = cc_genrule_module.device_supported
    blueprint.add_module(module)
    return module


def create_action_module(blueprint,
                         gn,
                         target,
                         genrule_type,
                         is_test_target,
                         context,
                         mode=None):
    '''
  Create module for action target and add to the blueprint. If target has arch specific attributes
  this function merge them and create a single module.
  :param blueprint:
  :param target: target which is converted to the module.
  :param genrule_type: cc_genrule or java_genrule
  :return: created module
  '''
    # TODO: Handle this target correctly, this target generates java_genrule but this target has
    # different value for cpu-family arg between archs
    if re.match('//build/android:native_libraries_gen(__testing)?$',
                target.name):
        module = create_action_module_internal(gn,
                                               target,
                                               genrule_type,
                                               is_test_target,
                                               blueprint,
                                               context,
                                               target.arch['android_arm'],
                                               mode=mode)
        blueprint.add_module(module)
        return module

    modules = {
        arch_name:
        create_action_module_internal(gn,
                                      target,
                                      genrule_type,
                                      is_test_target,
                                      blueprint,
                                      context,
                                      arch,
                                      mode=mode)
        for arch_name, arch in target.get_archs().items()
    }
    module = merge_modules(modules, genrule_type)
    blueprint.add_module(module)
    return module


def get_jni_zero_generator_proxy_and_placeholder_paths(module):
    assert module.jni_zero_target_type == soong_ast.JniZeroTargetType.GENERATOR

    def is_placeholder(path):
        return path.endswith('_placeholder.srcjar')

    placeholder_paths = [out for out in module.out if is_placeholder(out)]
    assert len(placeholder_paths) == 1, module.name
    proxy_paths = [
        out for out in module.out
        if out.endswith('.srcjar') and not is_placeholder(out)
    ]
    assert len(proxy_paths) == 1, module.name
    return proxy_paths[0], placeholder_paths[0]


def create_jni_zero_proxy_only_module(jni_zero_generator_module, context):
    '''
  Creates a module that filters the output of an existing jni_zero generator
  action module, outputting the proxy classes only, leaving out the placeholder
  classes.

  This is used to work around a Soong limitation where it's not possible to
  refer to specific files from the output of a genrule. Instead, we create an
  additional trivial genrule that merely copies a specific subset of the
  original output files. We can then depend on these genrules to pull the files
  we want.
  '''
    assert jni_zero_generator_module.jni_zero_target_type == soong_ast.JniZeroTargetType.GENERATOR
    proxy_path, _ = get_jni_zero_generator_proxy_and_placeholder_paths(
        jni_zero_generator_module)

    proxy_only_module = soong_ast.create_module(
        jni_zero_generator_module.type,
        f"{jni_zero_generator_module.name}_proxy_only",
        jni_zero_generator_module.gn_target,
        context,
        is_test=jni_zero_generator_module._is_test)
    proxy_only_module.cmd = "cp $(in) $(genDir)"
    proxy_only_module.srcs = [f":{jni_zero_generator_module.name}"]
    proxy_only_module.out = [os.path.basename(proxy_path)]

    return proxy_only_module


def _is_cflag_allowed(cflag):
    if cflag.startswith("-Wno-"):
        # Allow all -Wno- flags as those demote errors to warning.
        return True
    return all(not cflag.startswith(denied_prefix) for denied_prefix in [
        # Soong handles this according to the module's attributes.
        "--sysroot=",
        # Soong handles this according to the architecture.
        "--target=",
        "--warning-suppression-mappings=",
        # Remove all promotions of warning to errors. The code is developed in
        # chromium, and the checks should be there.
        "-W",
        # Soong handles this according to the module's attributes.
        "-isystem",
        # Best handled by Soong according to the build configuration.
        '-fcrash-diagnostics-dir=',
        # Enabled by default in Soong.
        '-flto',
        # Enabled by default in Soong.
        '-fsplit-lto-unit',
        # Enabled by a special attribute instead.
        '-fwhole-program-vtables',
        # LLVM in AOSP fails when this is added. It's mostly used to warn
        # against non-standard compiler extensions. It's forbidden by Soong as it's
        # in the list of the IllegalFlags: http://ac/build/soong/cc/config/global.go?l=405-413
        '-pedantic',
        # Same as above.
        '-w',
        # This is used by a clang-plugin to show errors / warning for unsafe buffers during
        # compilation. We don't care about static analysis errors / warnings as they're
        # shown on the Chromium side. The reason why we're excluding this flag is because
        # it introduces build breakages as clang's toolchain does not understand the
        # pragma enabled by this define.
        '-DUNSAFE_BUFFERS_BUILD',
        '-Wunsafe-buffer-usage',
        '-Wno-error=unsafe-buffer-usage',
        # Causes Soong to fail with:
        #   "Bad flag: `-gsplit-dwarf`, soong cannot track dependencies to split dwarf debuginfo"
        # See https://crbug.com/481594099
        '-gsplit-dwarf',
        # Causes the build to fail with:
        #   clang++-real: error: unknown argument: '-fsanitize-ignore-for-ubsan-feature=array-bounds'
        # See https://crbug.com/481594099
        '-fsanitize-ignore-for-ubsan-feature=array-bounds',
        # Causes the build to fail with:
        #   clang++-real: error: unknown argument: '-fsanitize-ignore-for-ubsan-feature=return'
        # See https://crbug.com/493168827
        '-fsanitize-ignore-for-ubsan-feature=',
        # Causes the build to fail with:
        #   clang++-real: error: unknown argument: '-fno-lifetime-dse'
        # See https://crbug.com/484919839
        '-fno-lifetime-dse',
        # C++ version is picked via a Soong attribute.
        '-std=',
        # Added automatically by specifying `afdo: true`
        '-fdebug-info-for-profiling',
        # Any cflag that starts with '-g' is solely for debugging information.
        # This is best left handled by Soong according to the build configuration.
        # See https://clang.llvm.org/docs/ClangCommandLineReference.html#debug-information-options
        '-g',
        # MLGO optimizations are handled automatically by Soong when AFDO is
        # applied.
        '-mllvm -enable-ml-inliner=',
        '-mllvm -ml-inliner-model-selector=',
        '-fdiagnostics-show-inlining-chain',
        # Soong sets -msse3 by default for the architectures where its relevant.
        '-msse3',
    ])


def _merge_key_value_cflags(cflags: List[str]) -> List[str]:
    cflags_merged = []
    iterator = iter(cflags)
    for flag in iterator:

        if flag == '-mllvm':
            # Merge the two consecutive flags together and skip the next iteration.
            cflags_merged.append(f'{flag} {next(iterator)}')
        else:
            cflags_merged.append(flag)
    return cflags_merged


def _get_cflags(cflags, defines):
    cflags = _merge_key_value_cflags(cflags)
    cflags = [flag for flag in cflags if _is_cflag_allowed(flag)]

    # Android _may_ set a platform default for _LIBCPP_HARDENING_MODE. If that
    # conflicts with the level specified on this target, we'll get build errors.
    #
    # Allow Android's default to apply to builds where we don't specify one, but
    # prefer our default for builds that do.
    libcpp_hardening_flag = "_LIBCPP_HARDENING_MODE"
    if any(define.startswith(libcpp_hardening_flag) for define in defines):
        cflags.append(f"-U{libcpp_hardening_flag}")

    # Consider proper allowlist or denylist if needed
    cflags.extend(sorted(["-D%s" % define for define in defines]))
    return cflags


def _get_cpp_std(cflags: List[str]) -> Union[str, None]:
    cpp_stds = [
        cflag.removeprefix('-std=') for cflag in cflags
        if cflag.startswith('-std=')
    ]
    if cpp_stds:
        # There can be multiple cpp std in cflags list. Return the last one as this will
        # override any previous version.
        return cpp_stds[-1]
    return None


def _extract_version_script(ldflags):
    new_ldflags = []
    version_script = None
    for flag in ldflags:
        if flag.startswith("-Wl,--version-script="):
            # Everything after the = is the path and delete all leading ../
            new_version_script = re.sub('^(\.\./)+', '',
                                        flag.split("=", maxsplit=2)[1])
            assert version_script is None, f"Found two different version scripts for a single target! First script: {version_script}, Second script: {new_version_script}"
            version_script = new_version_script
        else:
            new_ldflags.append(flag)
    return new_ldflags, version_script


def _create_linker_script_filegroup(linker_script_path, is_test_target,
                                    context):
    filegroup_name = linker_script_path.replace('/', '_').replace('.', '_')
    filegroup_module = soong_ast.create_module(
        "filegroup",
        f"{context.module_prefix}{filegroup_name}_filegroup",
        f"Created to reference {linker_script_path}",
        context,
        is_test=is_test_target)
    filegroup_module.srcs = [linker_script_path]
    # TODO(aymanm): Change the default for build_file_path to be top-level.
    filegroup_module.build_file_path = ""
    return filegroup_module


def _is_allowed_ldflag(flag):
    return all(not flag.startswith(denied_prefix) for denied_prefix in [
        # Already applied by Soong according to module's attributes.
        "--sysroot=",
        # Already applied by Soong.
        "--target=",
        # Throws an error for some unknown reason?
        "--unwindlib=",
        # Tries to write to disk which is disallowed by Soong. It also
        # simply controls the caching behaviour of thinLTO which is
        # not essential.
        "-Wl,--thinlto-cache-dir=",
        # Controls the caching behaviour of thinLTO which is
        # not essential.
        "-Wl,--thinlto-cache-policy=",
        # Controls the threading behaviour of thinLTO which is
        # not essential.
        "-Wl,--thinlto-jobs=",
        # Applied by Soong by default
        "-flto=",
        # Throws an error currently because GNU_PROPERTY_AARCH64_FEATURE_1_BTI is
        # not defined in some object files. This requires further investigation
        # to enable. It's fine to disable for now as it has never been enabled in
        # HttpEngine.
        "-Wl,-z,force-bti",
        # Soong handles this automatically based on the lunch options.
        "-Wl,-z,max-page-size=",
        # Let Soong handle the stripping of debug library according to the
        # lunch configuration.
        "-Wl,--strip-debug",
        # Android is experimenting with XOM(crbug.com/379071663) which conflicts with
        # rosegment flag. Disable this flag until XOM has landed, and we have
        # an attribute which we can use to enable --no-rosegment.
        "-Wl,--no-rosegment",
        # This is already the default in AOSP.
        "-fuse-ld",
        # Specified by a Soong attribute instead.
        "-fwhole-program-vtables",
        # All MLGO is left best-handled by Soong as we're now employing AFDO
        # profiles.
        "-Wl,-mllvm,-enable-ml-inliner",
        "-Wl,-mllvm,-ml-inliner-model-selector",
        "-Wl,-mllvm,-ml-inliner-skip-policy",
    ])


def configure_cc_module(cc_properties, cflags, defines, ldflags, libs,
                        blueprint, context):
    cc_properties.cflags = _get_cflags(cflags, defines)
    ldflags, _ = _extract_version_script(ldflags)
    cc_properties.ldflags = [
        flag for flag in ldflags if _is_allowed_ldflag(flag)
    ]

    main_module = cc_properties if isinstance(
        cc_properties, soong_ast.Module) else cc_properties._parent
    is_test = main_module._is_test

    for lib in libs:
        if lib.endswith('.lds'):
            linker_script = gn_utils.label_to_path(lib)
            filegroup_module = _create_linker_script_filegroup(
                linker_script, is_test, context)
            blueprint.add_module(filegroup_module)
            linker_script_deps = f':{filegroup_module.name}'
            cc_properties.linker_scripts.add(linker_script_deps)
        else:
            # Generally library names should be mangled as 'libXXX', unless they
            # are HAL libraries (e.g., android.hardware.health@2.0) or AIDL c++ / NDK
            # libraries (e.g. "android.hardware.power.stats-V1-cpp")
            android_lib = lib if '@' in lib or "-cpp" in lib or "-ndk" in lib \
                else 'lib' + lib
            if lib in shared_library_allowlist:
                cc_properties.shared_libs.add(android_lib)
    # TODO: implement proper cflag parsing.
    for flag in cflags:
        if '-fexceptions' in flag:
            cc_properties.cppflags.append('-fexceptions')


def _create_rust_build_script_output_copy_genrule(module_name,
                                                  path_to_directory, files,
                                                  is_test_target, context):
    module = soong_ast.create_module(
        "genrule",
        module_name,
        "Copies generated Rust build script files somewhere the dependent code can find them",
        context,
        is_test=is_test_target)
    module.srcs = [f"{path_to_directory}/{file_name}" for file_name in files]
    module.cmd = "cp $(in) $(genDir)"
    module.out = files
    return module


def set_module_include_dirs(cc_properties, cflags, include_dirs, context):
    for flag in cflags:
        if '-isystem' in flag:
            cc_properties.include_dirs.add(
                f"external/cronet/{context.import_channel}/{flag[len('-isystem../../'):]}"
            )

    depends_on_binder_ndk = any("libbinder_ndk_cpp" in include_dir
                                for include_dir in include_dirs)
    if depends_on_binder_ndk:
        cc_properties.shared_libs.add("libbinder_ndk")
        include_dirs = [
            include_dir for include_dir in include_dirs
            if "libbinder_ndk_cpp" not in include_dir
        ]
    # Adding include_dirs is necessary due to source_sets / filegroups
    # which do not properly propagate include directories.
    # Filter any directory inside //out as a) this directory does not exist for
    # aosp / soong builds and b) the include directory should already be
    # configured via library dependency.
    # Note: include_dirs is used instead of local_include_dirs as an Android.bp
    # can't access other directories outside of its current directory. This
    # is worked around by using include_dirs.
    cc_properties.include_dirs.update([
        f"external/cronet/{context.import_channel}/{gn_utils.label_to_path(d)}"
        for d in include_dirs if not d.startswith('//out')
    ])
    # Remove prohibited include directories
    cc_properties.include_dirs = [
        d for d in cc_properties.include_dirs
        if d not in context.include_dirs_denylist
    ]

    # If we end up including Cronet's root, then also include the Android-side
    # unversioned include override directory, with higher precedence.
    if f"external/cronet/{context.import_channel}/" in cc_properties.include_dirs:
        cc_properties.include_dirs.insert(0, "external/cronet/include/")


def create_aidl_module(bp_module_name, target, blueprint, is_test_target,
                       context):
    module = soong_ast.create_module("aidl_interface",
                                     bp_module_name,
                                     target.name,
                                     context,
                                     is_test=is_test_target)
    module.unstable = True
    module.include_dirs = [
        f"external/cronet/{context.import_channel}/{path}"
        for path in sorted(target.aidl_includes)
    ]
    # This is necessary as Soong adds a dependency behind the scenes ;(
    # https://cs.android.com/android/platform/superproject/main/+/main:system/tools/aidl/build/aidl_interface_backends.go;l=162
    module.visibility.add("//system/tools/aidl/build")
    filegroup_module_name = f"{bp_module_name}_filegroup"
    module.srcs = {f":{filegroup_module_name}"}
    # Filegroup exists here because Soong's genrule for AIDL contains a bug where there's
    # a discrepancy between the expected generated file path and the actual path.
    # See crbug.com/418726870 for more information.
    filegroup_module = soong_ast.create_module("filegroup",
                                               filegroup_module_name,
                                               target.name,
                                               context,
                                               is_test=is_test_target)
    filegroup_module.srcs = [
        gn_utils.label_to_path(src) for src in sorted(target.common.sources)
    ]
    filegroup_module.build_file_path = target.build_file_path
    # The following lines will trim an absolute path to the path
    # of the java package. There's an assumption here that AIDL files
    # live in java-kind packages.
    # e.g. A/B/C/src/package/path/path.aidl -> A/B/C
    source_file_path = list(filegroup_module.srcs)[0]
    path_to_package = source_file_path[:source_file_path.find("src/") +
                                       len("src/")]
    assert all(
        src.startswith(path_to_package) for src in filegroup_module.srcs
    ), f"AIDL module {target.name} has sources from different packages which is not supported."
    filegroup_module.path = path_to_package
    blueprint.add_module(filegroup_module)
    return (module, )


# Declares internal modules that are processed by GN but excluded from the final Android.bp.
# While GN traverses the graph, the Soong crawler does not visit these dependencies,
# which prevents them from being written to the output file.
#
# When referenced as a dependency, a custom handler is invoked (e.g., via `translation_config.replace_deps`)
# instead of the usual code flow.
#
# The benefit of using |translation_config.replace_deps| over |translation_config.builtin_deps| is that some of the module's
# attributes can be reused / copied as the module is created in memory. unlike |translation_config.builtin_deps|
# which never visits the target itself.


def get_target_name(label):
    return label[label.find(":") + 1:]


def normalize_rust_flags(
        rust_flags: List[str]) -> Dict[str, Union[Set[str], None]]:
    """
  Normalizes the rust params where it tries to put (key, value) param
  as a dictionary key. A key without value will have None as value.

  An example of this would be:

  Input: ["--cfg=feature=\"float_roundtrip\"", "--cfg=feature=\"std\"",
          "--edition=2021", "-Cforce-unwind-tables=no", "-Dwarnings"]

  Output: {
          "--cfg": [feature=\"float_roundtrip\", feature=\"std\"],
          "--edition": [2021],
          "-Cforce-unwind-tables": [no],
          "-Dwarnings": None
          }
  :param rust_flags: List of rust flags.
  :return: Dictionary of rust flags where each key will point to a list of
  values.
  """
    args_mapping = {}
    previous_key = None
    for rust_flag in rust_flags:
        if not rust_flag:
            # Ignore empty strings
            continue

        if not rust_flag.startswith("-"):
            # This might be a key on its own, rustc supports params with no keys
            # such as (@path).
            if rust_flag.startswith("@"):
                args_mapping[rust_flag] = None
                if previous_key:
                    args_mapping[previous_key] = None
            else:
                # This is the value to the previous key (eg: ["--cfg", "val"])
                if not previous_key:
                    raise ValueError(
                        f"Field {rust_flag} does not relate to any key. Rust flags found: {rust_flags}"
                    )
                if previous_key not in args_mapping:
                    args_mapping[previous_key] = set()
                args_mapping[previous_key].add(rust_flag)
                previous_key = None
        else:
            if previous_key:
                # We have a previous key, that means that the previous key is
                # a no-value key.
                args_mapping[previous_key] = None
                previous_key = None
            # This can be a key-only string or key=value or
            # key=foo=value (eg:--cfg=feature=X) or key and value in different strings.
            if "=" in rust_flag:
                # We found an equal, this is probably a key=value string.
                rust_flag_split = rust_flag.split("=")
                if len(rust_flag_split) > 3:
                    raise ValueError(
                        f"Could not normalize flag {rust_flag} as it has multiple equal signs."
                    )
                if rust_flag_split[0] not in args_mapping:
                    args_mapping[rust_flag_split[0]] = set()
                args_mapping[rust_flag_split[0]].add("=".join(
                    rust_flag_split[1:]))
            else:
                # Assume this is a key-only string. This will be resolved in the next
                # iteration.
                previous_key = rust_flag
    if previous_key:
        # We have a previous key without a value, this must be a key-only string.
        args_mapping[previous_key] = None
    return args_mapping


def _set_rust_flags(module: soong_ast.Target, rust_flags: List[str],
                    arch_name: str) -> None:
    rust_flags_dict = normalize_rust_flags(rust_flags)
    if "--edition" in rust_flags_dict:
        module.edition = list(rust_flags_dict["--edition"])[0]

    for cfg in rust_flags_dict.get("--cfg", set()):
        # This cfg is not actually used in code; Chromium only uses it to force
        # rebuilds on rustc rolls. It doesn't hurt, per se, but it does create
        # annoying diff noise on Android.bp files, so we drop it for
        # aesthetic/convenience reasons.
        if cfg.startswith("cr_rustc_revision="):
            continue
        feature_regex = re.match(_FEATURE_REGEX, cfg)
        if feature_regex:
            module.features.add(feature_regex.group(1))
        else:
            module.cfgs.add(cfg)

    pre_filter_flags = []
    for (key, values) in rust_flags_dict.items():
        if values is None:
            pre_filter_flags.append(key)
        else:
            pre_filter_flags.extend(f"{key}={param_val}"
                                    for param_val in values)

    flags_to_remove = _RUST_FLAGS_TO_REMOVE
    # AOSP compiles everything for host under panic=unwind instead of abort.
    # In order to be consistent with the ecosystem, remove the -Cpanic flag.
    if arch_name == "host":
        flags_to_remove.append("-Cpanic")

    # Remove restricted flags
    for pre_filter_flag in pre_filter_flags:
        if not any(
                pre_filter_flag.startswith(restricted_flag)
                for restricted_flag in flags_to_remove):
            module.flags.append(pre_filter_flag)


def create_jni_registration_modules(blueprint, gn, target, is_test_target,
                                    context):
    header_genrule = create_action_module(blueprint,
                                          gn,
                                          target,
                                          'cc_genrule',
                                          is_test_target,
                                          context,
                                          mode='headers')

    # Export both root genDir and the subdirectory to satisfy different include styles.
    header_genrule.export_include_dirs.update(
        [".", "components/cronet/android"])

    cc_genrule = create_action_module(blueprint,
                                      gn,
                                      target,
                                      'cc_genrule',
                                      is_test_target,
                                      context,
                                      mode='source')

    cc_genrule.genrule_srcs = {f":{cc_genrule.name}"}

    return [cc_genrule, header_genrule]


def _create_initial_modules(blueprint, gn, target, bp_module_name,
                            parent_gn_type, is_test_target, context):
    gn_target_name = target.name
    if target.type == 'executable':
        if target.testonly:
            module_type = 'cc_test'
        else:
            # Can be used for both host and device targets.
            module_type = 'cc_binary'
        modules = (soong_ast.create_module(module_type,
                                           bp_module_name,
                                           gn_target_name,
                                           context,
                                           is_test=is_test_target), )
    elif target.type == 'rust_executable':
        modules = (soong_ast.create_module("rust_binary",
                                           bp_module_name,
                                           gn_target_name,
                                           context,
                                           is_test=is_test_target), )
    elif target.type == "rust_library":
        # Here we have to choose between rust_library_rlib and rust_ffi_static.
        #
        # Ideally we should pick rust_library_rlib if there are rust_library
        # dependents, or rust_ffi_static if there are cc_library dependents.
        # This is a bit tricky, however, because it's theoretically possible for
        # *both* Rust and C++ code to directly depend on the library.
        #
        # In practice, there is currently no real difference between
        # rust_library_rlib and rust_ffi_static as far as the actual build process
        # is concerned - they are practically interchangeable. So, to keep things
        # simple, we just arbitrarily pick one - here rust_ffi_static on
        # suggestion of AOSP Rust people. See http://b/383552450.
        #
        # This decision may need to be revisited if the AOSP build system starts
        # treating rust_library_rlib and rust_ffi_static differently.
        modules = (soong_ast.create_module("rust_ffi_static",
                                           bp_module_name,
                                           gn_target_name,
                                           context,
                                           is_test=is_test_target), )
    elif target.type == "rust_proc_macro":
        modules = (soong_ast.create_module("rust_proc_macro",
                                           bp_module_name,
                                           gn_target_name,
                                           context,
                                           is_test=is_test_target), )
    elif target.type in ['static_library', 'source_set']:
        modules = (soong_ast.create_module('cc_library_static',
                                           bp_module_name,
                                           gn_target_name,
                                           context,
                                           is_test=is_test_target), )
    elif target.type == 'shared_library':
        modules = (soong_ast.create_module('cc_library_shared',
                                           bp_module_name,
                                           gn_target_name,
                                           context,
                                           is_test=is_test_target), )
    elif target.type == 'proto_library':
        modules = create_proto_modules(blueprint, gn, target, is_test_target,
                                       context)
        if modules is None:
            return ()
    elif target.type == "rust_bindgen":
        modules = (create_bindgen_module(blueprint, target, bp_module_name,
                                         is_test_target, context), )
    elif target.type == 'action':
        jni_zero_target_type = soong_ast.get_jni_zero_target_type(target)
        if jni_zero_target_type == soong_ast.JniZeroTargetType.REGISTRATION_GENERATOR and parent_gn_type != "java_library":
            modules = create_jni_registration_modules(blueprint, gn, target,
                                                      is_test_target, context)
        else:
            module = create_action_module(
                blueprint, gn, target, 'java_genrule' if parent_gn_type
                == "java_library" else 'cc_genrule', is_test_target, context)
            module.jni_zero_target_type = jni_zero_target_type
            modules = (module, )
    elif target.type == 'action_foreach':
        if target.script == "//third_party/rust/cxx/chromium_integration/run_cxxbridge.py":
            modules = create_rust_cxx_modules(blueprint, gn, target,
                                              is_test_target, context)
        else:
            modules = create_action_foreach_modules(blueprint, gn, target,
                                                    is_test_target, context)
    elif target.type == 'copy':
        # Copy targets are not supported: currently, we stop traversing the
        # dependency tree when we encounter one.
        return ()
    elif target.type == 'java_library':
        modules = (create_java_module(bp_module_name, target, blueprint,
                                      is_test_target, context), )
    elif target.type == 'aidl_interface':
        modules = create_aidl_module(bp_module_name, target, blueprint,
                                     is_test_target, context)
    else:
        # Note we don't have to handle `group` targets because parse_gn_desc() never
        # returns any; it just recurses through them and bubbles their dependencies
        # upwards.
        raise Exception('Unknown target %s (%s)' % (target.name, target.type))
    return modules


def _extract_cc_global_properties(target):
    cpp_std = None
    version_script = None

    # Check common
    if target.common.cflags:
        cpp_std = _get_cpp_std(target.common.cflags)
    if target.common.ldflags:
        _, version_script = _extract_version_script(target.common.ldflags)

    # Check archs
    for arch in target.get_archs().values():
        arch_cpp_std = _get_cpp_std(arch.cflags)
        if arch_cpp_std:
            # The -std= compiler option has a dedicated property in Android.bp, called cpp_std. That property
            # can only be set at module top level; it cannot be set per-target. However in GN
            # cflags are arch-specific, so we will find -std= when running on the
            # arch-specific module. Hence we need to go back to the main module and set it there.
            assert cpp_std is None or cpp_std == arch_cpp_std, f"Found different CPP version across different architectures!, target name: {target.name}, first cpp version: {cpp_std}, current cpp version: {arch_cpp_std}"
            cpp_std = arch_cpp_std

        if arch.ldflags:
            _, arch_version_script = _extract_version_script(arch.ldflags)
            if arch_version_script:
                assert version_script is None or version_script == arch_version_script, f"Found different version scripts across different architectures!, target name: {target.name}, first version_script: {version_script}, second version_script: {arch_version_script}"
                version_script = arch_version_script

    return cpp_std, version_script


def _configure_cc_properties(module, target, blueprint, context):
    if isinstance(module, soong_ast.CcModule):
        module.rtti = target.rtti

    if target.type in gn_utils.LINKER_UNIT_TYPES:
        cpp_std, version_script = _extract_cc_global_properties(target)
        if cpp_std:
            module.cpp_std = cpp_std
        if version_script:
            # Unfortunately, Soong does not allow accessing linker scripts from parent
            # path. So create a filegroup at the top-level Android.bp and reference it instead.
            filegroup_module = _create_linker_script_filegroup(
                version_script, module._is_test, context)
            blueprint.add_module(filegroup_module)
            module.version_script = f':{filegroup_module.name}'

        configure_cc_module(module, target.common.cflags,
                            target.common.defines, target.common.ldflags,
                            target.common.libs, blueprint, context)
        set_module_include_dirs(module, target.common.cflags,
                                target.common.include_dirs, context)
        # TODO: set_module_xxx is confusing, apply similar function to module and target in better way.
        for arch_name, arch in target.get_archs().items():
            # TODO(aymanm): Make libs arch-specific.
            configure_cc_module(module.target[arch_name], arch.cflags,
                                arch.defines, arch.ldflags, arch.libs,
                                blueprint, context)
            # -Xclang -target-feature -Xclang +mte are used to enable MTE (Memory Tagging Extensions).
            # Flags which does not start with '-' could not be in the cflags so enabling MTE by
            # -march and -mcpu Feature Modifiers. MTE is only available on arm64. This is needed for
            # building //base/allocator/partition_allocator:partition_alloc for arm64.
            if '+mte' in arch.cflags and arch_name == 'android_arm64':
                module.target[arch_name].cflags.add('-march=armv8-a+memtag')
            set_module_include_dirs(module.target[arch_name], arch.cflags,
                                    arch.include_dirs, context)
        if module.type == 'cc_library_static':
            module.export_generated_headers = module.generated_headers

        if module.type == 'cc_library_shared':
            output_name = target.output_name
            if output_name is None:
                module.stem = 'lib' + target.get_target_name().removesuffix(
                    gn_utils.TESTING_SUFFIX)
            else:
                module.stem = 'lib' + output_name


def _configure_rust_properties(module, target):
    if module.type in ["rust_proc_macro", "rust_binary", "rust_ffi_static"]:
        module.crate_name = target.crate_name
        module.crate_root = gn_utils.label_to_path(target.crate_root)
        if target.common.inputs:
            module.srcs.update(
                gn_utils.label_to_path(inp) for inp in target.common.inputs)
        if target.rust_package_version:
            module.cargo_env_compat = True
            module.cargo_pkg_version = target.rust_package_version
        module.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
        module.apex_available.add(common.tethering_apex)
        for arch_name, arch in target.get_archs().items():
            _set_rust_flags(module.target[arch_name], arch.rust_flags,
                            arch_name)

    if module.type in ("rust_proc_macro", "rust_binary", "rust_ffi_static",
                       "rust_bindgen"):
        # We may end up (in)directly depending on cc modules, e.g. through the
        # rust bindgen "generated headers" library we may generate. Our cc modules
        # set this. We need to be consistent, otherwise Soong will complain about
        # the incompatible dependency.
        module.target['host'].compile_multilib = '64'

    if module.type in ("rust_bindgen", "rust_ffi_static", "cc_genrule",
                       "cc_library_static", "cc_binary", "rust_binary"):
        # If we don't add this, then some types of AOSP builds fail due to an
        # issue with proc_macro2 - see https://crbug.com/392704960.
        # Note: technically we only need this on modules that ultimately depend
        # on proc_macro2, but there doesn't seem to be any downside to just set
        # it everywhere, so for simplicity we do just that.
        module.host_cross_supported = False


def _configure_common_properties(module, target, context):
    if not module.type == "rust_proc_macro":
        # rust_proc_macro modules does not support the fields of `host_supported`
        # or `device_supported`. In a different world, we would have classes for
        # each different module that specifies what it can support to avoid
        # those kind of conditions.
        #
        # See go/android.bp for additional information.
        module.host_supported = target.host_supported()
        module.device_supported = target.device_supported()

    module.gn_type = target.type
    module.build_file_path = target.build_file_path
    # Chromium does not use visibility at all, in order to avoid visibility issues
    # in AOSP. Make every module visible to any module in external/cronet.
    module.visibility.add("//external/cronet:__subpackages__")

    if module.is_genrule():
        module.apex_available.add(common.tethering_apex)

    if (module.is_compiled() and not module.type.startswith("java")
            and not module.type.startswith("rust")):
        # Don't try to inject library/source dependencies into genrules or
        # filegroups because they are not compiled in the traditional sense.
        module.defaults.add(context.cc_defaults_module)


def _resolve_dependencies(blueprint, gn, module, target, is_test_target,
                          context):
    # dep_name is an unmangled GN target name (e.g. //foo:bar(toolchain)).
    all_deps = [(dep_name, 'common') for dep_name in target.proto_deps]
    all_deps += [(dep_name, 'common') for dep_name in target.common.deps]
    for arch_name, arch in target.arch.items():
        all_deps += [(dep_name, arch_name) for dep_name in arch.deps]

    # Sort deps before iteration to make result deterministic.
    for (dep_name, arch_name) in sorted(all_deps):
        module_target = module.target[
            arch_name] if arch_name != 'common' else module
        # |translation_config.builtin_deps| override GN deps with Android-specific ones. See the
        # config in the top of this file.
        if dep_name in translation_config.builtin_deps:
            translation_config.builtin_deps[dep_name](
                module.java_unfiltered_module
                if module.is_java_top_level_module() else module, arch_name,
                context)
            continue

        for dep_module in create_modules_from_target(blueprint, gn, dep_name,
                                                     target.type,
                                                     is_test_target, context):
            if dep_name in translation_config.replace_deps:
                translation_config.replace_deps[dep_name](
                    module.java_unfiltered_module
                    if module.is_java_top_level_module() else module,
                    arch_name, context)
                continue

            if dep_module is None:
                continue

            # TODO: Proper dependency check for genrule.
            # Currently, only propagating genrule dependencies.
            # Also, currently, all the dependencies are propagated upwards.
            # in gn, public_deps should be propagated but deps should not.
            # Not sure this information is available in the desc.json.
            # Following rule works for adding android_runtime_jni_headers to base:base.
            # If this doesn't work for other target, hardcoding for specific target
            # might be better.
            if module.is_genrule() and dep_module.is_genrule():
                if module_target.gn_type != "proto_library":
                    # proto_library are treated differently because each proto action
                    # is split into two different targets, a cpp target and a header target.
                    # the cpp target is used as the entry point to the proto action, hence
                    # it should not be propagated as a genrule header because it generates
                    # cpp files only.
                    module_target.genrule_headers.add(dep_module.name)
                module_target.genrule_headers.update(
                    dep_module.genrule_headers)

            # For filegroups, and genrule, recurse but don't apply the
            # deps.
            if not module.is_compiled() or module.is_genrule():
                continue

            # Drop compiled modules that doesn't provide any benefit. This is mostly
            # applicable to source_sets when converted to cc_static_library, sometimes
            # the source set only has header files which are dropped so the module becomes empty.
            # is_compiled is there to prevent dropping of genrules.
            if dep_module.is_compiled() and not dep_module.has_input_files():
                continue

            module_is_cc = module.type in [
                'cc_library_shared', 'cc_binary', 'cc_library_static'
            ]

            if dep_module.type == 'cc_library_shared':
                module_target.shared_libs.add(dep_module.name)
            elif dep_module.type == 'cc_library_static' or (
                    dep_module.type == "rust_ffi_static" and module_is_cc):
                if module.type in [
                        'cc_library_shared', 'cc_binary', 'rust_binary',
                        'cc_library_static'
                ]:
                    if module.type != 'cc_library_static':
                        module_target.whole_static_libs.add(dep_module.name)
                    else:
                        if hasattr(dep_module, 'generated_headers'):
                            module_target.generated_headers.update(
                                dep_module.generated_headers)
                    module_target.shared_libs.update(
                        getattr(dep_module, 'shared_libs', set()))
                    module_target.header_libs.update(
                        getattr(dep_module, 'header_libs', set()))
                elif module.type in ('rust_ffi_static', 'rust_bindgen'):
                    module_target.shared_libs.update(dep_module.shared_libs)
                    # Add the cc_library_static as a static_lib to ensure that
                    # they propagate their exported headers correctly.
                    module_target.static_libs.add(dep_module.name)
                elif module.type == 'rust_proc_macro' and dep_module.type == 'cc_library_static':
                    # rust_proc_macro cannot depend on cc_library_static. Having said
                    # that, we still need these dependencies to further bubble them up
                    # to rust_proc_macro targets dependencies, so simply ignore them.
                    # See https:/crbug.com/417429009.
                    pass
                else:
                    raise Exception(
                        f"Cannot add {dep_module.name} ({dep_module.type}) to {module.name} ({module.type})"
                    )
            elif dep_module.type == "rust_bindgen":
                if module.type.startswith("rust"):
                    # Soong does not support using `rust_bindgen` modules directly as an input because
                    # it produces more than a single output (b/467420029). Create an intermediate
                    # genrule that copies that rust file and use it instead.
                    intermediate_target = _create_extract_rust_files_target(
                        dep_module, blueprint, context).name
                    module.srcs.add(":" + intermediate_target)
                    if module.crate_root and module.crate_root.startswith(
                            "out/"):
                        # Sometimes the crate_root is an output of another module which is indicated
                        # by a path starting with "out/". The only case where this happens at the moment
                        # is when a rust_library is created for the rust_bindgen output.
                        module.crate_root = f":{intermediate_target}"
                else:
                    module.srcs.add(":" + dep_module.name)
                if module_target.type == "cc_library_static":
                    # This is a bindgen _static_fns GN target. We need to translate that
                    # to the Soong rust_bindgen "static inline library" concept.

                    # AOSP Rust team wants every bindgen static inline library module to
                    # have a "lib" prefix. Due to the way Chromium //build/rust bindgen
                    # generator rules work, we know the _static_fns target is only
                    # referenced by its corresponding bindgen target and nothing else;
                    # therefore, we can safely assume we are only going to enter this
                    # path once, so there is no need to protect against the prefix being
                    # added multiple times - nor is there a need to go back and fix
                    # previous references.
                    module.name = "lib" + module.name
                    # rust_bindgen generates a .c / .cc file which has include
                    # defined from the root of the android tree.
                    module_target.include_dirs.append(".")
                    # The rust_bindgen has to know the name of the cc library which is going to
                    # consume it. We don't know that until we add the `rust_bindgen` as a dep.
                    dep_module.static_inline_library = module.name
            elif dep_module.type == "rust_ffi_static":
                if module.type in [
                        "rust_binary", "rust_proc_macro", "rust_ffi_static"
                ]:
                    module_target.rustlibs.add(dep_module.name)
            elif dep_module.type == "rust_proc_macro":
                module_target.proc_macros.add(dep_module.name)
            elif dep_module.type == "aidl_interface":
                # See https://cs.android.com/android/platform/superproject/main/+/main:system/tools/aidl/build/aidl_interface_backends.go
                # for how those modules "-lang-source" is generated.
                if module.type.startswith("cc_"):
                    module.srcs.add(f":{dep_module.name}-ndk-source")
                    module.generated_headers.add(
                        f"{dep_module.name}-ndk-source")
                    module.export_generated_headers.add(
                        f"{dep_module.name}-ndk-source")
                elif module.type.startswith("java_"):
                    module.srcs.add(f":{dep_module.name}-java-source")
                elif module.type.startswith("rust_"):
                    module.srcs.add(f":{dep_module.name}-rust-source")
            elif dep_module.type == 'cc_genrule':
                if dep_module.genrule_headers:
                    if module.type == "rust_ffi_static":
                        # Don't bubble up generated_headers on Rust modules, as that doesn't make sense
                        # (Rust cannot use C++ headers directly) and is not supported anyway. See also
                        # https://crbug.com/405987939.
                        # TODO: https://crbug.com/406267472 - how we end up in this situation in the
                        # first place is not entirely clear. We may have to revisit how generated
                        # headers interact with cxx/bindgen targets.
                        pass
                    elif module.type == "rust_bindgen":
                        # rust_bindgen modules don't support the `generated_headers` attribute;
                        # see http://crbug.com/394615281. We work around this limitation by
                        # inserting a module whose sole purpose is to export the generated
                        # headers, and then depending on that. See also
                        # http://crbug.com/394069879.
                        module_target.header_libs.add(
                            create_generated_headers_export_module(
                                blueprint, dep_module, context).name)
                    else:
                        module_target.generated_headers.update(
                            dep_module.genrule_headers)
                module_target.srcs.update(dep_module.genrule_srcs)
                module_target.shared_libs.update(
                    dep_module.genrule_shared_libs)
                module_target.header_libs.update(
                    dep_module.genrule_header_libs)
            elif dep_module.is_java_top_level_module():
                # A module depending on a module with system_current sdk version should also compile against
                # the system sdk. This is because a module's SDK API surface should be >= its deps SDK API surface.
                # And system_current has a larger API surface than current or module_current.
                if dep_module.sdk_version == 'system_current':
                    module_target.sdk_version = module_target.java_unfiltered_module.sdk_version = 'system_current'

                module_target.static_libs.add(dep_module.name)

                # `create_java_module()` implements Chromium's Java jar filtering
                # feature. Here we deal with another subtlety around that feature,
                # which is how jar filtering affects the inputs of the various build
                # steps.
                #
                # When Chromium runs javac, it runs it against the raw output of
                # javac from the dependencies. In other words, the javac classpath is
                # made of *unfiltered* jars. However, it is the *filtered* jars that
                # eventually get shipped in the final build outputs. javac running
                # against unfiltered jars is important - some targets rely on this
                # (e.g. //base:log_java pulling BuildConfig from
                # //build/android:build_java), so we need to preserve this behavior.
                #
                # Reproducing this in Soong is somewhat of a headache. The difficulty
                # is, in Soong `static_libs` dependencies on `java_library` modules
                # automatically bubble up the dependency tree. If we just list
                # `__unfiltered` modules in `static_libs`, the unfiltered jars will
                # propagate all the way to the final build outputs, which is not what
                # we want.
                #
                # To solve this problem, we generate two dependency trees: a filtered
                # tree that links top-level Java modules together, and an unfiltered
                # tree that links unfiltered Java modules together. When one depends
                # on the top-level modules one gets the filtered jars; when one
                # depends on the unfiltered module one gets the unfiltered jars. (This
                # is the reason why we have to have a separate top-level module and
                # can't just merge it with the filtered module: the dependency tree of
                # filtered modules indirectly includes unfiltered jars, which we don't
                # want to pull in top-level modules.)
                #
                # A keen eye will notice we still have a problem, because the
                # unfiltered dependencies of unfiltered modules will bubble up through
                # filtered modules and then to top-level targets. This would result in
                # top-level targets producing unfiltered jars, which is not what we
                # want.
                #
                # To solve this problem, we don't use `static_libs` on unfiltered
                # modules. Instead, we use `libs`. Indeed, Soong does *not* bubble up
                # `libs` dependencies, thus preventing unfiltered jars from bubbling
                # up and appearing in final build outputs.
                #
                # TODO: as if this wasn't complicated enough, in GN a `java_library`
                # can use a flag, `prevent_excluded_classes_from_classpath`, that
                # flips the above behavior and makes dependent compile targets pull
                # the *filtered* jars in the javac classpath instead of the unfiltered
                # ones. This flag is notably used in `generate_jni()` autogenerated
                # java_library targets to prevent the jni_zero placeholder classes
                # from bubbling up and potentially conflicting with their real
                # counterparts up the build tree. We currently do not support this
                # flag, i.e. we behave as if it is false. Surprisingly the resulting
                # build rules work anyway - presumably by sheer luck (classpath
                # ordering maybe?). In the future we may have to support it. This
                # should be easy - just depend on the filtered target instead of the
                # unfiltered target when the flag is true on the dependency.
                #
                # For even more more background, see https://crbug.com/397396295.
                module_target.add_java_dependency(dep_module)
            elif dep_module.type in ['genrule', 'java_genrule']:
                if dep_module.jni_zero_target_type == soong_ast.JniZeroTargetType.GENERATOR:
                    # TODO: we are special-casing jni_zero here. Ideally this should be
                    # handled more generically, by making gn2bp understand the general
                    # concept of a target depending on only a subset of the outputs of
                    # an action.
                    _, placeholder_path = get_jni_zero_generator_proxy_and_placeholder_paths(
                        dep_module)
                    if placeholder_path in target.common.inputs:
                        # The target depends on both jni_zero generator outputs (proxy and
                        # placeholder). We can simply pull both of them at the same time
                        # by depending on the jni_zero generator module directly. In
                        # practice this branch is taken when a standalone jni_zero library
                        # is being built separately from the JNI user code, such as the
                        # java_library generated by jni_zero's generate_jni() GN rule. One
                        # example is //base:command_line_jni_java.
                        module_target.srcs.add(":" + dep_module.name)
                    else:
                        # The target only depends on the generated proxy classes but not
                        # the placeholder classes. Typically this happens when the
                        # proxy classes are being compiled alongside the JNI user code: in
                        # this case there is no need for the placeholder classes since the
                        # user code provides all the necessary definitions. One example is
                        # //components/cronet/android:cronet_impl_native_java. In this
                        # situation it is imperative that we do *not* pull the
                        # placeholder classes, as they would conflict with user code. See
                        # https://crbug.com/397396295 for more background.
                        proxy_only_module = create_jni_zero_proxy_only_module(
                            dep_module, context)
                        blueprint.add_module(proxy_only_module)
                        module_target.srcs.add(f":{proxy_only_module.name}")
                else:
                    module_target.srcs.add(":" + dep_module.name)
            else:
                raise Exception(
                    'Unsupported arch-specific dependency %s of target %s with type %s'
                    % (dep_module.name, target.name, dep_module.type))


def create_modules_from_target(blueprint, gn, gn_target_name, parent_gn_type,
                               is_test_target, context):
    """Generate module(s) for a given GN target.

    Given a GN target name, generate one or more corresponding modules into a
    blueprint. Most of the time this will only generate one module, with some
    exceptions such as protos and rust cxxbridge generation.

    Args:
        blueprint: soong_ast.Blueprint instance which is being generated.
        gn: gn_utils.GnParser object.
        gn_target_name: GN target for module generation.
        parent_gn_type: GN type of the parent node.
    """
    bp_module_name = soong_ast.label_to_module_name(gn_target_name, context)
    target = gn.get_target(gn_target_name)

    # Append __java suffix to actions reachable from java_library. This is necessary
    # to differentiate them from cc actions.
    # This means that a GN action of name X will be translated to two different modules of names
    # X and X__java(only if X is reachable from a java target).
    if target.type == "action" and parent_gn_type == "java_library":
        bp_module_name += "__java"

    target_types_to_hash_module_name = [
        "rust_executable",
        "rust_library",
        "rust_proc_macro",
    ]
    if target.type in target_types_to_hash_module_name:
        # "lib{crate_name}" must be a prefix of the module name, this is a Soong
        # restriction.
        # https://cs.android.com/android/_/android/platform/build/soong/+/31934a55a8a1f9e4d56d68810f4a646f12ab6eb5:rust/library.go;l=724;drc=fdec8723d574daf54b956cc0f6dc879087da70a6;bpv=0;bpt=0
        # Use the hash of the module_name instead of the entire name otherwise we will
        # exceed the maximum file name length (b/376452102).
        bp_module_hash = hashlib.sha256(
            bp_module_name.encode('utf-8')).hexdigest()[:4]
        bp_module_name = f"lib{target.crate_name}__{bp_module_hash}"

    if bp_module_name in blueprint.modules:
        return (blueprint.modules[bp_module_name], )

    log.info('create modules for %s (%s)', target.name, target.type)

    if common.is_rust_build_script(target.script):
        # Build scripts are generated via `generate_build_scripts_output.py`. See the header
        # of that script for more details.
        generated_files = [
            output.split("/")[-1] for output in target.common.outputs if
            output.endswith(".rs") and not output.endswith("/cargo_flags.rs")
        ]
        if len(generated_files) == 0:
            # No files were generated by this build script. Just ignore it and return None.
            return (None, )
        # The `generated_outputs` is hardcoded as we assume that the `generate_build_scripts_output.py` script has executed
        # and generated all the necessary files in that destination. This creates some kind of hard dependencies between
        # those two scripts.
        # TODO(b/447593242): Find a better way to indicate to GN2BP that generate_build_scripts_output has generated those files.
        # TODO(b/447592983): Use architecture-specific fields instead of harcoding arm64.
        # Rust code typically consumes generated files using the following pattern:
        # include!(concat!(env!("OUT_DIR"), "/somefile.rs"));
        # Because this uses OUT_DIR the generated files will not be found if we just leave this
        # in the source tree - we need to copy them to the output directory. Hence this genrule.
        module = _create_rust_build_script_output_copy_genrule(
            bp_module_name,
            f"{target.rust_source_dir}/gn2bp_rust_build_script_outputs/arm64",
            generated_files, is_test_target, context)
        blueprint.add_module(module)
        return (module, )

    modules = _create_initial_modules(blueprint, gn, target, bp_module_name,
                                      parent_gn_type, is_test_target, context)
    if not modules:
        return ()

    for module in modules:
        blueprint.add_module(module)
        if target.type not in ['action', 'action_foreach', 'aidl_interface']:
            # Actions should get their srcs from their corresponding ActionSanitizer as actionSanitizer
            # filters srcs differently according to the type of the action.
            module.srcs.update(
                gn_utils.label_to_path(src) for src in target.common.sources
                if common.is_supported_source_file(src))

        # Add arch-specific properties
        for arch_name, arch in target.get_archs().items():
            module.target[arch_name].srcs.update(
                gn_utils.label_to_path(src) for src in arch.sources
                if common.is_supported_source_file(src))

        _configure_cc_properties(module, target, blueprint, context)
        _configure_rust_properties(module, target)
        _configure_common_properties(module, target, context)

        if gn_target_name in translation_config.replace_deps:
            # Do not recurse into translation_config.replace_deps target's dependencies.
            return (module, )

        _resolve_dependencies(blueprint, gn, module, target, is_test_target,
                              context)

        if module.is_java_top_level_module():
            # The Java top-level module is not the one doing the actual compiling; the
            # unfiltered module is, so it should get the srcs.
            module.java_unfiltered_module.srcs = module.srcs
            module.srcs = ()

    return modules
