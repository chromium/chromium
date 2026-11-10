#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import enum  # noqa: F401
import itertools
import json
import logging as log
import operator  # noqa: F401
import os
import re
import sys
import copy
from typing import List, Dict, Iterable, Set, Tuple, Union  # noqa: F401
from pathlib import Path
import hashlib  # noqa: F401
import shlex  # noqa: F401
import collections  # noqa: F401

sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))
import gn_utils  # noqa: E402
import targets as gn2bp_targets  # noqa: E402
import soong_ast  # noqa: E402
import common  # noqa: E402
import translation_config  # noqa: E402
import context as translation_context  # noqa: E402
import translators  # noqa: E402

PARENT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir)
)

sys.path.insert(0, os.path.join(PARENT_ROOT, "license"))
import license_utils  # noqa: E402
import constants as license_constants  # noqa: E402

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)
)
sys.path.insert(0, REPOSITORY_ROOT)

import components.cronet.tools.utils as cronet_utils  # noqa: E402


# Android equivalents for third-party libraries that the upstream project
# depends on. This will be applied to normal and testing targets.
def turn_off_allocator_shim_for_musl(module):
    allocation_shim = (
        "base/allocator/partition_allocator/shim/allocator_shim.cc"
    )
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
    defaults = soong_ast.create_module(
        'cc_defaults', context.cc_defaults_module, '//gn:default_deps', context
    )
    defaults.cflags = [
        # Flags below are required to neutralize warnings enabled by Soong's
        # global C/C++ defaults (build/soong/cc/config/global.go) or differences
        # between Chromium's and Android's Clang versions. Target-specific and
        # Chromium-wide warning flags (-Wno-*, -fPIC, etc.) are propagated
        # directly from GN cflags/cflags_cc in translators.py.
        '-DGOOGLE_PROTOBUF_NO_RTTI',
        '-Wno-error=return-type',
        '-Wno-non-virtual-dtor',
        '-Wno-macro-redefined',
        '-Wno-sign-compare',
        '-Wno-sign-promo',
        '-Wno-ambiguous-reversed-operator',  # needed for icui18n
        '-Wno-unreachable-code-loop-increment',  # needed for icui18n
        # b/330508686 disable coverage profiling for files or function in this list.
        '-fprofile-list=external/cronet/exclude_coverage.list',
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
    defaults.extra_include_dir_deps.add(
        f':{context.module_prefix}libcxx_headers'
    )
    return defaults


def _clean_gn_label(label: str) -> str:
    if ':' in label:
        path, target_name = label.rsplit(':', 1)
        target_name = re.sub(
            gn_utils.TOOLCHAIN_SUFFIX + r'[a-zA-Z0-9_]+', '', target_name
        )
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
    context: translation_context.TranslationContext,
) -> soong_ast.Module:
    module_name = boringssl_module.name + "__flags"
    cc_default_flags_module = soong_ast.create_module(
        "cc_defaults",
        module_name,
        "Flags auto-extracted from BoringSSL GN rules, to be used in manually maintained BoringSSL Android.bp rules",
        context,
        is_test=boringssl_module._is_test,
    )

    libcrypto_cc_defaults_flags_module = soong_ast.create_module(
        "cc_defaults",
        f'{module_name}_libcrypto',
        f"""This cc_defaults inherits the same flags from {module_name} except some flags that breaks FIPS compliance.""",
        context,
        is_test=boringssl_module._is_test,
    )

    def _get_libcrypto_cflags(cflags):
        return [
            cflag
            for cflag in cflags
            if all(
                not cflag.startswith(denied_prefix)
                for denied_prefix in [
                    # Breaks FIPS compliance as this is used by the linker's `--gc-sections` to remove
                    # unused sections which breaks the hash. `-fno-*-sections` is intentionally not
                    # added here as those flags are used to build all of boringSSL, while only
                    # boringCrypto(libcrypto) requires it.
                    "-ffunction-sections",
                    "-fdata-sections",
                    # Causes 'tot_cronet_bcm_object.o:(.text): multiple relocation sections to one section are not supported'
                    # during linking stage.
                    "-fno-unique-section-names",
                ]
            )
        ]

    def _get_libcrypto_ldflags(ldflags):
        return [
            ldflag
            for ldflag in ldflags
            if all(
                not ldflag.startswith(denied_prefix)
                for denied_prefix in [
                    # -Wl, --gc-sections is effectively useless when '-ffunctions-sections' and
                    # '-fdata-sections' are not used. Hence remove it.
                    "-Wl,--gc-sections",
                ]
            )
        ]

    cc_default_flags_module.cflags = boringssl_module.cflags
    cc_default_flags_module.ldflags = boringssl_module.ldflags
    libcrypto_cc_defaults_flags_module.cflags = _get_libcrypto_cflags(
        boringssl_module.cflags
    )
    libcrypto_cc_defaults_flags_module.ldflags = _get_libcrypto_ldflags(
        boringssl_module.ldflags
    )
    for arch, variant in boringssl_module.target.items():
        cc_default_flags_module.target[arch].cflags = variant.cflags
        cc_default_flags_module.target[arch].ldflags = variant.ldflags
        libcrypto_cc_defaults_flags_module.target[
            arch
        ].cflags = _get_libcrypto_cflags(variant.cflags)
        libcrypto_cc_defaults_flags_module.target[
            arch
        ].ldflags = _get_libcrypto_ldflags(variant.ldflags)

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
        f"{context.module_prefix}components_cronet_android_cronet"
    ]
    cronet_shared_library_copy = copy.deepcopy(cronet_shared_library_module)
    cronet_shared_library_copy.name += "_against_unstripped_libcrypto"
    cronet_shared_library_module.shared_libs.remove(
        f"{context.module_prefix}{translation_config.LIBCRYPTO_UNSTRIPPED}"
    )
    cronet_shared_library_module.shared_libs.add(
        f"{context.module_prefix}{translation_config.LIBCRYPTO_STRIPPED}"
    )
    blueprint.add_module(cronet_shared_library_copy)

    # Generates a linker version script ("intersection.ld") containing the list of
    # symbols that are shared between libhttpengine.so and libcrypto.so.
    #
    # Mechanism:
    #   It calculates the intersection of:
    #   1. Undefined symbols in the Cronet object (what Cronet needs).
    #   2. Defined symbols in the Libcrypto (what Libcrypto provides).
    #
    # Why:
    # To satisfy FIPS compliance, libcrypto must be a shared library, which defaults
    # to exporting all symbols and prevents dead-code elimination. Because
    # libhttpengine.so is the exclusive consumer, we generate a map of only the
    # symbols it actually uses. This restricts the export list, enabling the
    # linker to delete unused code and discard unused libcrypto functions.
    #
    # How:
    # libhttpengine.so is initially linked against the full libcrypto.so to
    # determine necessary symbols. These symbols are used to generate a pruned
    # libcrypto_stripped.so. Finally, libhttpengine.so is recompiled against this
    # stripped version to ensure link-time integrity.
    linker_script_genrule = soong_ast.create_module(
        "cc_genrule",
        f"{context.module_prefix}generate_libcronet_libcrypto_symbol_linker_script",
        "Extra-Artificial",
        context,
    )
    linker_script_genrule.build_file_path = ""
    linker_script_genrule.srcs = {
        f":{cronet_shared_library_copy.name}",
        f":{context.module_prefix}{translation_config.LIBCRYPTO_UNSTRIPPED}",
    }
    linker_script_genrule.out = {"intersection.ld"}
    linker_script_genrule.tools = {"llvm-objdump"}
    linker_script_genrule.tool_files = {
        "build/android/java_home.py",
        "components/cronet/tools/symbols_map_generator/generate_version_script_from_symbols_intersection.py",
        "components/cronet/tools/utils.py",
    }
    linker_script_genrule.cmd = [
        "python3 $(location components/cronet/tools/symbols_map_generator/generate_version_script_from_symbols_intersection.py)",
        "--objdump $(location llvm-objdump)",
        f"--lib-undefined $(location :{cronet_shared_library_copy.name})",
        f"--lib-defined $(location :{context.module_prefix}{translation_config.LIBCRYPTO_UNSTRIPPED})",
        "--out $(out)",
    ]
    linker_script_genrule.apex_available = {common.tethering_apex}
    blueprint.add_module(linker_script_genrule)


