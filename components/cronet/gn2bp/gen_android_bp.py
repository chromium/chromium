#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import enum
import json
import logging as log
import operator
import os
import re
import sys
import copy
from typing import List, Dict, Iterable, Set, Union
from pathlib import Path
import hashlib
import shlex
import collections
import gn_utils
import targets as gn2bp_targets
import soong_ast
import common
import translation_config
import context as translation_context
import translators

PARENT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir))

sys.path.insert(0, os.path.join(PARENT_ROOT, "license"))
import license_utils
import constants as license_constants

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
sys.path.insert(0, REPOSITORY_ROOT)

import components.cronet.tools.utils as cronet_utils


# Android equivalents for third-party libraries that the upstream project
# depends on. This will be applied to normal and testing targets.
def turn_off_allocator_shim_for_musl(module):
    allocation_shim = "base/allocator/partition_allocator/shim/allocator_shim.cc"
    allocator_shim_files = {
        allocation_shim,
        "base/allocator/partition_allocator/shim/allocator_shim_default_dispatch_to_glibc.cc",
    }
    module.srcs -= allocator_shim_files
    for arch in module.target.values():
        arch.srcs -= allocator_shim_files
    module.target['android'].srcs.add(allocation_shim)
    if gn_utils.TESTING_SUFFIX in module.name:
        # allocator_shim_default_dispatch_to_glibc is only added to the __testing version of base
        # since base_base__testing is compiled for host. When compiling for host. Soong compiles
        # using glibc or musl(experimental). We currently only support compiling for glibc.
        module.target['glibc'].srcs.update(allocator_shim_files)
    else:
        # allocator_shim_default_dispatch_to_glibc does not exist in the prod version of base
        # `base_base` since this only compiles for android and bionic is used. Bionic is the equivalent
        # of glibc but for android.
        module.target['glibc'].srcs.add(allocation_shim)


def create_cc_defaults_module(context: translation_context.TranslationContext):
    defaults = soong_ast.create_module('cc_defaults',
                                       context.cc_defaults_module,
                                       '//gn:default_deps', context)
    defaults.cflags = [
        # TODO: this list is brittle and painful to maintain. We are too easily
        # broken by changes to Chromium cflags, e.g. https://crbug.com/406704769.
        # Ideally this list should be deduced from GN cflags.
        '-DGOOGLE_PROTOBUF_NO_RTTI',
        '-Wno-error=return-type',
        '-Wno-non-virtual-dtor',
        '-Wno-macro-redefined',
        '-Wno-missing-field-initializers',
        '-Wno-sign-compare',
        '-Wno-sign-promo',
        '-Wno-unused-parameter',
        '-Wno-null-pointer-subtraction',  # Needed to libevent
        '-Wno-ambiguous-reversed-operator',  # needed for icui18n
        '-Wno-unreachable-code-loop-increment',  # needed for icui18n
        '-fPIC',
        '-Wno-c++11-narrowing',
        # Needed for address_space_randomization.h on riscv
        # Can be removed after 125.0.6375.0 is imported
        '-Wno-invalid-constexpr',
        # b/330508686 disable coverage profiling for files or function in this list.
        '-fprofile-list=external/cronet/exclude_coverage.list',
        # https://crrev.com/c/6396655/7/build/config/compiler/BUILD.gn
        # https://crbug.com/406704769
        '-Wno-nullability-completeness',
        # Stops warning about unknown options. This usually happens when
        # Chromium uses a newer version of Clang that supports a flag which
        # Android's clang does not know about.
        '-Wno-unknown-warning-option',
        # Required to correctly compile quiche tests.
        # TODO(crbug.com/433273929): Remove once fixed.
        "-Wno-nonnull",
    ]
    defaults.build_file_path = ""
    defaults.include_build_directory = False
    defaults.whole_program_vtables = True
    defaults.c_std = 'gnu11'
    # Chromium builds do not add a dependency for headers found inside the
    # sysroot, so they are added globally via defaults.
    defaults.target['android'].header_libs = [
        'jni_headers',
    ]
    defaults.target['android'].shared_libs = ['libmediandk']
    defaults.target['host'].cflags = [
        # -DANDROID is added by default but target.defines contain -DANDROID if
        # it's required.  So adding -UANDROID to cancel default -DANDROID if it's
        # not specified.
        # Note: -DANDROID is not consistently applied across the chromium code
        # base, so it is removed unconditionally for host targets.
        '-UANDROID',
    ]
    # Don't build 32-bit binaries for the host - otherwise
    # cronet_aml_base_base__testing fails to build on aosp_cheetah due to
    # partition_alloc failing on a static assertion that pointers are 64-bit.
    defaults.target['host'].compile_multilib = '64'
    defaults.stl = 'none'
    defaults.cpp_std = common.CPP_VERSION
    defaults.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
    defaults.apex_available.add(common.tethering_apex)
    return defaults