def _data_to_soong_globs(data_paths: Iterable[str]) -> Set[str]:
    globs = set()
    for path in data_paths:
        clean = gn_utils.label_to_path(path)
        if clean.endswith("/"):
            globs.add(f"{clean}**/*")
        else:
            globs.add(clean)
    return globs


def create_extras_modules(
    gn: gn_utils.GnParser,
    context: translation_context.TranslationContext,
) -> List[soong_ast.Module]:
    """Creates top-level extra Soong modules for Cronet in AOSP."""
    modules = []

    def _new_extra_module(mod_type: str, short_name: str) -> soong_ast.Module:
        mod = soong_ast.create_module(
            mod_type,
            f"{context.module_prefix}{short_name}",
            "Extra-Artificial",
            context,
        )
        mod.build_file_path = ""
        modules.append(mod)
        return mod

    license_mod = _new_extra_module("license", "license")
    license_mod.license_kinds = {
        "SPDX-license-identifier-Apache-2.0",
        "SPDX-license-identifier-BSD",
        "SPDX-license-identifier-BSD-3-Clause",
        "SPDX-license-identifier-BSD-Source-Code",
        "SPDX-license-identifier-BSD-2-Clause",
        "SPDX-license-identifier-BSD-4-Clause",
        "SPDX-license-identifier-BSL-1.0",
        "SPDX-license-identifier-ICU",
        "SPDX-license-identifier-ISC",
        "SPDX-license-identifier-MIT",
        "SPDX-license-identifier-MPL",
        "SPDX-license-identifier-MPL-1.1",
        "SPDX-license-identifier-MPL-2.0",
        "SPDX-license-identifier-NCSA",
        "SPDX-license-identifier-Zlib",
        # "SPDX-license-identifier-GPL-2.0-with-autoconf-exception", # ICU TODO(aymanm): Reapply those once aosp/2470343 is merged
        # "SPDX-license-identifier-GPL-3.0-with-autoconf-exception", # ICU TODO(aymanm): Reapply those once aosp/2470343 is merged
        "SPDX-license-identifier-OpenSSL",
        "SPDX-license-identifier-Unicode-DFS-2016",
        "legacy_unencumbered",
    }
    license_mod.license_text = {"LICENSE"}

    repackage_test_target_name = f"//components/cronet/android:repackage_test_jars{gn_utils.TESTING_SUFFIX}"
    tests_final_jni_modules = set()
    if repackage_test_target_name in gn.all_targets:
        repackage_test_target = gn.get_target(repackage_test_target_name)
        tests_final_jni_modules = {
            soong_ast.label_to_module_name(dep, context)
            for dep in repackage_test_target.common.deps
            if gn.get_target(dep).type == "java_library"
            and dep.endswith(f"_final_jni_java{gn_utils.TESTING_SUFFIX}")
        }

    # Redirect "jni_registration" to "final_jni". This module name is being referenced
    # in the top-level external/cronet/Android.bp file so we temporarily alias it to
    # avoid having to coordinate an Android-side change.
    jni_reg_testing = _new_extra_module(
        "java_library",
        "components_cronet_android_cronet_tests_jni_registration_java__testing",
    )
    jni_reg_testing.apex_available = {common.tethering_apex}
    jni_reg_testing.defaults = {
        f"{context.module_prefix}java_framework_defaults"
    }
    jni_reg_testing.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
    jni_reg_testing.static_libs = set(tests_final_jni_modules)
    jni_reg_testing.visibility = {"//external/cronet:__pkg__"}

    api_java = _new_extra_module("java_library", "api_java")
    api_java.static_libs = {
        soong_ast.label_to_module_name(
            translation_config.java_api_target_name, context
        )
    }
    api_java.sdk_version = "module_current"
    api_java.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
    api_java.visibility = {
        "//packages/modules/Connectivity:__subpackages__",
        "//external/cronet/android:__subpackages__",
    }
    api_java.apex_available = {common.tethering_apex}

    java_mod = _new_extra_module("java_library", "java")
    prod_target_name = "//components/cronet/android:gn2bp_production_targets"
    if prod_target_name in gn.all_targets:
        prod_target = gn.get_target(prod_target_name)
        java_mod.static_libs = {
            soong_ast.label_to_module_name(dep, context)
            for dep in prod_target.common.deps
            if gn.get_target(dep).type == "java_library"
            and dep != translation_config.java_api_target_name
        }
    java_mod.sdk_version = "module_current"
    java_mod.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
    java_mod.visibility = {"//external/cronet:__subpackages__"}
    java_mod.apex_available = {common.tethering_apex}

    java_testing = _new_extra_module("java_library", "java__testing")
    java_testing.static_libs = {
        f"{lib}{gn_utils.TESTING_SUFFIX}" for lib in java_mod.static_libs
    } | tests_final_jni_modules
    java_testing.sdk_version = "module_current"
    java_testing.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
    java_testing.visibility = {"//external/cronet:__subpackages__"}
    java_testing.apex_available = {common.tethering_apex}

    for_testing_net_java = _new_extra_module(
        "java_library", "for_testing_net_java"
    )
    net_unittests_target_name = f"//net:net_unittests{gn_utils.TESTING_SUFFIX}"
    if net_unittests_target_name in gn.all_targets:
        net_unittests_target = gn.get_target(net_unittests_target_name)
        for_testing_net_java.static_libs = {
            soong_ast.label_to_module_name(dep, context)
            for dep in net_unittests_target.common.deps
            if dep.startswith("//net/android:")
            and gn.get_target(dep).type == "java_library"
            and gn.get_target(dep).testonly
        }

    net_unittests_data = _new_extra_module("filegroup", "net_unittests_data")
    net_unittests_lib_name = (
        f"//net:net_unittests__library{gn_utils.TESTING_SUFFIX}"
    )
    if net_unittests_lib_name in gn.all_targets:
        net_unittests_lib = gn.get_target(net_unittests_lib_name)
        net_unittests_data.srcs = _data_to_soong_globs(
            net_unittests_lib.common.data
        )

    javatests_resources = _new_extra_module("filegroup", "javatests_resources")
    cronet_javatests_target_name = (
        f"//components/cronet/android:cronet_javatests{gn_utils.TESTING_SUFFIX}"
    )
    if cronet_javatests_target_name in gn.all_targets:
        cronet_javatests_target = gn.get_target(cronet_javatests_target_name)
        javatests_resources.srcs = _data_to_soong_globs(
            cronet_javatests_target.common.data
        )
    javatests_resources.visibility = {"//external/cronet:__subpackages__"}

    java_tests = _new_extra_module("java_library", "java_tests")
    java_tests.sdk_version = "module_current"
    java_tests.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
    java_tests.static_libs = {
        "androidx.test.ext.junit",
        "androidx.test.rules",
        "guava",
        "hamcrest-library",
        "junit",
        "net-tests-utils",
        "truth",
        *java_testing.static_libs,
    }
    if cronet_javatests_target_name in gn.all_targets:
        java_tests.static_libs.add(
            soong_ast.label_to_module_name(
                cronet_javatests_target_name, context
            )
        )
    java_tests.libs = {
        "android.test.base.stubs.system",
        f"{context.module_prefix}api_java",
        "framework-connectivity.stubs.module_lib",
        "framework-connectivity-pre-jarjar",
        # android.net.TrafficStats apis
        "framework-connectivity-t.stubs.module_lib",
        "guava",
    }
    java_tests.lint = {"test": True}
    java_tests.visibility = {"//external/cronet:__subpackages__"}

    java_framework_defaults = _new_extra_module(
        "java_defaults", "java_framework_defaults"
    )
    java_framework_defaults.sdk_version = "module_current"
    # LINT.IfChange
    java_framework_defaults.java_version = "25"
    # LINT.ThenChange(
    #     //build/android/gradle/generate_gradle.py,
    #     //build/android/gyp/compile_java.py
    # )
    java_framework_defaults.libs = {
        "framework-annotations-lib",
        "framework-connectivity-pre-jarjar-without-cronet",
        "framework-connectivity.stubs.module_lib",
        "framework-connectivity-t.stubs.module_lib",
        "framework-location.stubs.module_lib",
        "framework-mediaprovider.stubs.module_lib",
        "framework-statsd.stubs.module_lib",
        "framework-wifi.stubs.module_lib",
        "framework-sdkextensions.stubs.module_lib",
        "framework-conscrypt-nsc.stubs",
    }
    java_framework_defaults.visibility = {"//external/cronet:__subpackages__"}

    # rust_bindgen modules do not have an `include_dirs` attribute. Due to that,
    # C++ headers generated from rust_bindgen can't reference other headers
    # through paths relative to the repository root. This is what Chromium does
    # so, to circumvent this issue, create this module that adds the root of the
    # repository to `include_dirs`. This will be added to all modules depending
    # on a rust_bindgen module.
    repo_root_anchor = _new_extra_module(
        "cc_library_headers", "repository_root_include_dirs_anchor"
    )
    repo_root_anchor.host_supported = True
    repo_root_anchor.apex_available = {common.tethering_apex}
    repo_root_anchor.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
    repo_root_anchor.export_include_dirs = {"."}
    repo_root_anchor.visibility = {"//external/cronet:__subpackages__"}

    # Bundled libc++ headers do not have file extensions (e.g. <algorithm>,
    # <vector>, __assertion_handler, __config_site), and libc++ sources include
    # runtime/*.ipp outside include_dirs, so Soong's default include_dirs header
    # globs do not capture them for CC action sandboxing. Expose them via a
    # filegroup referenced by ${GN2BP_MODULE_PREFIX}cc_defaults.
    # C++ standard library headers are extensionless; restrict the include/
    # glob to top-level files and exclude files with extensions to optimize
    # build.ninja's size (https://crrev.com/c/8663081).
    libcxx_headers = _new_extra_module("filegroup", "libcxx_headers")
    libcxx_target_name = "//buildtools/third_party/libc++:libc++"
    if libcxx_target_name in gn.all_targets:
        libcxx_target = gn.get_target(libcxx_target_name)
        for inp in libcxx_target.common.inputs:
            libcxx_headers.srcs.add(gn_utils.label_to_path(inp))
        for cflag in itertools.chain(
            libcxx_target.common.cflags,
            *(arch.cflags for arch in libcxx_target.get_archs().values()),
        ):
            if cflag.startswith("-isystem../../third_party/libc++/"):
                inc_path = cflag.removeprefix("-isystem../../").rstrip("/")
                libcxx_headers.srcs.add(f"{inc_path}/*")
                libcxx_headers.exclude_srcs.add(f"{inc_path}/*.*")
        for inc_dir in libcxx_target.common.include_dirs:
            if inc_dir.startswith("//third_party/libc++/"):
                clean_dir = gn_utils.label_to_path(inc_dir).rstrip("/")
                libcxx_headers.srcs.add(f"{clean_dir}/**/*.ipp")
    libcxx_headers.visibility = {"//external/cronet:__subpackages__"}

    return modules