def _clean_gn_label(label: str) -> str:
    if ':' in label:
        path, target_name = label.rsplit(':', 1)
        target_name = re.sub(gn_utils.TOOLCHAIN_SUFFIX + r'[a-zA-Z0-9_]+', '',
                             target_name)
        target_name = re.sub(gn_utils.TESTING_SUFFIX, '', target_name)
        return f"{path}:{target_name}"
    return label


def apply_post_processing(module, context):
    clean_target = _clean_gn_label(module.gn_target)
    keys = []
    if module.role is None:
        keys.append(clean_target)
        if module._is_test:
            keys.append(f"{clean_target}#testing")
        else:
            keys.append(f"{clean_target}#nontesting")
    else:
        keys.append(f"{clean_target}#{module.role}")
        if module._is_test:
            keys.append(f"{clean_target}#{module.role}#testing")
        else:
            keys.append(f"{clean_target}#{module.role}#nontesting")

    for key in keys:
        for override in context.additional_args.get(key, []):
            override.apply(module)


def make_cc_defaults_from_boringssl(
        boringssl_module: soong_ast.Module,
        context: translation_context.TranslationContext) -> soong_ast.Module:
    module_name = boringssl_module.name + "__flags"
    cc_default_flags_module = soong_ast.create_module(
        "cc_defaults",
        module_name,
        "Flags auto-extracted from BoringSSL GN rules, to be used in manually maintained BoringSSL Android.bp rules",
        context,
        is_test=boringssl_module._is_test)

    libcrypto_cc_defaults_flags_module = soong_ast.create_module(
        "cc_defaults",
        f'{module_name}_libcrypto',
        f"""This cc_defaults inherits the same flags from {module_name} except some flags that breaks FIPS compliance.""",
        context,
        is_test=boringssl_module._is_test)

    def _get_libcrypto_cflags(cflags):
        return [
            cflag for cflag in cflags
            if all(not cflag.startswith(denied_prefix) for denied_prefix in [
                # Breaks FIPS compliance as this is used by the linker's `--gc-sections` to remove
                # unused sections which breaks the hash. `-fno-*-sections` is intentionally not
                # added here as those flags are used to build all of boringSSL, while only
                # boringCrypto(libcrypto) requires it.
                "-ffunction-sections",
                "-fdata-sections",
                # Causes 'tot_cronet_bcm_object.o:(.text): multiple relocation sections to one section are not supported'
                # during linking stage.
                "-fno-unique-section-names",
            ])
        ]

    def _get_libcrypto_ldflags(ldflags):
        return [
            ldflag for ldflag in ldflags
            if all(not ldflag.startswith(denied_prefix) for denied_prefix in [
                # -Wl, --gc-sections is effectively useless when '-ffunctions-sections' and
                # '-fdata-sections' are not used. Hence remove it.
                "-Wl,--gc-sections",
            ])
        ]

    cc_default_flags_module.cflags = boringssl_module.cflags
    cc_default_flags_module.ldflags = boringssl_module.ldflags
    libcrypto_cc_defaults_flags_module.cflags = _get_libcrypto_cflags(
        boringssl_module.cflags)
    libcrypto_cc_defaults_flags_module.ldflags = _get_libcrypto_ldflags(
        boringssl_module.ldflags)
    for arch, variant in boringssl_module.target.items():
        cc_default_flags_module.target[arch].cflags = variant.cflags
        cc_default_flags_module.target[arch].ldflags = variant.ldflags
        libcrypto_cc_defaults_flags_module.target[
            arch].cflags = _get_libcrypto_cflags(variant.cflags)
        libcrypto_cc_defaults_flags_module.target[
            arch].ldflags = _get_libcrypto_ldflags(variant.ldflags)

    cc_default_flags_module.build_file_path = ""
    libcrypto_cc_defaults_flags_module.build_file_path = ""
    cc_default_flags_module.defaults.add(context.cc_defaults_module)
    libcrypto_cc_defaults_flags_module.defaults.add(context.cc_defaults_module)
    return (cc_default_flags_module, libcrypto_cc_defaults_flags_module)