def create_blueprint_for_targets(gn, targets, test_targets, context):
    """Generate a blueprint for a list of GN targets."""
    blueprint = soong_ast.Blueprint()

    # Default settings used by all modules.
    blueprint.add_module(create_cc_defaults_module(context))

    for target in targets:
        modules = translators.create_modules_from_target(
            blueprint,
            gn,
            target,
            parent_gn_type=None,
            is_test_target=False,
            context=context,
        )
        for module in modules:
            module.visibility.update(translation_config.root_modules_visibility)

    for test_target in test_targets:
        modules = translators.create_modules_from_target(
            blueprint,
            gn,
            test_target + gn_utils.TESTING_SUFFIX,
            parent_gn_type=None,
            is_test_target=True,
            context=context,
        )
        for module in modules:
            module.visibility.update(translation_config.root_modules_visibility)

    # Merge in additional hardcoded arguments.
    for module in blueprint.modules.values():
        apply_post_processing(module, context)

    for module in make_cc_defaults_from_boringssl(
        blueprint.modules[
            soong_ast.label_to_module_name(
                "//third_party/boringssl:boringssl", context
            )
        ],
        context,
    ):
        blueprint.add_module(module)

    setup_libcrypto_stripping(blueprint, context)
    for module in create_extras_modules(gn, context):
        blueprint.add_module(module)
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
        rebased_srcs.add(src[len(parent_prefix) + 1 :])
    return rebased_srcs


# TODO: Move to Module's class.
def _rebase_module(
    module: soong_ast.Module, blueprint_path: str
) -> Union[soong_ast.Module, None]:
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
        module_copy.crate_root = _rebase_file(
            module_copy.crate_root, blueprint_path
        )
        if module_copy.crate_root is None:
            return None

    if getattr(module_copy, 'path', None):
        module_copy.path = _rebase_file(module_copy.path, blueprint_path)
        if module_copy.path is None:
            return None

    if getattr(module_copy, 'wrapper_src', None):
        module_copy.wrapper_src = _rebase_file(
            module_copy.wrapper_src, blueprint_path
        )
        if module_copy.wrapper_src is None:
            return None

    if getattr(module_copy, 'srcs', None):
        module_copy.srcs = _rebase_files(module_copy.srcs, blueprint_path)
        if module_copy.srcs is None:
            return None

    if getattr(module_copy, 'extra_include_dir_deps', None):
        module_copy.extra_include_dir_deps = _rebase_files(
            module_copy.extra_include_dir_deps, blueprint_path
        )
        if module_copy.extra_include_dir_deps is None:
            return None

    if getattr(module_copy, 'jars', None):
        module_copy.jars = _rebase_files(module_copy.jars, blueprint_path)
        if module_copy.jars is None:
            return None

    for arch_name, _ in module_copy.target.items():
        module_copy.target[arch_name].srcs = _rebase_files(
            module_copy.target[arch_name].srcs, blueprint_path
        )
        if module_copy.target[arch_name].srcs is None:
            return None

    return module_copy