def setup_libcrypto_stripping(blueprint, context):
    # Two-step build process for libhttpengine.so:
    # 1. Build against the full libcrypto to identify required symbols and
    # generate a stripped variant of libcrypto.
    # 2. Rebuild against that stripped libcrypto to guarantee no undefined symbols.
    cronet_shared_library_module = blueprint.modules[
        f"{context.module_prefix}components_cronet_android_cronet"]
    cronet_shared_library_copy = copy.deepcopy(cronet_shared_library_module)
    cronet_shared_library_copy.name += "_against_unstripped_libcrypto"
    cronet_shared_library_module.shared_libs.remove(
        f"{context.module_prefix}{translation_config.LIBCRYPTO_UNSTRIPPED}")
    cronet_shared_library_module.shared_libs.add(
        f"{context.module_prefix}{translation_config.LIBCRYPTO_STRIPPED}")
    blueprint.add_module(cronet_shared_library_copy)


def create_blueprint_for_targets(gn, targets, test_targets, context):
    """Generate a blueprint for a list of GN targets."""
    blueprint = soong_ast.Blueprint()

    # Default settings used by all modules.
    blueprint.add_module(create_cc_defaults_module(context))

    for target in targets:
        modules = translators.create_modules_from_target(blueprint,
                                                         gn,
                                                         target,
                                                         parent_gn_type=None,
                                                         is_test_target=False,
                                                         context=context)
        for module in modules:
            module.visibility.update(
                translation_config.root_modules_visibility)

    for test_target in test_targets:
        modules = translators.create_modules_from_target(
            blueprint,
            gn,
            test_target + gn_utils.TESTING_SUFFIX,
            parent_gn_type=None,
            is_test_target=True,
            context=context)
        for module in modules:
            module.visibility.update(
                translation_config.root_modules_visibility)

    # Merge in additional hardcoded arguments.
    for module in blueprint.modules.values():
        apply_post_processing(module, context)

    for module in make_cc_defaults_from_boringssl(
            blueprint.modules[soong_ast.label_to_module_name(
                "//third_party/boringssl:boringssl", context)], context):
        blueprint.add_module(module)

    setup_libcrypto_stripping(blueprint, context)
    return blueprint


def _rebase_file(filepath: str, blueprint_path: str) -> Union[str, None]:
    """
  Rebases a single file, this method delegates to _rebase_files

  :param filepath: a single string representing filepath.
  :param blueprint_path: Path for which the srcs will be rebased relative to.
  :returns The rebased filepaths or None.
  """
    rebased_file = _rebase_files([filepath], blueprint_path)
    if rebased_file:
        return list(rebased_file)[0]
    return None


def _rebase_files(filepaths, parent_prefix):
    """
  Rebase a list of filepaths according to the provided path. This assumes
  that the |filepaths| are subdirectories of the |parent|.
  If the assumption is violated then None is returned.

  Note: filepath can be references to other modules (eg: ":module"), those
  are added as-is without any translation.

  :param filepaths: Collection of strings representing filepaths.
  :param parent_prefix: Path for which the srcs will be rebased relative to.
  :returns The rebased filepaths or None.
  """
    if not parent_prefix:
        return filepaths

    rebased_srcs = set()
    for src in filepaths:
        if src.startswith(":"):
            # This is a reference to another Android.bp module, add as-is.
            rebased_srcs.add(src)
            continue

        if not src.startswith(parent_prefix):
            # This module depends on a source file that is not in its subpackage.
            return None
        # Remove the BUILD file path to make it relative.
        rebased_srcs.add(src[len(parent_prefix) + 1:])
    return rebased_srcs