def _path_to_name(
    path: str, context: translation_context.TranslationContext
) -> str:
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
        os.path.join(REPOSITORY_ROOT, path, "README.chromium")
    )
    if (
        not readme_chromium_file.exists()
        or license_utils.is_ignored_readme_chromium(readme_relative_path)
    ):
        return None

    license_module = soong_ast.create_module(
        "license", _path_to_name(path, context), "License-Artificial", context
    )
    license_module.visibility = {":__subpackages__"}
    # Assume that a LICENSE file always exist as we run the
    # create_android_metadata_license.py script each time we run GN2BP.
    license_module.license_text = {"LICENSE"}
    metadata = license_utils.parse_chromium_readme_file(
        str(readme_chromium_file),
        license_constants.POST_PROCESS_OPERATION.get(
            readme_relative_path, lambda _metadata: _metadata
        ),
    )
    for license_name in metadata.get_licenses():
        license_module.license_kinds.add(
            license_utils.get_license_bp_name(license_name)
        )
    return license_module


def _get_longest_matching_blueprint(
    current_blueprint_path: str, all_blueprints: Dict[str, soong_ast.Blueprint]
) -> Union[soong_ast.Blueprint, None]:
    longest_path_matching = None
    for blueprint_path, search_blueprint in all_blueprints.items():
        if (
            search_blueprint.get_license_module()
            and current_blueprint_path.startswith(blueprint_path)
            and (
                longest_path_matching is None
                or len(blueprint_path) > len(longest_path_matching)
            )
        ):
            longest_path_matching = blueprint_path

    if longest_path_matching:
        return all_blueprints[longest_path_matching]
    return None


def finalize_package_modules(
    blueprints: Dict[str, soong_ast.Blueprint],
    context: translation_context.TranslationContext,
):
    """
    Adds a package module to every blueprint passed in |blueprints|. A package
    module is just a reference to a license module, the approach here is that
    the package module will point to the nearest ancestor's license module, the
    nearest ancestor could be in the same Android.bp.

    :param blueprints: Dictionary of (path, blueprint) to be populated with
    """

    for current_path, blueprint in blueprints.items():
        if current_path == "":
            # Don't add a package module for the top-level Android.bp.
            continue

        package_module = soong_ast.create_module(
            "package", None, "Package-Artificial", context
        )
        if blueprint.get_license_module():
            package_module.default_applicable_licenses.add(
                blueprint.get_license_module().name
            )
        else:  # Search for closest ancestor with a license module
            ancestor_blueprint = _get_longest_matching_blueprint(
                current_path, blueprints
            )
            if ancestor_blueprint:
                # We found an ancestor, make a reference to its license module
                package_module.default_applicable_licenses.add(
                    ancestor_blueprint.get_license_module().name
                )
            else:
                # No ancestor with a license found, this is most likely a non-third
                # license, just point at Chromium's license.
                package_module.default_applicable_licenses.add(
                    "external_cronet_license"
                )

        blueprint.set_package_module(package_module)


def create_license_modules(
    blueprints: Dict[str, soong_ast.Blueprint],
    context: translation_context.TranslationContext,
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
            # by create_extras_modules().
            continue

        license_module = _maybe_create_license_module(
            blueprint.get_readme_location(), context
        )
        if license_module:
            license_modules[blueprint_path] = license_module
    return license_modules


def _get_rust_crate_root_directory_from_crate_root(crate_root: str) -> str:
    if crate_root and crate_root.startswith(
        "third_party/rust/chromium_crates_io/vendor"
    ):
        # Return the first 5 directories (a/b/c/d/e)
        crate_root_dir = crate_root.split("/")[:5]
        return "/".join(crate_root_dir)
    return None


_CPP_SOURCE_EXTENSIONS = (
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".inc",
    ".inl",
    ".ipp",
    ".def",
    ".S",
    ".s",
    ".m",
    ".mm",
)
# This is a naive regex that does not understand conditional includes
# (i.e. #ifdef XXX #include), and therefore may report false positives.
# Ideally we should ask the compiler to dump the list of included files after
# preprocessing, but that may be significantly more complicated and slower.
_CPP_INCLUDE_REGEX = re.compile(
    r'^\s*#\s*(?:include|import)\s+[<"]([^>"]+)[>"]', re.MULTILINE
)


def extract_copybara_paths(
    gn: gn_utils.GnParser,
    top_level_blueprint: Union[soong_ast.Blueprint, None] = None,
    final_blueprints: Union[Dict[str, soong_ast.Blueprint], None] = None,
    context: Union[translation_context.TranslationContext, None] = None,
) -> List[str]:
    """Extracts exact file paths and required globs for Copybara from the GN graph and blueprints."""
    paths = {
        f"{src_dir}/**"
        for src_dir, dst_dir in translation_config.BLUEPRINTS_MAPPING.items()
        if not dst_dir
    }
    excluded_prefixes = (
        tuple(
            d.removeprefix(f"{context.tree_path}/")
            for d in context.include_dirs_denylist
        )
        if context is not None
        else ()
    )

    def _add_path(raw_path: Union[str, None]):
        if (
            not raw_path
            or raw_path == "//cp"
            or raw_path.startswith((":", "//out/", "out/"))
        ):
            return
        is_dir = raw_path.endswith("/")
        clean_path = os.path.normpath(raw_path.removeprefix("//"))
        if is_dir:
            clean_path = f"{clean_path}/**"
        elif clean_path.endswith("/**/*"):
            clean_path = clean_path.removesuffix("/*")
        elif clean_path.endswith("/*"):
            clean_path = f"{clean_path}*"
        if clean_path in ("", ".") or clean_path.startswith(("/", "../")):
            return
        if clean_path.startswith(excluded_prefixes):
            return
        if clean_path == "BUILD.gn" or clean_path.endswith(
            ("/BUILD.gn", ".gni", ".map.txt")
        ):
            return
        paths.add(clean_path)

    include_dirs = [""]
    seen_include_dirs = {""}

    def _add_include_dir(raw_dir: Union[str, None]):
        if not raw_dir or raw_dir.startswith((":", "//out/", "out/")):
            return
        clean_dir = os.path.normpath(
            raw_dir.removeprefix("external/cronet/tot/")
            .removeprefix("external/cronet/stable/")
            .removeprefix("//")
        )
        if clean_dir == ".":
            clean_dir = ""
        if clean_dir.startswith(("/", "../", "external/cronet/")):
            return
        if clean_dir.startswith(excluded_prefixes):
            return
        if clean_dir not in seen_include_dirs and os.path.isdir(
            os.path.join(REPOSITORY_ROOT, clean_dir)
        ):
            seen_include_dirs.add(clean_dir)
            include_dirs.append(clean_dir)

    build_dirs = set()
    for target in gn.all_targets.values():
        if target.build_file_path:
            build_dirs.add(
                gn2bp_targets.README_MAPPING.get(
                    target.build_file_path, target.build_file_path
                )
            )
        _add_path(target.crate_root)
        for src in itertools.chain(
            target.public_headers, target.transitive_jni_java_sources
        ):
            _add_path(src)
        for arch in (target.common, *target.arch.values()):
            for src in itertools.chain(arch.sources, arch.data):
                _add_path(src)

    for src in getattr(gn, "jni_java_sources", ()):
        _add_path(src)

    if top_level_blueprint is not None:
        for module in top_level_blueprint.modules.values():
            for attr in (
                "crate_root",
                "wrapper_src",
                "version_script",
                "test_config",
                "jarjar_rules",
                "manifest",
            ):
                _add_path(getattr(module, attr, None))
            for prop in (module, *module.target.values()):
                for attr in (
                    "include_dirs",
                    "local_include_dirs",
                    "export_include_dirs",
                ):
                    for val in getattr(prop, attr, None) or ():
                        _add_include_dir(val)
                for attr in (
                    "srcs",
                    "tool_files",
                    "jars",
                    "extra_include_dir_deps",
                    "init_rc",
                    "linker_scripts",
                ):
                    for val in getattr(prop, attr, None) or ():
                        _add_path(val)

    glob_prefixes = tuple(
        p.removesuffix("**") for p in paths if p.endswith("/**")
    )
    paths = {
        p
        for p in paths
        if not (
            p.endswith("/**")
            and any(
                p != f"{prefix}**" and p.startswith(prefix)
                for prefix in glob_prefixes
            )
        )
    }

    cpp_queue = [p for p in paths if p.endswith(_CPP_SOURCE_EXTENSIONS)]
    visited_cpp = set(cpp_queue)
    inc_cache: Dict[str, List[str]] = {}
    file_cache: Dict[str, bool] = {}

    def _is_repo_file(rel_path: str) -> bool:
        res = file_cache.get(rel_path)
        if res is None:
            res = os.path.isfile(os.path.join(REPOSITORY_ROOT, rel_path))
            file_cache[rel_path] = res
        return res

    def _is_valid_include_candidate(cand: str) -> bool:
        return (
            cand not in ("", ".")
            and not cand.startswith(("/", "../"))
            and not cand.startswith(excluded_prefixes)
            and not cand.startswith(glob_prefixes)
            and _is_repo_file(cand)
        )

    def _resolve_include_from_dirs(inc: str) -> List[str]:
        if inc in inc_cache:
            return inc_cache[inc]
        matches = []
        for inc_dir in include_dirs:
            cand = (
                os.path.normpath(os.path.join(inc_dir, inc))
                if inc_dir
                else os.path.normpath(inc)
            )
            if _is_valid_include_candidate(cand):
                matches.append(cand)
        inc_cache[inc] = matches
        return matches

    while cpp_queue:
        curr = cpp_queue.pop()
        if not _is_repo_file(curr):
            continue
        try:
            with open(
                os.path.join(REPOSITORY_ROOT, curr), "r", errors="ignore"
            ) as f:
                content = f.read()
        except OSError:
            continue
        curr_dir = os.path.dirname(curr)
        for match in _CPP_INCLUDE_REGEX.finditer(content):
            inc = match.group(1)
            rel_cand = (
                os.path.normpath(os.path.join(curr_dir, inc))
                if curr_dir
                else os.path.normpath(inc)
            )
            if _is_valid_include_candidate(rel_cand):
                found_list = [rel_cand]
            else:
                found_list = _resolve_include_from_dirs(inc)
            for found in found_list:
                if found not in visited_cpp:
                    visited_cpp.add(found)
                    _add_path(found)
                    cpp_queue.append(found)

    for inc_dir in include_dirs:
        if (
            inc_dir
            and not inc_dir.startswith(glob_prefixes)
            and not any(p.startswith(f"{inc_dir}/") for p in paths)
        ):
            paths.add(f"{inc_dir}/**")

    if final_blueprints is not None:
        for bp_path, blueprint in final_blueprints.items():
            if bp_path:
                _add_path(f"{bp_path}/Android.bp")
            if blueprint.get_readme_location():
                build_dirs.add(blueprint.get_readme_location())

    readme_candidates = {
        os.path.join(d, "README.chromium") for d in build_dirs if d
    }
    for inc_readme in license_constants.INCLUDED_README:
        if any(inc_readme.startswith(f"{d}/") for d in build_dirs):
            readme_candidates.add(inc_readme)

    for readme_rel in readme_candidates:
        if (
            readme_rel in license_constants.IGNORED_README
            or readme_rel.startswith(excluded_prefixes)
        ):
            continue
        readme_abs = os.path.join(REPOSITORY_ROOT, readme_rel)
        if not os.path.exists(readme_abs):
            continue
        _add_path(readme_rel)
        readme_dir_rel = os.path.dirname(readme_rel)
        metadata = license_utils.parse_chromium_readme_file(
            readme_abs,
            license_constants.POST_PROCESS_OPERATION.get(
                readme_rel, lambda _metadata: _metadata
            ),
        )
        license_file_path = metadata.get_license_file_path()
        if (
            readme_dir_rel.startswith("third_party/rust/")
            and license_file_path
            and license_file_path.startswith("//")
        ):
            rel_license_dir = license_file_path[
                2 : license_file_path.rfind("/")
            ]
        else:
            rel_license_dir = readme_dir_rel
        _add_path(f"{rel_license_dir}/LICENSE")
        _add_path(f"{rel_license_dir}/METADATA")
        for license_name in metadata.get_licenses():
            _add_path(
                f"{rel_license_dir}/MODULE_LICENSE_{license_utils.get_license_file_format(license_name)}"
            )
        if license_file_path:
            resolved_abs = os.path.join(
                REPOSITORY_ROOT,
                license_utils.resolve_license_path(
                    os.path.dirname(readme_abs), license_file_path
                ),
            )
            _add_path(os.path.relpath(resolved_abs, REPOSITORY_ROOT))

    return sorted(paths)


def write_copybara_paths_file(
    output_dir: str,
    generated_paths: Iterable[str],
):
    """Writes a Starlark file listing generated origin paths for Copybara."""
    header = f"""# Copyright (C) 2026 The Chromium Authors
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
#
# This file is automatically generated by {Path(__file__).name}. Do not edit.
"""
    files = [
        path
        for path in generated_paths
        if not any(c in path for c in ("*", "?", "[", "{"))
    ]
    globs = [
        path
        for path in generated_paths
        if any(c in path for c in ("*", "?", "[", "{"))
    ]
    formatted_files = "\n".join(f'    "{path}",' for path in files)
    formatted_globs = "\n".join(f'    "{path}",' for path in globs)
    content = (
        f"{header}\n"
        f"GENERATED_ORIGIN_FILES = [\n{formatted_files}\n]\n\n"
        f"GENERATED_ORIGIN_GLOBS = [\n{formatted_globs}\n]\n"
    )
    output_file = Path(
        os.path.join(output_dir, "copybara_generated_paths.bara.sky")
    )
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(content)


def _locate_android_bp_destination(module: soong_ast.Module) -> str:
    """Returns the appropriate location of the generated Android.bp for the
    specified module. Sometimes it is favourable to relocate the Android.bp to
    a different location other than next to BUILD.gn (eg: rust's BUILD.gn are
    defined in a different directory than the source code).

    :returns the appropriate location for the blueprint
    """
    crate_root_dir = _get_rust_crate_root_directory_from_crate_root(
        getattr(module, 'crate_root', None)
    )
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
    for module_name, module in top_level_blueprint.modules.items():
        if (
            module.type
            in [
                "package",
                "genrule",
                "cc_genrule",
                "java_genrule",
                "cc_preprocess_no_configuration",
            ]
            and not module.allow_rebasing
        ):
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
                f"Found module {module_name} without a build file path."
            )

        rebased_module = _rebase_module(module, android_bp_path)
        if rebased_module:
            if android_bp_path not in blueprints.keys():
                blueprints[android_bp_path] = soong_ast.Blueprint(
                    module.build_file_path
                )
            blueprints[android_bp_path].add_module(rebased_module)
        else:
            # Append to the top-level blueprint.
            blueprints[""].add_module(module)

    for blueprint in blueprints.values():
        if blueprint.get_buildgn_location() in gn2bp_targets.README_MAPPING:
            blueprint.set_readme_location(
                gn2bp_targets.README_MAPPING[blueprint.get_buildgn_location()]
            )
    return blueprints