# TODO: Move to Module's class.
def _rebase_module(module: soong_ast.Module,
                   blueprint_path: str) -> Union[soong_ast.Module, None]:
    """
  Rebases the module specified on top of the blueprint_path if possible.
  If the rebase operation has failed, None is returned to indicate that the
  module should stay as a top-level module.

  Currently, there is no support for rebasing genrules and libraries that
  breaks the package boundaries.

  :returns A new module based on the provided one but rebased or None.
  """

    module_copy = copy.deepcopy(module)
    # TODO: Find a better way to rebase attribute and verify if all rebase operations
    # have succeeded or not.
    if getattr(module_copy, 'crate_root', None):
        module_copy.crate_root = _rebase_file(module_copy.crate_root,
                                              blueprint_path)
        if module_copy.crate_root is None:
            return None

    if getattr(module_copy, 'path', None):
        module_copy.path = _rebase_file(module_copy.path, blueprint_path)
        if module_copy.path is None:
            return None

    if getattr(module_copy, 'wrapper_src', None):
        module_copy.wrapper_src = _rebase_file(module_copy.wrapper_src,
                                               blueprint_path)
        if module_copy.wrapper_src is None:
            return None

    if getattr(module_copy, 'srcs', None):
        module_copy.srcs = _rebase_files(module_copy.srcs, blueprint_path)
        if module_copy.srcs is None:
            return None

    if getattr(module_copy, 'jars', None):
        module_copy.jars = _rebase_files(module_copy.jars, blueprint_path)
        if module_copy.jars is None:
            return None

    for (arch_name, _) in module_copy.target.items():
        module_copy.target[arch_name].srcs = (_rebase_files(
            module_copy.target[arch_name].srcs, blueprint_path))
        if module_copy.target[arch_name].srcs is None:
            return None

    return module_copy


def _path_to_name(path: str,
                  context: translation_context.TranslationContext) -> str:
    path = path.replace("/", "_").lower()
    return f"{context.module_prefix}{path}_license"


def _maybe_create_license_module(
    path: str, context: translation_context.TranslationContext
) -> Union[soong_ast.Module, None]:
    """
  Creates a module license if a README.chromium exists in the path provided
  otherwise just returns None.

  :param path: Path to check for README.chromium
  :return: Module or None.
  """
    readme_relative_path = os.path.join(path, "README.chromium")
    readme_chromium_file = Path(
        os.path.join(REPOSITORY_ROOT, path, "README.chromium"))
    if (not readme_chromium_file.exists()
            or license_utils.is_ignored_readme_chromium(readme_relative_path)):
        return None

    license_module = soong_ast.create_module("license",
                                             _path_to_name(path, context),
                                             "License-Artificial", context)
    license_module.visibility = {":__subpackages__"}
    # Assume that a LICENSE file always exist as we run the
    # create_android_metadata_license.py script each time we run GN2BP.
    license_module.license_text = {"LICENSE"}
    metadata = license_utils.parse_chromium_readme_file(
        str(readme_chromium_file),
        license_constants.POST_PROCESS_OPERATION.get(
            readme_relative_path, lambda _metadata: _metadata))
    for license_name in metadata.get_licenses():
        license_module.license_kinds.add(
            license_utils.get_license_bp_name(license_name))
    return license_module


def _get_longest_matching_blueprint(
    current_blueprint_path: str, all_blueprints: Dict[str, soong_ast.Blueprint]
) -> Union[soong_ast.Blueprint, None]:
    longest_path_matching = None
    for (blueprint_path, search_blueprint) in all_blueprints.items():
        if (search_blueprint.get_license_module()
                and current_blueprint_path.startswith(blueprint_path)
                and (longest_path_matching is None
                     or len(blueprint_path) > len(longest_path_matching))):
            longest_path_matching = blueprint_path

    if longest_path_matching:
        return all_blueprints[longest_path_matching]
    return None


def finalize_package_modules(blueprints: Dict[str, soong_ast.Blueprint],
                             context: translation_context.TranslationContext):
    """
  Adds a package module to every blueprint passed in |blueprints|. A package
  module is just a reference to a license module, the approach here is that
  the package module will point to the nearest ancestor's license module, the
  nearest ancestor could be in the same Android.bp.

  :param blueprints: Dictionary of (path, blueprint) to be populated with
  """

    for (current_path, blueprint) in blueprints.items():
        if current_path == "":
            # Don't add a package module for the top-level Android.bp, this is handled
            # manually in Android.extras.bp.
            continue

        package_module = soong_ast.create_module("package", None,
                                                 "Package-Artificial", context)
        if blueprint.get_license_module():
            package_module.default_applicable_licenses.add(
                blueprint.get_license_module().name)
        else:  # Search for closest ancestor with a license module
            ancestor_blueprint = _get_longest_matching_blueprint(
                current_path, blueprints)
            if ancestor_blueprint:
                # We found an ancestor, make a reference to its license module
                package_module.default_applicable_licenses.add(
                    ancestor_blueprint.get_license_module().name)
            else:
                # No ancestor with a license found, this is most likely a non-third
                # license, just point at Chromium's license in Android.extras.bp.
                package_module.default_applicable_licenses.add(
                    "external_cronet_license")

        blueprint.set_package_module(package_module)


def create_license_modules(
    blueprints: Dict[str, soong_ast.Blueprint],
    context: translation_context.TranslationContext
) -> Dict[str, soong_ast.Module]:
    """
  Creates license module (if possible) for each blueprint passed, a license
  module will be created if a README.chromium exists in the same directory as
  the BUILD.gn which created that blueprint.

  Note: A blueprint can be in a different directory than where the BUILD.gn is
  declared, this is the case in rust crates.

  :param blueprints: List of paths for all possible blueprints.
  :return: Dictionary of (path, license_module).
  """
    license_modules = {}
    for blueprint_path, blueprint in blueprints.items():
        if not blueprint.get_readme_location():
            # Don't generate a license for the top-level Android.bp as this is handled
            # manually in Android.extras.bp
            continue

        license_module = _maybe_create_license_module(
            blueprint.get_readme_location(), context)
        if license_module:
            license_modules[blueprint_path] = license_module
    return license_modules


def _get_rust_crate_root_directory_from_crate_root(crate_root: str) -> str:
    if crate_root and crate_root.startswith(
            "third_party/rust/chromium_crates_io/vendor"):
        # Return the first 5 directories (a/b/c/d/e)
        crate_root_dir = crate_root.split("/")[:5]
        return "/".join(crate_root_dir)
    return None


def _locate_android_bp_destination(module: soong_ast.Module) -> str:
    """Returns the appropriate location of the generated Android.bp for the
  specified module. Sometimes it is favourable to relocate the Android.bp to
  a different location other than next to BUILD.gn (eg: rust's BUILD.gn are
  defined in a different directory than the source code).

  :returns the appropriate location for the blueprint
  """
    crate_root_dir = _get_rust_crate_root_directory_from_crate_root(
        getattr(module, 'crate_root', None))
    if module.build_file_path in translation_config.BLUEPRINTS_MAPPING:
        return translation_config.BLUEPRINTS_MAPPING[module.build_file_path]
    if crate_root_dir:
        return crate_root_dir
    return module.build_file_path


def _break_down_blueprint(top_level_blueprint: soong_ast.Blueprint):
    """
  This breaks down the top-level blueprint into smaller blueprints in
  different directory. The goal here is to break down the huge Android.bp
  into smaller ones for compliance with SBOM. At the moment, not all targets
  can be easily rebased to a different directory as GN does not respect
  package boundaries.

  :returns A dictionary of path -> Blueprint, the path is relative to repository
  root.
  """
    blueprints = {"": soong_ast.Blueprint()}
    for (module_name, module) in top_level_blueprint.modules.items():
        if module.type in [
                "package", "genrule", "cc_genrule", "java_genrule",
                "cc_preprocess_no_configuration"
        ] and not module.allow_rebasing:
            # Exclude the genrules from the rebasing as there is no support for them.
            # cc_preprocess_no_configuration is created only for the sake of genrules as an intermediate
            # target.
            blueprints[""].add_module(module)
            continue

        android_bp_path = _locate_android_bp_destination(module)
        # third_party/android_deps is not imported which means that copybara will not
        # pick up the Android.bp in there. Instead direct the modules to the top-level
        # Android.bp
        if android_bp_path.startswith("third_party/android_deps"):
            blueprints[""].add_module(module)
            continue
        if android_bp_path is None:
            # Raise an exception if the module does not specify a BUILD file path.
            raise Exception(
                f"Found module {module_name} without a build file path.")

        rebased_module = _rebase_module(module, android_bp_path)
        if rebased_module:
            if android_bp_path not in blueprints.keys():
                blueprints[android_bp_path] = soong_ast.Blueprint(
                    module.build_file_path)
            blueprints[android_bp_path].add_module(rebased_module)
        else:
            # Append to the top-level blueprint.
            blueprints[""].add_module(module)

    for blueprint in blueprints.values():
        if blueprint.get_buildgn_location() in gn2bp_targets.README_MAPPING:
            blueprint.set_readme_location(
                gn2bp_targets.README_MAPPING[blueprint.get_buildgn_location()])
    return blueprints