def main():
    parser = argparse.ArgumentParser(
        description='Generate Android.bp from a GN description.'
    )
    parser.add_argument(
        '--desc',
        help='GN description (e.g., gn desc out --format=json "//*".'
        + 'You can specify multiple --desc options for different target_cpu',
        required=True,
        action='append',
    )
    parser.add_argument(
        '--output_dir',
        required=True,
        help='Directory where the generated Android.bp files should be written',
    )
    parser.add_argument(
        '--build_script_output',
        help='JSON-formatted file containing output of build scripts broken down'
        + 'by architecture.',
        required=True,
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
        help='Targets to include in the blueprint (e.g., "//:perfetto_tests")',
    )

    parser.add_argument(
        '--channel',
        help='The channel this Android.bp generation is being performed for.',
        type=str,
        choices=['tot', 'stable'],
        default='tot',
    )
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
        log.basicConfig(
            format='%(levelname)s:%(funcName)s:%(message)s', level=log.DEBUG
        )

    context = translation_context.TranslationContext(args.channel)
    targets = args.targets or gn2bp_targets.DEFAULT_TARGETS
    build_scripts_output = None
    with open(args.build_script_output) as f:
        build_scripts_output = json.load(f)
    gn = gn_utils.GnParser(
        translation_config.builtin_deps, build_scripts_output
    )
    for desc_file in args.desc:
        with open(desc_file) as f:
            desc = json.load(f)
        for target in targets:
            gn.parse_gn_desc(desc, target)
        for test_target in gn2bp_targets.DEFAULT_TESTS:
            gn.parse_gn_desc(desc, test_target, is_test_target=True)
    gn.propagate_properties()
    top_level_blueprint = create_blueprint_for_targets(
        gn, targets, gn2bp_targets.DEFAULT_TESTS, context
    )

    final_blueprints = _break_down_blueprint(top_level_blueprint)
    if args.license:
        license_modules = create_license_modules(final_blueprints, context)
        for path, module in license_modules.items():
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

    for path, blueprint in final_blueprints.items():
        android_bp_file = Path(
            os.path.join(args.output_dir, path, "Android.bp")
        )
        android_bp_file.parent.mkdir(parents=True, exist_ok=True)
        android_bp_file.write_text(
            "\n".join(
                [header]
                + blueprint.to_string(
                    exclude_gn_targets=translation_config.replace_deps.keys()
                )
            )
        )

    generated_paths = extract_copybara_paths(
        gn, top_level_blueprint, final_blueprints, context
    )
    write_copybara_paths_file(args.output_dir, generated_paths)

    return 0


if __name__ == '__main__':
    sys.exit(main())