def main():
    parser = argparse.ArgumentParser(
        description='Generate Android.bp from a GN description.')
    parser.add_argument(
        '--desc',
        help='GN description (e.g., gn desc out --format=json "//*".' +
        'You can specify multiple --desc options for different target_cpu',
        required=True,
        action='append')
    parser.add_argument(
        '--output_dir',
        required=True,
        help='Directory where the generated Android.bp files should be written'
    )
    parser.add_argument(
        '--build_script_output',
        help=
        'JSON-formatted file containing output of build scripts broken down' +
        'by architecture.',
        required=True)
    parser.add_argument(
        '--extras',
        help='Extra targets to include at the end of the Blueprint file',
        default=os.path.join(gn_utils.repo_root(), 'Android.bp.extras'),
    )
    parser.add_argument(
        '--output',
        help='Blueprint file to create',
        default=os.path.join(gn_utils.repo_root(), 'Android.bp'),
    )
    parser.add_argument(
        '-v',
        '--verbose',
        help='Print debug logs.',
        action='store_true',
    )
    parser.add_argument(
        'targets',
        nargs=argparse.REMAINDER,
        help='Targets to include in the blueprint (e.g., "//:perfetto_tests")')

    parser.add_argument(
        '--channel',
        help='The channel this Android.bp generation is being performed for.',
        type=str,
        choices=['tot', 'stable'],
        default='tot')
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        '--license',
        help='Generate license.',
        dest='license',
        action='store_true',
    )
    group.add_argument(
        '--no-license',
        help='Do not generate license.',
        dest='license',
        action='store_false',
    )
    parser.set_defaults(license=True)
    args = parser.parse_args()

    if args.verbose:
        log.basicConfig(format='%(levelname)s:%(funcName)s:%(message)s',
                        level=log.DEBUG)

    context = translation_context.TranslationContext(args.channel)
    targets = args.targets or gn2bp_targets.DEFAULT_TARGETS
    build_scripts_output = None
    with open(args.build_script_output) as f:
        build_scripts_output = json.load(f)
    gn = gn_utils.GnParser(translation_config.builtin_deps,
                           build_scripts_output)
    for desc_file in args.desc:
        with open(desc_file) as f:
            desc = json.load(f)
        for target in targets:
            gn.parse_gn_desc(desc, target)
        for test_target in gn2bp_targets.DEFAULT_TESTS:
            gn.parse_gn_desc(desc, test_target, is_test_target=True)
    gn.propagate_properties()
    top_level_blueprint = create_blueprint_for_targets(
        gn, targets, gn2bp_targets.DEFAULT_TESTS, context)

    final_blueprints = _break_down_blueprint(top_level_blueprint)
    if args.license:
        license_modules = create_license_modules(final_blueprints, context)
        for (path, module) in license_modules.items():
            final_blueprints[path].set_license_module(module)

    finalize_package_modules(final_blueprints, context)

    header = """// Copyright (C) 2022 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is automatically generated by %s. Do not edit.
""" % (Path(__file__).name)

    for (path, blueprint) in final_blueprints.items():
        android_bp_file = Path(
            os.path.join(args.output_dir, path, "Android.bp"))
        android_bp_file.parent.mkdir(parents=True, exist_ok=True)
        android_bp_file.write_text("\n".join(
            [header] + translation_config.BLUEPRINTS_EXTRAS.get(path, []) +
            blueprint.to_string(
                exclude_gn_targets=translation_config.replace_deps.keys())))

    return 0


if __name__ == '__main__':
    sys.exit(main())
