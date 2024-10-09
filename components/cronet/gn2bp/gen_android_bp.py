#!/usr/bin/env python3
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

# This tool translates a collection of BUILD.gn files into a mostly equivalent
# Android.bp file for the Android Soong build system. The input to the tool is a
# JSON description of the GN build definition generated with the following
# command:
#
#   gn desc out --format=json --all-toolchains "//*" > desc.json
#
# The tool is then given a list of GN labels for which to generate Android.bp
# build rules. The dependencies for the GN labels are squashed to the generated
# Android.bp target, except for actions which get their own genrule. Some
# libraries are also mapped to their Android equivalents -- see |builtin_deps|.

import argparse
import json
import logging as log
import operator
import os
import re
import sys
import copy
from typing import List, Dict, Set, Union
from pathlib import Path

import gn_utils
PARENT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir))

sys.path.insert(0, os.path.join(PARENT_ROOT, "license"))
import license_utils

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CRONET_LICENSE_NAME = "external_cronet_license"

# Default targets to translate to the blueprint file.
DEFAULT_TARGETS = [
    "//components/cronet/android:cronet_api_java",
    '//components/cronet/android:cronet',
    '//components/cronet/android:cronet_impl_native_java',
    '//components/cronet/android:cronet_jni_registration_java',
]

DEFAULT_TESTS = [
    '//components/cronet/android:cronet_unittests_android__library',
    '//net:net_unittests__library',
    '//components/cronet/android:cronet_tests',
    '//components/cronet/android:cronet',
    '//components/cronet/android:cronet_javatests',
    '//components/cronet/android:cronet_jni_registration_java',
    '//components/cronet/android:cronet_tests_jni_registration_java',
    '//testing/android/native_test:native_test_java',
    '//net/android:net_test_support_provider_java',
    '//net/android:net_tests_java',
    '//third_party/netty-tcnative:netty-tcnative-so',
    '//third_party/netty4:netty_all_java',
    "//build/rust/tests/test_rust_static_library:test_rust_static_library",  # Added to make sure that rust still compiles
    "//build/rust/tests/test_serde_json_lenient:test_serde_json_lenient__library",
]

EXTRAS_ANDROID_BP_FILE = "Android.extras.bp"

CRONET_API_MODULE_NAME = "cronet_aml_api_java"

# All module names are prefixed with this string to avoid collisions.
module_prefix = 'cronet_aml_'

REMOVE_GEN_JNI_JARJAR_RULES_FILE = ":remove_gen_jni_jarjar_rules"
# Shared libraries which are directly translated to Android system equivalents.
shared_library_allowlist = [
    'android',
    'log',
]

# A dictionary that adds extra content to a specific Android.bp according to the
# provided path.
# The path defined must be relative to the root-repo.
BLUEPRINTS_EXTRAS = {"": ["build = [\"Android.extras.bp\"]"]}

# A dictionary that specifies the relocation of modules from one blueprint to
# another.
# The default format is (relative_path_A -> relative_path_B), this means
# that all targets which should live in relative_path_A/Android.bp will live
# inside relative_path_B/Android.bp.
BLUEPRINTS_MAPPING = {
    # An Android.bp already exists inside boringssl, creating another one will
    # lead to conflicts, add all of the boringssl generated targets to the
    # top-level Android.bp as they are only used for tests.
    "third_party/boringssl": "",
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++": "third_party/libc++",
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++abi": "third_party/libc++abi",
}

# Usually, README.chromium lives next to the BUILD.gn. However, some cases are
# different, this dictionary allows setting a specific README.chromium path
# for a specific BUILD.gn
README_MAPPING = {
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++": "third_party/libc++",
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++abi": "third_party/libc++abi",
}

# Include directories that will be removed from all targets.
include_dirs_denylist = [
    'external/cronet/third_party/zlib/',
]

# Name of the module which settings such as compiler flags for all other
# modules.
cc_defaults_module = module_prefix + 'cc_defaults'

# Name of the java default module for non-test java modules defined in Android.extras.bp
java_framework_defaults_module = 'cronet_aml_java_framework_defaults'

# Location of the project in the Android source tree.
tree_path = 'external/cronet'

# Path for the protobuf sources in the standalone build.
buildtools_protobuf_src = '//buildtools/protobuf/src'

# Location of the protobuf src dir in the Android source tree.
android_protobuf_src = 'external/protobuf/src'

# put all args on a new line for better diffs.
NEWLINE = ' " +\n         "'

# Compiler flags which are passed through to the blueprint.
cflag_allowlist = [
    # needed for zlib:zlib
    "-mpclmul",
    # needed for zlib:zlib
    "-mssse3",
    # needed for zlib:zlib
    "-msse3",
    # needed for zlib:zlib
    "-msse4.2",
    # flags to reduce binary size
    "-O1",
    "-O2",
    "-O3",
    "-Oz",
    "-g1",
    "-g2",
    "-fdata-sections",
    "-ffunction-sections",
    "-fvisibility=hidden",
    "-fvisibility-inlines-hidden",
    "-fstack-protector",
    "-mno-outline",
    "-mno-outline-atomics",
    "-fno-asynchronous-unwind-tables",
    "-fno-unwind-tables",
]

# Linker flags which are passed through to the blueprint.
ldflag_allowlist = [
    # flags to reduce binary size
    "-Wl,--as-needed",
    "-Wl,--gc-sections",
    "-Wl,--icf=all",
]

def get_linker_script_ldflag(script_path):
    return f'-Wl,--script,{tree_path}/{script_path}'

# Additional arguments to apply to Android.bp rules.
additional_args = {
    'cronet_aml_net_third_party_quiche_net_quic_test_tools_proto_gen_headers':
    [('export_include_dirs', {
        "net/third_party/quiche/src",
    })],
    'cronet_aml_net_third_party_quiche_net_quic_test_tools_proto_gen__testing_headers':
    [('export_include_dirs', {
        "net/third_party/quiche/src",
    })],
    'cronet_aml_third_party_quic_trace_quic_trace_proto_gen__testing_headers':
    [('export_include_dirs', {
        "third_party/quic_trace/src",
    })],
    # TODO: fix upstream. Both //base:base and
    # //base/allocator/partition_allocator:partition_alloc do not create a
    # dependency on gtest despite using gtest_prod.h.
    'cronet_aml_base_base': [
        ('header_libs', {
            'libgtest_prod_headers',
        }),
        ('export_header_lib_headers', {
            'libgtest_prod_headers',
        }),
    ],
    'cronet_aml_base_allocator_partition_allocator_partition_alloc': [
        ('header_libs', {
            'libgtest_prod_headers',
        }),
    ],
    # TODO(b/309920629): Remove once upstreamed.
    'cronet_aml_components_cronet_android_cronet_api_java': [
        ('srcs', {
            'components/cronet/android/api/src/org/chromium/net/UploadDataProviders.java',
            'components/cronet/android/api/src/org/chromium/net/apihelpers/UploadDataProviders.java',
        }),
    ],
    'cronet_aml_components_cronet_android_cronet_api_java__testing': [
        ('srcs', {
            'components/cronet/android/api/src/org/chromium/net/UploadDataProviders.java',
            'components/cronet/android/api/src/org/chromium/net/apihelpers/UploadDataProviders.java',
        }),
    ],
    'cronet_aml_components_cronet_android_cronet_javatests__testing': [
        # Needed to @SkipPresubmit annotations
        ('static_libs', {
            'net-tests-utils',
        }),
        # This is necessary because net-tests-utils compiles against private SDK.
        ('sdk_version', ""),
    ],
    'cronet_aml_components_cronet_android_cronet__testing': [
        ('target', ('android_riscv64', {
            'stem': "libmainlinecronet_riscv64"
        })),
        ('comment', """TODO: remove stem for riscv64
// This is essential as there can't be two different modules
// with the same output. We usually got away with that because
// the non-testing Cronet is part of the Tethering APEX and the
// testing Cronet is not part of the Tethering APEX which made them
// look like two different outputs from the build system perspective.
// However, Cronet does not ship to Tethering APEX for RISCV64 which
// raises the conflict. Once we start shipping Cronet for RISCV64,
// this can be removed."""),
    ],
    'cronet_aml_third_party_netty_tcnative_netty_tcnative_so__testing': [
        ('cflags', {"-Wno-error=pointer-bool-conversion"})
    ],
    'cronet_aml_third_party_apache_portable_runtime_apr__testing': [
        ('cflags', {
            "-Wno-incompatible-pointer-types-discards-qualifiers",
        })
    ],
    # TODO(b/324872305): Remove when gn desc expands public_configs and update code to propagate the
    # include_dir from the public_configs
    # We had to add the export_include_dirs for each target because soong generates each header
    # file in a specific directory named after the target.
    'cronet_aml_base_allocator_partition_allocator_src_partition_alloc_chromecast_buildflags':
    [('export_include_dirs', {
        "base/allocator/partition_allocator/src/",
    })],
    'cronet_aml_base_allocator_partition_allocator_src_partition_alloc_chromecast_buildflags__testing':
    [('export_include_dirs', {
        "base/allocator/partition_allocator/src/",
    })],
    'cronet_aml_base_allocator_partition_allocator_src_partition_alloc_chromeos_buildflags':
    [('export_include_dirs', {
        "base/allocator/partition_allocator/src/",
    })],
    'cronet_aml_base_allocator_partition_allocator_src_partition_alloc_chromeos_buildflags__testing':
    [('export_include_dirs', {
        "base/allocator/partition_allocator/src/",
    })],
    'cronet_aml_base_allocator_partition_allocator_src_partition_alloc_debugging_buildflags':
    [('export_include_dirs', {
        "base/allocator/partition_allocator/src/",
    })],
    'cronet_aml_base_allocator_partition_allocator_src_partition_alloc_debugging_buildflags__testing':
    [('export_include_dirs', {
        "base/allocator/partition_allocator/src/",
    })],
    'cronet_aml_base_allocator_partition_allocator_src_partition_alloc_partition_alloc_buildflags':
    [('export_include_dirs', {
        ".",
        "base/allocator/partition_allocator/src/",
    })],
    'cronet_aml_base_allocator_partition_allocator_src_partition_alloc_partition_alloc_buildflags__testing':
    [('export_include_dirs', {
        ".",
        "base/allocator/partition_allocator/src/",
    })],
    'cronet_aml_base_allocator_partition_allocator_src_partition_alloc_raw_ptr_buildflags':
    [('export_include_dirs', {
        "base/allocator/partition_allocator/src/",
    })],
    'cronet_aml_base_allocator_partition_allocator_src_partition_alloc_raw_ptr_buildflags__testing':
    [('export_include_dirs', {
        "base/allocator/partition_allocator/src/",
    })],
    'cronet_aml_base_base_java_test_support__testing': [
        ('errorprone', ('javacflags', {
            "-Xep:ReturnValueIgnored:WARN",
        }))
    ]
    # end export_include_dir.
}

_FEATURE_REGEX = "feature=\\\"(.+)\\\""
_RUST_FLAGS_TO_REMOVE = [
    "--target",  # Added by Soong
    "--color",  # Added by Soong.
    "--edition",  # Added to the appropriate field, must be removed from flags.
    "--sysroot",  # Use AOSP's stdlib so we don't need any hacks for sysroot.
    "-Cembed-bitcode=no",  # Not compatible with Thin-LTO which is added by Soong.
    "--cfg",  # Added to the appropriate field.
    "--extern",  # Soong automatically adds that for us when we use proc_macro
    "@",  # Used by build_script outputs to have rustc load flags from a file.
    "-Z",  # Those are unstable features, completely remove those.
]


def always_disable(module, arch):
    return None

def enable_zlib(module, arch):
    # Requires crrev/c/4109079
    if arch == 'common':
        module.shared_libs.add('libz')
    else:
        module.target[arch].shared_libs.add('libz')

def enable_boringssl(module, arch):
    # Do not add boringssl targets to cc_genrules. This happens, because protobuf targets are
    # originally static_libraries, but later get converted to a cc_genrule.
    if module.is_genrule(): return
    # Lets keep statically linking BoringSSL for testing target for now. This should be fixed.
    if module.name.endswith(gn_utils.TESTING_SUFFIX): return
    if arch == 'common':
        shared_libs = module.shared_libs
    else:
        shared_libs = module.target[arch].shared_libs
    shared_libs.add('//external/cronet/third_party/boringssl:libcrypto')
    shared_libs.add('//external/cronet/third_party/boringssl:libssl')
    shared_libs.add('//external/cronet/third_party/boringssl:libpki')

def add_androidx_experimental_java_deps(module, arch):
    module.libs.add("androidx.annotation_annotation-experimental")

def add_androidx_annotation_java_deps(module, arch):
    module.libs.add("androidx.annotation_annotation")

def add_protobuf_lite_runtime_java_deps(module, arch):
    module.static_libs.add("libprotobuf-java-lite")

def add_androidx_core_java_deps(module, arch):
    module.libs.add("androidx.core_core")

def add_jsr305_java_deps(module, arch):
    module.libs.add("jsr305")

def add_errorprone_annotation_java_deps(module, arch):
    module.libs.add("error_prone_annotations")

def add_androidx_collection_java_deps(module, arch):
    module.libs.add("androidx.collection_collection")

def add_junit_java_deps(module, arch):
    module.static_libs.add("junit")

def add_truth_java_deps(module, arch):
    module.static_libs.add("truth")

def add_hamcrest_java_deps(module, arch):
    module.static_libs.add("hamcrest-library")
    module.static_libs.add("hamcrest")

def add_mockito_java_deps(module, arch):
    module.static_libs.add("mockito")

def add_guava_java_deps(module, arch):
    module.static_libs.add("guava")

def add_androidx_junit_java_deps(module, arch):
    module.static_libs.add("androidx.test.ext.junit")

def add_androidx_test_runner_java_deps(module, arch):
    module.static_libs.add("androidx.test.runner")

def add_androidx_test_rules_java_deps(module, arch):
    module.static_libs.add("androidx.test.rules")

def add_android_test_base_java_deps(module, arch):
    module.libs.add("android.test.base")

def add_accessibility_test_framework_java_deps(module, arch):
    # BaseActivityTestRule.java depends on this but BaseActivityTestRule.java is not used in aosp.
    pass

def add_espresso_java_deps(module, arch):
    module.static_libs.add("androidx.test.espresso.contrib")

def add_android_test_mock_java_deps(module, arch):
    module.libs.add("android.test.mock.stubs")

def add_androidx_multidex_java_deps(module, arch):
    # Androidx-multidex is disabled on unbundled branches.
    pass

def add_androidx_test_monitor_java_deps(module, arch):
    module.libs.add("androidx.test.monitor")

def add_androidx_ui_automator_java_deps(module, arch):
    module.static_libs.add("androidx.test.uiautomator_uiautomator")

def add_androidx_test_annotation_java_deps(module, arch):
    module.static_libs.add("androidx.test.rules")

def add_androidx_test_core_java_deps(module, arch):
    module.static_libs.add("androidx.test.core")

def add_androidx_activity_activity(module, arch):
    module.static_libs.add("androidx.activity_activity")

def add_androidx_fragment_fragment(module, arch):
    module.static_libs.add("androidx.fragment_fragment")

# Android equivalents for third-party libraries that the upstream project
# depends on. This will be applied to normal and testing targets.
_builtin_deps = {
    '//buildtools/third_party/libunwind:libunwind':
    always_disable,
    # This is a binary module that generates C++ binding files, Skip this
    # dependency completely as we construct the modules differently.
    '//third_party/rust/cxxbridge_cmd/v1:cxxbridge':
    always_disable,
    '//net/data/ssl/chrome_root_store:gen_root_store_inc':
    always_disable,
    '//third_party/zstd:headers':
    always_disable,
    '//testing/buildbot/filters:base_unittests_filters':
    always_disable,
    '//testing/buildbot/filters:net_unittests_filters':
    always_disable,
    '//third_party/boringssl/src/third_party/fiat:fiat_license':
    always_disable,
    '//net/tools/root_store_tool:root_store_tool':
    always_disable,
    '//third_party/zlib:zlib':
    enable_zlib,
    '//third_party/androidx:androidx_annotation_annotation_java':
    add_androidx_annotation_java_deps,
    '//third_party/android_deps:protobuf_lite_runtime_java':
    add_protobuf_lite_runtime_java_deps,
    '//third_party/androidx:androidx_annotation_annotation_experimental_java':
    add_androidx_experimental_java_deps,
    '//third_party/androidx:androidx_core_core_java':
    add_androidx_core_java_deps,
    '//third_party/android_deps:com_google_code_findbugs_jsr305_java':
    add_jsr305_java_deps,
    '//third_party/android_deps:com_google_errorprone_error_prone_annotations_java':
    add_errorprone_annotation_java_deps,
    '//third_party/androidx:androidx_collection_collection_java':
    add_androidx_collection_java_deps,
    '//third_party/junit:junit':
    add_junit_java_deps,
    '//third_party/google-truth:google_truth_java':
    add_truth_java_deps,
    '//third_party/hamcrest:hamcrest_core_java':
    add_hamcrest_java_deps,
    '//third_party/mockito:mockito_java':
    add_mockito_java_deps,
    '//third_party/android_deps:guava_android_java':
    add_guava_java_deps,
    '//third_party/androidx:androidx_test_ext_junit_java':
    add_androidx_junit_java_deps,
    '//third_party/androidx:androidx_test_runner_java':
    add_androidx_test_runner_java_deps,
    '//third_party/android_sdk:android_test_base_java':
    add_android_test_base_java_deps,
    '//third_party/accessibility_test_framework:accessibility_test_framework_java':
    add_accessibility_test_framework_java_deps,
    '//third_party/accessibility_test_framework:accessibility_core_java':
    add_accessibility_test_framework_java_deps,
    '//third_party/android_deps:espresso_java':
    add_espresso_java_deps,
    '//third_party/android_sdk:android_test_mock_java':
    add_android_test_mock_java_deps,
    '//third_party/androidx:androidx_multidex_multidex_java':
    add_androidx_multidex_java_deps,
    '//third_party/androidx:androidx_test_monitor_java':
    add_androidx_test_monitor_java_deps,
    '//third_party/androidx:androidx_test_annotation_java':
    add_androidx_test_annotation_java_deps,
    '//third_party/androidx:androidx_test_core_java':
    add_androidx_test_core_java_deps,
    '//third_party/androidx:androidx_test_uiautomator_uiautomator_java':
    add_androidx_ui_automator_java_deps,
    '//third_party/hamcrest:hamcrest_java':
    add_hamcrest_java_deps,
    '//third_party/androidx:androidx_activity_activity_java':
    add_androidx_activity_activity,
    '//third_party/androidx:androidx_fragment_fragment_java':
    add_androidx_fragment_fragment,
    '//third_party/androidx:androidx_test_rules_java':
    add_androidx_test_rules_java_deps,
}
builtin_deps = {
    "{}{}".format(key, suffix): value
    for key, value in _builtin_deps.items()
    for suffix in ["", gn_utils.TESTING_SUFFIX]
}

# Same as _builtin_deps but will only apply what is explicitly specified.
builtin_deps.update({
    '//third_party/boringssl:boringssl': enable_boringssl,
    '//third_party/boringssl:boringssl_asm':
    # Due to FIPS requirements, downstream BoringSSL has a different "shape" than upstream's.
    # We're guaranteed that if X depends on :boringssl it will also depend on :boringssl_asm.
    # Hence, always drop :boringssl_asm and handle the translation entirely in :boringssl.
    always_disable,
})

# Name of tethering apex module
tethering_apex = "com.android.tethering"

# Name of cronet api target
java_api_target_name = "//components/cronet/android:cronet_api_java"

# Visibility set for package default
package_default_visibility = ":__subpackages__"

# Visibility set for modules used from Connectivity and within external/cronet
root_modules_visibility = {
    "//packages/modules/Connectivity:__subpackages__",
    "//external/cronet:__subpackages__"
}

# ----------------------------------------------------------------------------
# End of configuration.
# ----------------------------------------------------------------------------

def write_blueprint_key_value(output, name, value, sort=True):
    """Writes a Blueprint key-value pair to the output"""

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
    if isinstance(value, list):
        output.append('    %s: [' % name)
        for item in sorted(value) if sort else value:
            output.append('        "%s",' % item)
        output.append('    ],')
        return
    if isinstance(value, Module.Target):
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
    output.append('    %s: "%s",' % (name, value))


class Module(object):
    """A single module (e.g., cc_binary, cc_test) in a blueprint."""

    class Target(object):
        """A target-scoped part of a module"""

        def __init__(self, name):
            self.name = name
            self.srcs = set()
            self.shared_libs = set()
            self.static_libs = set()
            self.whole_static_libs = set()
            self.header_libs = set()
            self.cflags = set()
            self.stl = None
            self.cppflags = set()
            self.include_dirs = set()
            self.generated_headers = set()
            self.export_generated_headers = set()
            self.ldflags = set()
            self.compile_multilib = None
            self.stem = ""
            self.edition = ""
            self.features = set()
            self.cfgs = set()
            self.flags = list()
            self.rustlibs = set()
            self.proc_macros = set()
            if name == 'host':
                self.compile_multilib = '64'

        def to_string(self, output):
            nested_out = []
            self._output_field(nested_out, 'srcs')
            self._output_field(nested_out, 'shared_libs')
            self._output_field(nested_out, 'static_libs')
            self._output_field(nested_out, 'whole_static_libs')
            self._output_field(nested_out, 'header_libs')
            self._output_field(nested_out, 'cflags')
            self._output_field(nested_out, 'stl')
            self._output_field(nested_out, 'cppflags')
            self._output_field(nested_out, 'include_dirs')
            self._output_field(nested_out, 'generated_headers')
            self._output_field(nested_out, 'export_generated_headers')
            self._output_field(nested_out, 'ldflags')
            self._output_field(nested_out, 'stem')
            self._output_field(nested_out, "edition")
            self._output_field(nested_out, 'cfgs')
            self._output_field(nested_out, 'features')
            self._output_field(nested_out, 'flags', False)
            self._output_field(nested_out, 'rustlibs')
            self._output_field(nested_out, 'proc_macros')

            if nested_out:
                # This is added here to make sure it doesn't add a `host` arch-specific module just for
                # `compile_multilib` flag.
                self._output_field(nested_out, 'compile_multilib')
                output.append('    %s: {' % self.name)
                for line in nested_out:
                    output.append('    %s' % line)
                output.append('    },')

        def _output_field(self, output, name, sort=True):
            value = getattr(self, name)
            return write_blueprint_key_value(output, name, value, sort)

    def __init__(self, mod_type, name, gn_target):
        self.type = mod_type
        self.gn_target = gn_target
        self.name = name
        self.srcs = set()
        self.comment = 'GN: ' + gn_target
        self.shared_libs = set()
        self.static_libs = set()
        self.whole_static_libs = set()
        self.tools = set()
        self.cmd = None
        self.host_supported = False
        self.device_supported = True
        self.init_rc = set()
        self.out = set()
        self.export_include_dirs = set()
        self.generated_headers = set()
        self.export_generated_headers = set()
        self.export_static_lib_headers = set()
        self.export_header_lib_headers = set()
        self.defaults = set()
        self.cflags = set()
        self.include_dirs = set()
        self.local_include_dirs = set()
        self.header_libs = set()
        self.tool_files = set()
        # target contains a dict of Targets indexed by os_arch.
        # example: { 'android_x86': Target('android_x86')
        self.target = dict()
        self.target['android'] = self.Target('android')
        self.target['android_x86'] = self.Target('android_x86')
        self.target['android_x86_64'] = self.Target('android_x86_64')
        self.target['android_arm'] = self.Target('android_arm')
        self.target['android_arm64'] = self.Target('android_arm64')
        self.target['android_riscv64'] = self.Target('android_riscv64')
        self.target['host'] = self.Target('host')
        self.target['glibc'] = self.Target('glibc')
        self.stl = None
        self.cpp_std = None
        self.strip = dict()
        self.data = set()
        self.apex_available = set()
        self.min_sdk_version = None
        self.proto = dict()
        self.linker_scripts = set()
        self.ldflags = set()
        # The genrule_XXX below are properties that must to be propagated back
        # on the module(s) that depend on the genrule.
        self.genrule_headers = set()
        self.genrule_srcs = set()
        self.genrule_shared_libs = set()
        self.genrule_header_libs = set()
        self.version_script = None
        self.test_suites = set()
        self.test_config = None
        self.cppflags = set()
        self.rtti = False
        # Name of the output. Used for setting .so file name for libcronet
        self.libs = set()
        self.stem = None
        self.compile_multilib = None
        self.aidl = dict()
        self.plugins = set()
        self.processor_class = None
        self.sdk_version = None
        self.javacflags = set()
        self.c_std = None
        self.default_applicable_licenses = set()
        self.default_visibility = []
        self.visibility = set()
        self.gn_type = None
        self.jarjar_rules = ""
        self.jars = set()
        self.build_file_path = None
        self.include_build_directory = None
        self.allow_rebasing = False
        self.license_kinds = set()
        self.license_text = set()
        self.errorprone = dict()
        self.crate_name = None
        # Should be arch-dependant
        self.crate_root = None
        self.edition = None
        self.rustlibs = set()
        self.proc_macros = set()

    def to_string(self, output):
        if self.comment:
            output.append('// %s' % self.comment)
        output.append('%s {' % self.type)
        self._output_field(output, 'name')
        self._output_field(output, 'srcs')
        self._output_field(output, 'shared_libs')
        self._output_field(output, 'static_libs')
        self._output_field(output, 'whole_static_libs')
        self._output_field(output, 'tools')
        self._output_field(output, 'cmd', sort=False)
        if self.host_supported:
            self._output_field(output, 'host_supported')
        if not self.device_supported:
            self._output_field(output, 'device_supported')
        self._output_field(output, 'init_rc')
        self._output_field(output, 'out')
        self._output_field(output, 'export_include_dirs')
        self._output_field(output, 'generated_headers')
        self._output_field(output, 'export_generated_headers')
        self._output_field(output, 'export_static_lib_headers')
        self._output_field(output, 'export_header_lib_headers')
        self._output_field(output, 'defaults')
        self._output_field(output, 'cflags')
        self._output_field(output, 'include_dirs')
        self._output_field(output, 'local_include_dirs')
        self._output_field(output, 'header_libs')
        self._output_field(output, 'strip')
        self._output_field(output, 'tool_files')
        self._output_field(output, 'data')
        self._output_field(output, 'stl')
        self._output_field(output, 'cpp_std')
        self._output_field(output, 'apex_available')
        self._output_field(output, 'min_sdk_version')
        self._output_field(output, 'version_script')
        self._output_field(output, 'test_suites')
        self._output_field(output, 'test_config')
        self._output_field(output, 'proto')
        self._output_field(output, 'linker_scripts')
        self._output_field(output, 'ldflags')
        self._output_field(output, 'cppflags')
        self._output_field(output, 'libs')
        self._output_field(output, 'stem')
        self._output_field(output, 'compile_multilib')
        self._output_field(output, 'aidl')
        self._output_field(output, 'plugins')
        self._output_field(output, 'processor_class')
        self._output_field(output, 'sdk_version')
        self._output_field(output, 'javacflags')
        self._output_field(output, 'c_std')
        self._output_field(output, 'default_applicable_licenses')
        self._output_field(output, 'default_visibility')
        self._output_field(output, 'visibility')
        self._output_field(output, 'jarjar_rules')
        self._output_field(output, 'jars')
        self._output_field(output, 'include_build_directory')
        self._output_field(output, 'license_text')
        self._output_field(output, "license_kinds")
        self._output_field(output, "errorprone")
        self._output_field(output, 'crate_name')
        self._output_field(output, 'crate_root')
        self._output_field(output, 'rustlibs')
        self._output_field(output, 'proc_macros')
        if self.rtti:
            self._output_field(output, 'rtti')

        target_out = []
        for arch, target in sorted(self.target.items()):
            # _output_field calls getattr(self, arch).
            setattr(self, arch, target)
            self._output_field(target_out, arch)

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
        elif self.type == 'cc_binary_host':
            raise Exception(
                'Adding Android shared lib for host tool is unsupported')
        elif self.host_supported:
            self.target['android'].shared_libs.add(lib)
        else:
            self.shared_libs.add(lib)

    def is_test(self):
        if gn_utils.TESTING_SUFFIX in self.name:
            name_without_prefix = self.name[:self.name.find(gn_utils.
                                                            TESTING_SUFFIX)]
            return any([
                name_without_prefix == label_to_module_name(target)
                for target in DEFAULT_TESTS
            ])
        return False

    def _output_field(self, output, name, sort=True):
        value = getattr(self, name)
        return write_blueprint_key_value(output, name, value, sort)

    def is_compiled(self):
        return self.type not in ('cc_genrule', 'filegroup', 'java_genrule')

    def is_genrule(self):
        return self.type == "cc_genrule"

    def has_input_files(self):
        if self.type in ["java_library", "java_import"]:
            return True
        if len(self.srcs) > 0:
            return True
        if any([len(target.srcs) > 0 for target in self.target.values()]):
            return True
        # Allow cc_static_library with export_generated_headers as those are crucial for
        # the depending modules
        return len(self.export_generated_headers) > 0


class Blueprint(object):
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

    def to_string(self):
        ret = []
        if self._package_module:
            self._package_module.to_string(ret)
        if self._license_module:
            self._license_module.to_string(ret)
        for m in sorted(self.modules.values(), key=lambda m: m.name):
            if m.type != "cc_library_static" or m.has_input_files():
                # Don't print cc_library_static with empty srcs. These attributes are already
                # propagated up the tree. Printing them messes the presubmits because
                # every module is compiled while those targets are not reachable in
                # a normal compilation path.
                m.to_string(ret)
        return ret


def label_to_module_name(label):
    """Turn a GN label (e.g., //:perfetto_tests) into a module name."""
    module = re.sub(r'^//:?', '', label)
    module = re.sub(r'[^a-zA-Z0-9_]', '_', module)

    if not module.startswith(module_prefix):
        return module_prefix + module
    return module


def get_target_name(label):
    return label[label.find(":") + 1:]


def is_supported_source_file(name):
    """Returns True if |name| can appear in a 'srcs' list."""
    return os.path.splitext(name)[1] in [
        '.c', '.cc', '.cpp', '.java', '.proto', '.S', '.aidl', '.rs'
    ]


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


def _set_rust_flags(module: Module.Target, rust_flags: List[str],
                    arch_name: str) -> None:
    rust_flags_dict = normalize_rust_flags(rust_flags)
    if "--edition" in rust_flags_dict:
        module.edition = list(rust_flags_dict["--edition"])[0]

    for cfg in rust_flags_dict.get("--cfg", set()):
        feature_regex = re.match(_FEATURE_REGEX, cfg)
        if feature_regex:
            module.features.add(feature_regex.group(1))
        else:
            module.cfgs.add(cfg.replace("\"", "\\\""))

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
        if not any([
                pre_filter_flag.startswith(restricted_flag)
                for restricted_flag in flags_to_remove
        ]):
            module.flags.append(pre_filter_flag)


def get_protoc_module_name(gn):
    protoc_gn_target_name = gn.get_target('//third_party/protobuf:protoc').name
    return label_to_module_name(protoc_gn_target_name)


def create_rust_cxx_module(blueprint, target):
    """Generate genrules for a CXX GN target

    GN actions are used to dynamically generate files during the build. The
    Soong equivalent is a genrule. Currently, Chromium GN targets generates
    both .cc and .h files in the same target, we have to split this up to be
    compatible with Soong.

    CXX bridge binary is used from AOSP instead of compiling Chromium's CXX bridge.

    Args:
        blueprint: Blueprint instance which is being generated.
        target: gn_utils.Target object.

    Returns:
        The source_genrule module.
  """
    header_genrule = Module("cc_genrule",
                            label_to_module_name(target.name) + "_header",
                            target.name)
    header_genrule.tools = {"cxxbridge"}
    header_genrule.cmd = "$(location cxxbridge) $(in) --header > $(out)"
    header_genrule.srcs = set(
        [gn_utils.label_to_path(src) for src in target.sources])
    # The output of the cc_genrule is the input + ".h" suffix, this is because
    # the input to a CXX genrule is just one source file.
    header_genrule.out = set(
        [f"{gn_utils.label_to_path(out)}.h" for out in target.sources])

    cc_genrule = Module("cc_genrule", label_to_module_name(target.name),
                        target.name)
    cc_genrule.tools = {"cxxbridge"}
    cc_genrule.cmd = "$(location cxxbridge) $(in) > $(out)"
    cc_genrule.srcs = set(
        [gn_utils.label_to_path(src) for src in target.sources])
    cc_genrule.genrule_srcs = {f":{cc_genrule.name}"}
    # The output of the cc_genrule is the input + ".cc" suffix, this is because
    # the input to a CXX genrule is just one source file.
    cc_genrule.out = set(
        [f"{gn_utils.label_to_path(out)}.cc" for out in target.sources])

    cc_genrule.genrule_headers.add(header_genrule.name)
    blueprint.add_module(cc_genrule)
    blueprint.add_module(header_genrule)
    return cc_genrule


def create_proto_modules(blueprint, gn, target):
    """Generate genrules for a proto GN target.

    GN actions are used to dynamically generate files during the build. The
    Soong equivalent is a genrule. This function turns a specific kind of
    genrule which turns .proto files into source and header files into a pair
    equivalent genrules.

    Args:
        blueprint: Blueprint instance which is being generated.
        target: gn_utils.Target object.

    Returns:
        The source_genrule module.
    """
    assert (target.type == 'proto_library')

    protoc_module_name = get_protoc_module_name(gn)
    tools = {protoc_module_name}
    cpp_out_dir = '$(genDir)/%s/' % (target.proto_in_dir)
    target_module_name = label_to_module_name(target.name)

    # In GN builds the proto path is always relative to the output directory
    # (out/tmp.xxx).
    cmd = ['$(location %s)' % protoc_module_name]
    cmd += ['--proto_path=%s/%s' % (tree_path, target.proto_in_dir)]

    for proto_path in target.proto_paths:
        cmd += [f'--proto_path={tree_path}/{proto_path}']
    if buildtools_protobuf_src in target.proto_paths:
        cmd += ['--proto_path=%s' % android_protobuf_src]

    # We don't generate any targets for source_set proto modules because
    # they will be inlined into other modules if required.
    if target.proto_plugin == 'source_set':
        return None

    # Descriptor targets only generate a single target.
    if target.proto_plugin == 'descriptor':
        out = '{}.bin'.format(target_module_name)

        cmd += ['--descriptor_set_out=$(out)']
        cmd += ['$(in)']

        descriptor_module = Module('cc_genrule', target_module_name,
                                   target.name)
        descriptor_module.cmd = ' '.join(cmd)
        descriptor_module.out = [out]
        descriptor_module.tools = tools
        blueprint.add_module(descriptor_module)

        # Recursively extract the .proto files of all the dependencies and
        # add them to srcs.
        descriptor_module.srcs.update(
            gn_utils.label_to_path(src) for src in target.sources)
        for dep in target.proto_deps:
            current_target = gn.get_target(dep)
            descriptor_module.srcs.update(
                gn_utils.label_to_path(src) for src in current_target.sources)

        return descriptor_module

    # We create two genrules for each proto target: one for the headers and
    # another for the sources. This is because the module that depends on the
    # generated files needs to declare two different types of dependencies --
    # source files in 'srcs' and headers in 'generated_headers' -- and it's not
    # valid to generate .h files from a source dependency and vice versa.
    source_module_name = target_module_name
    source_module = Module('cc_genrule', source_module_name, target.name)
    blueprint.add_module(source_module)
    source_module.srcs.update(
        gn_utils.label_to_path(src) for src in target.sources)

    header_module = Module('cc_genrule', source_module_name + '_headers',
                           target.name)
    blueprint.add_module(header_module)
    header_module.srcs = set(source_module.srcs)

    header_module.export_include_dirs = {'.', 'protos'}
    # Since the .cc file and .h get created by a different gerule target, they
    # are not put in the same intermediate path, so local includes do not work
    # without explictily exporting the include dir.
    header_module.export_include_dirs.add(target.proto_in_dir)

    # This function does not return header_module so setting apex_available attribute here.
    header_module.apex_available.add(tethering_apex)

    source_module.genrule_srcs.add(':' + source_module.name)
    source_module.genrule_headers.add(header_module.name)

    if target.proto_plugin == 'proto':
        suffixes = ['pb']
        source_module.genrule_shared_libs.add('libprotobuf-cpp-lite')
        cmd += ['--cpp_out=lite=true:' + cpp_out_dir]
    else:
        raise Exception('Unsupported proto plugin: %s' % target.proto_plugin)

    cmd += ['$(in)']
    source_module.cmd = ' '.join(cmd)
    header_module.cmd = source_module.cmd
    source_module.tools = tools
    header_module.tools = tools

    for sfx in suffixes:
        source_module.out.update('%s' % src.replace('.proto', '.%s.cc' % sfx)
                                 for src in source_module.srcs)
        header_module.out.update('%s' % src.replace('.proto', '.%s.h' % sfx)
                                 for src in header_module.srcs)
    # This has proto files that will be used for reference resolution
    # but not compiled into cpp files. These additional sources has no output.
    proto_data_sources = sorted([
        gn_utils.label_to_path(proto_src) for proto_src in target.inputs
        if proto_src.endswith(".proto")
    ])
    source_module.srcs.update(proto_data_sources)
    header_module.srcs.update(proto_data_sources)

    # Allow rebasing proto genrules according to their proper path.
    source_module.allow_rebasing = True
    header_module.allow_rebasing = True
    header_module.build_file_path = target.build_file_path
    source_module.build_file_path = target.build_file_path
    return source_module


def create_gcc_preprocess_modules(blueprint, target):
    # gcc_preprocess.py internally execute host gcc which is not allowed in genrule.
    # So, this function create multiple modules and realize equivalent processing
    # TODO: Consider to support gcc_preprocess.py in different way
    # It's not great to have genrule and cc_object in the dependency from java_library
    assert (len(target.sources) == 1)
    source = list(target.sources)[0]
    assert (Path(source).suffix == '.template')
    stem = Path(source).stem

    bp_module_name = label_to_module_name(target.name)

    # Rename .template to .cc since cc_object does not accept .template file as srcs
    rename_module = Module('genrule', bp_module_name + '_rename', target.name)
    rename_module.srcs.add(gn_utils.label_to_path(source))
    rename_module.out.add(stem + '.cc')
    rename_module.cmd = 'cp $(in) $(out)'
    blueprint.add_module(rename_module)

    # Preprocess template file and generates java file
    preprocess_module = Module('cc_object', bp_module_name + '_preprocess',
                               target.name)
    # -E: stop after preprocessing.
    # -P: disable line markers, i.e. '#line 309'
    preprocess_module.cflags.update(['-E', '-P', '-DANDROID'])
    preprocess_module.srcs.add(':' + rename_module.name)
    defines = [
        '-D' + target.args[i + 1] for i, arg in enumerate(target.args)
        if arg == '--define'
    ]
    preprocess_module.cflags.update(defines)
    # HACK: Specifying compile_multilib to build cc_object only once.
    # Without this, soong complain to genrule that depends on cc_object when built for 64bit target.
    # It seems this is because cc object is a module with per-architecture variants and genrule is a
    # module with default variant. For 64bit target, cc_object is built multiple times for 32/64bit
    # modes and genrule doesn't know which one to depend on.
    preprocess_module.compile_multilib = 'first'
    blueprint.add_module(preprocess_module)

    # Generates srcjar using soong_zip
    module = Module('genrule', bp_module_name, target.name)
    module.srcs.add(':' + preprocess_module.name)
    module.out.add(stem + '.srcjar')
    module.cmd = NEWLINE.join([
        f'cp $(in) $(genDir)/{stem}.java &&',
        f'$(location soong_zip) -o $(out) -srcjar -C $(genDir) -f $(genDir)/{stem}.java'
    ])
    module.tools.add('soong_zip')
    blueprint.add_module(module)
    return module


class BaseActionSanitizer():

    def __init__(self, target, arch):
        # Just to be on the safe side, create a deep-copy.
        self.target = copy.deepcopy(target)
        if arch:
            # Merge arch specific attributes
            self.target.sources |= arch.sources
            self.target.inputs |= arch.inputs
            self.target.outputs |= arch.outputs
            self.target.script = self.target.script or arch.script
            self.target.args = self.target.args or arch.args
            self.target.response_file_contents = \
              self.target.response_file_contents or arch.response_file_contents
        self.target.args = self._normalize_args()

    def get_name(self):
        return label_to_module_name(self.target.name)

    def _normalize_args(self):
        # Convert ['--param=value'] to ['--param', 'value'] for consistency.
        # Escape quotations.
        normalized_args = []
        for arg in self.target.args:
            arg = arg.replace('"', r'\"')
            if arg.startswith('-'):
                normalized_args.extend(arg.split('='))
            else:
                normalized_args.append(arg)
        return normalized_args

    # There are three types of args:
    # - flags (--flag)
    # - value args (--arg value)
    # - list args (--arg value1 --arg value2)
    # value args have exactly one arg value pair and list args have one or more arg value pairs.
    # Note that the set of list args contains the set of value args.
    # This is because list and value args are identical when the list args has only one arg value pair
    # Some functions provide special implementations for each type, while others
    # work on all of them.
    def _has_arg(self, arg):
        return arg in self.target.args

    def _get_arg_indices(self, target_arg):
        return [
            i for i, arg in enumerate(self.target.args) if arg == target_arg
        ]

    # Whether an arg value pair appears once or more times
    def _is_list_arg(self, arg):
        indices = self._get_arg_indices(arg)
        return len(indices) > 0 and all(
            [not self.target.args[i + 1].startswith('--') for i in indices])

    def _update_list_arg(self, arg, func, throw_if_absent=True):
        if self._should_fail_silently(arg, throw_if_absent):
            return
        assert (self._is_list_arg(arg))
        indices = self._get_arg_indices(arg)
        for i in indices:
            self._set_arg_at(i + 1, func(self.target.args[i + 1]))

    # Whether an arg value pair appears exactly once
    def _is_value_arg(self, arg):
        return operator.countOf(self.target.args,
                                arg) == 1 and self._is_list_arg(arg)

    def _get_value_arg(self, arg):
        assert (self._is_value_arg(arg))
        i = self.target.args.index(arg)
        return self.target.args[i + 1]

    # used to check whether a function call should cause an error when an arg is
    # missing.
    def _should_fail_silently(self, arg, throw_if_absent):
        return not throw_if_absent and not self._has_arg(arg)

    def _set_value_arg(self, arg, value, throw_if_absent=True):
        if self._should_fail_silently(arg, throw_if_absent):
            return
        assert (self._is_value_arg(arg))
        i = self.target.args.index(arg)
        self.target.args[i + 1] = value

    def _update_value_arg(self, arg, func, throw_if_absent=True):
        if self._should_fail_silently(arg, throw_if_absent):
            return
        self._set_value_arg(arg, func(self._get_value_arg(arg)))

    def _set_arg_at(self, position, value):
        self.target.args[position] = value

    def _update_arg_at(self, position, func):
        self.target.args[position] = func(self.target.args[position])

    def _delete_value_arg(self, arg, throw_if_absent=True):
        if self._should_fail_silently(arg, throw_if_absent):
            return
        assert (self._is_value_arg(arg))
        i = self.target.args.index(arg)
        self.target.args.pop(i)
        self.target.args.pop(i)

    def _append_arg(self, arg, value):
        self.target.args.append(arg)
        self.target.args.append(value)

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

    # Iterate through all the args and apply function
    def _update_all_args(self, func):
        self.target.args = [func(arg) for arg in self.target.args]

    def get_pre_cmd(self):
        pre_cmd = []
        out_dirs = [
            out[:out.rfind("/")] for out in self.target.outputs if "/" in out
        ]
        # Sort the list to make the output deterministic.
        for out_dir in sorted(set(out_dirs)):
            pre_cmd.append("mkdir -p $(genDir)/{} && ".format(out_dir))
        return NEWLINE.join(pre_cmd)

    def get_base_cmd(self):
        arg_string = NEWLINE.join(self.target.args)
        cmd = '$(location %s) %s' % (gn_utils.label_to_path(
            self.target.script), arg_string)

        if self.use_response_file:
            # Pipe response file contents into script
            cmd = 'echo \'%s\' |%s%s' % (self.target.response_file_contents,
                                         NEWLINE, cmd)
        return cmd

    def get_cmd(self):
        return self.get_pre_cmd() + self.get_base_cmd()

    def get_outputs(self):
        return self.target.outputs

    def get_srcs(self):
        # gn treats inputs and sources for actions equally.
        # soong only supports source files inside srcs, non-source files are added as
        # tool_files dependency.
        files = self.target.sources.union(self.target.inputs)
        return {
            gn_utils.label_to_path(file)
            for file in files if is_supported_source_file(file)
        }

    def get_tools(self):
        return set()

    def get_tool_files(self):
        # gn treats inputs and sources for actions equally.
        # soong only supports source files inside srcs, non-source files are added as
        # tool_files dependency.
        files = self.target.sources.union(self.target.inputs)
        tool_files = {
            gn_utils.label_to_path(file)
            # Files that starts with "out/" are usually an output of another action.
            # This is under the assumption that we generate the desc files in an
            # out/ directory usually.
            for file in files if not is_supported_source_file(file)
            and not file.startswith("//out/")
        }
        tool_files.add(gn_utils.label_to_path(self.target.script))
        return tool_files

    def _sanitize_args(self):
        # Handle passing parameters via response file by piping them into the script
        # and reading them from /dev/stdin.

        self.use_response_file = gn_utils.RESPONSE_FILE in self.target.args
        if self.use_response_file:
            # Replace {{response_file_contents}} with /dev/stdin
            self.target.args = [
                '/dev/stdin' if it == gn_utils.RESPONSE_FILE else it
                for it in self.target.args
            ]

    def _sanitize_inputs(self):
        pass

    def get_deps(self):
        return self.target.deps

    def sanitize(self):
        self._sanitize_args()
        self._sanitize_inputs()

    # Whether this target generates header files
    def is_header_generated(self):
        return any(
            os.path.splitext(it)[1] == '.h' for it in self.target.outputs)


class WriteBuildDateHeaderSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self._set_arg_at(0, '$(out)')
        super()._sanitize_args()

class WriteBuildFlagHeaderSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self._set_value_arg('--gen-dir', '.')
        self._set_value_arg('--output', '$(out)')
        super()._sanitize_args()

class GnRunBinarySanitizer(BaseActionSanitizer):

    def __init__(self, target, arch):
        super().__init__(target, arch)
        self.binary_to_target = {
            "clang_x64/transport_security_state_generator":
            "cronet_aml_net_tools_transport_security_state_generator_transport_security_state_generator__testing",
        }
        self.binary = self.binary_to_target[self.target.args[0]]

    def _replace_gen_with_location_tag(self, arg):
        if arg.startswith("gen/"):
            return "$(location %s)" % arg.replace("gen/", "")
        return arg

    def _replace_binary(self, arg):
        if arg in self.binary_to_target:
            return '$(location %s)' % self.binary
        return arg

    def _remove_python_args(self):
        self.target.args = [
            arg for arg in self.target.args if "python3" not in arg
        ]

    def _sanitize_args(self):
        self._update_all_args(self._sanitize_filepath_with_location_tag)
        self._update_all_args(self._replace_gen_with_location_tag)
        self._update_all_args(self._replace_binary)
        self._remove_python_args()
        super()._sanitize_args()

    def get_tools(self):
        tools = super().get_tools()
        tools.add(self.binary)
        return tools

    def get_cmd(self):
        # Remove the script and use the binary right away
        return self.get_pre_cmd() + NEWLINE.join(self.target.args)


class JniGeneratorSanitizer(BaseActionSanitizer):

    def __init__(self, target, arch, is_test_target):
        self.is_test_target = is_test_target
        super().__init__(target, arch)

    def get_srcs(self):
        all_srcs = super().get_srcs()
        all_srcs.update({
            gn_utils.label_to_path(file)
            for file in self.target.transitive_jni_java_sources
            if is_supported_source_file(file)
        })
        return set(src for src in all_srcs if src.endswith(".java"))

    def _add_location_tag_to_filepath(self, arg):
        if not arg.endswith('.class'):
            # --input_file supports both .class specifiers or source files as arguments.
            # Only source files need to be wrapped inside a $(location <label>) tag.
            arg = self._add_location_tag(arg)
        return arg

    def _sanitize_args(self):
        self._set_value_arg('--jar-file', '$(location :current_android_jar)',
                            False)
        if self._has_arg('--jar-file'):
            self._set_value_arg('--javap', '$(location :javap)')
        self._update_value_arg('--srcjar-path', self._sanitize_filepath, False)
        self._update_value_arg('--output-dir', self._sanitize_filepath)
        self._update_value_arg('--extra-include', self._sanitize_filepath,
                               False)
        self._update_value_arg('--placeholder-srcjar-path',
                               self._sanitize_filepath, False)
        self._update_list_arg('--input-file', self._sanitize_filepath)
        self._update_list_arg('--input-file',
                              self._add_location_tag_to_filepath)
        if not self.is_test_target and not self._has_arg('--jar-file'):
            # Don't jarjar classes that already exists within the java SDK. The headers generated
            # from those genrule can simply call into the original class as it exists outside
            # of cronet's jar.
            # Only jarjar platform code
            self._append_arg('--package-prefix', 'android.net.connectivity')
        super()._sanitize_args()

    def get_outputs(self):
        outputs = set()
        for out in super().get_outputs():
            # placeholder.srcjar contains empty placeholder classes used to compile generated java files
            # without any other deps. This is not used in aosp.
            if out.endswith("_placeholder.srcjar"):
                continue
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
        if self._has_arg('--jar-file'):
            tools.add(":javap")
        return tools


class JavaJniGeneratorSanitizer(JniGeneratorSanitizer):

    def __init__(self, target, arch, is_test_target):
        self.is_test_target = is_test_target
        super().__init__(target, arch, is_test_target)

    def get_outputs(self):
        # fix target.output directory to match #include statements.
        outputs = {
            re.sub('^jni_headers/', '', out)
            for out in super().get_outputs()
        }
        self.target.outputs = [
            out for out in outputs if out.endswith(".srcjar")
        ]
        return outputs

    def get_deps(self):
        return {}

    def get_name(self):
        name = super().get_name() + "__java"
        return name


class JniRegistrationGeneratorSanitizer(BaseActionSanitizer):

    def __init__(self, target, arch, is_test_target):
        self.is_test_target = is_test_target
        super().__init__(target, arch)

    def get_srcs(self):
        all_srcs = super().get_srcs()
        all_srcs.update({
            gn_utils.label_to_path(file)
            for file in self.target.transitive_jni_java_sources
            if is_supported_source_file(file)
        })
        return set(src for src in all_srcs if src.endswith(".java"))

    def _sanitize_inputs(self):
        self.target.inputs = [
            file for file in self.target.inputs
            if not file.startswith('//out/')
        ]

    def get_outputs(self):
        outputs = set()
        for out in super().get_outputs():
            # placeholder.srcjar contains empty placeholder classes used to compile generated java files
            # without any other deps. This is not used in aosp.
            if out.endswith("_placeholder.srcjar"):
                continue
            # fix target.output directory to match #include statements.
            outputs.add(re.sub('^jni_headers/', '', out))
        return outputs

    def _sanitize_args(self):
        self._update_value_arg('--depfile', self._sanitize_filepath)
        self._update_value_arg('--srcjar-path', self._sanitize_filepath)
        self._update_value_arg('--header-path', self._sanitize_filepath)
        self._update_value_arg('--placeholder-srcjar-path',
                               self._sanitize_filepath, False)
        self._delete_value_arg('--depfile', False)
        self._set_value_arg('--java-sources-file', '$(genDir)/java.sources')
        if not self.is_test_target:
            # Only jarjar platform code
            self._append_arg('--package-prefix', 'android.net.connectivity')
        super()._sanitize_args()

    def get_cmd(self):
        # jni_registration_generator.py doesn't work with python2
        cmd = "python3 " + super().get_base_cmd()
        # Path in the original sources file does not work in genrule.
        # So creating sources file in cmd based on the srcs of this target.
        # Adding ../$(current_dir)/ to the head because jni_registration_generator.py uses the files
        # whose path startswith(..)
        commands = [
            "current_dir=`basename \\\`pwd\\\``;", "for f in $(in);", "do",
            "echo \\\"../$$current_dir/$$f\\\" >> $(genDir)/java.sources;",
            "done;", cmd
        ]

        return self.get_pre_cmd() + NEWLINE.join(commands)

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

    def get_outputs(self):
        return [
            out for out in super().get_outputs() if out.endswith(".srcjar")
        ]

    def get_deps(self):
        return {}

class VersionSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self._set_value_arg('-o', '$(out)')
        # args for the version.py contain file path without leading --arg key. So apply sanitize
        # function for all the args.
        self._update_all_args(self._sanitize_filepath_with_location_tag)
        self._update_list_arg('-e', self._sanitize_eval)
        super()._sanitize_args()

    def _sanitize_eval(self, eval_arg):
        return "'%s'" % eval_arg.replace("\'", "\\\"")

    def get_tool_files(self):
        tool_files = super().get_tool_files()
        # android_chrome_version.py is not specified in anywhere but version.py imports this file
        tool_files.add('build/util/android_chrome_version.py')
        return tool_files

class JavaCppEnumSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self._update_all_args(self._sanitize_filepath_with_location_tag)
        self._set_value_arg('--srcjar', '$(out)')
        super()._sanitize_args()

class MakeDafsaSanitizer(BaseActionSanitizer):

    def is_header_generated(self):
        # This script generates .cc files but they are #included by other sources
        # (e.g. registry_controlled_domain.cc)
        return True

class JavaCppFeatureSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self._update_all_args(self._sanitize_filepath_with_location_tag)
        self._set_value_arg('--srcjar', '$(out)')
        super()._sanitize_args()

class JavaCppStringSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self._update_all_args(self._sanitize_filepath_with_location_tag)
        self._set_value_arg('--srcjar', '$(out)')
        super()._sanitize_args()

class WriteNativeLibrariesJavaSanitizer(BaseActionSanitizer):

    def _sanitize_args(self):
        self._set_value_arg('--output', '$(out)')
        super()._sanitize_args()


class ProtocJavaSanitizer(BaseActionSanitizer):

    def __init__(self, target, arch, gn):
        super().__init__(target, arch)
        self._protoc = get_protoc_module_name(gn)

    def _sanitize_proto_path(self, arg):
        arg = self._sanitize_filepath(arg)
        return tree_path + '/' + arg

    def _sanitize_args(self):
        super()._sanitize_args()
        self._delete_value_arg('--depfile')
        self._set_value_arg('--protoc', '$(location %s)' % self._protoc)
        self._update_value_arg('--proto-path', self._sanitize_proto_path)
        self._set_value_arg('--srcjar', '$(out)')
        self._update_arg_at(-1, self._sanitize_filepath_with_location_tag)

    def get_tools(self):
        tools = super().get_tools()
        tools.add(self._protoc)
        return tools


def get_action_sanitizer(gn, target, type, arch, is_test_target):
    if target.script == "//build/write_buildflag_header.py" or target.script == "//base/allocator/partition_allocator/src/partition_alloc/write_buildflag_header.py":
        # PartitionAlloc has forked the same write_buildflag_header.py script from
        # Chromium to break its dependency on //build.
        return WriteBuildFlagHeaderSanitizer(target, arch)
    elif target.script == "//base/write_build_date_header.py":
        return WriteBuildDateHeaderSanitizer(target, arch)
    elif target.script == "//build/util/version.py":
        return VersionSanitizer(target, arch)
    elif target.script == "//build/android/gyp/java_cpp_enum.py":
        return JavaCppEnumSanitizer(target, arch)
    elif target.script == "//net/tools/dafsa/make_dafsa.py":
        return MakeDafsaSanitizer(target, arch)
    elif target.script == '//build/android/gyp/java_cpp_features.py':
        return JavaCppFeatureSanitizer(target, arch)
    elif target.script == '//build/android/gyp/java_cpp_strings.py':
        return JavaCppStringSanitizer(target, arch)
    elif target.script == '//build/android/gyp/write_native_libraries_java.py':
        return WriteNativeLibrariesJavaSanitizer(target, arch)
    elif target.script == '//build/gn_run_binary.py':
        return GnRunBinarySanitizer(target, arch)
    elif target.script == '//build/protoc_java.py':
        return ProtocJavaSanitizer(target, arch, gn)
    elif target.script == '//third_party/jni_zero/jni_zero.py':
        if target.args[0] == 'generate-final':
            if type == 'java_genrule':
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
                    target.sources.update(gn.jni_java_sources)
                return JavaJniRegistrationGeneratorSanitizer(
                    target, arch, is_test_target)
            else:
                return JniRegistrationGeneratorSanitizer(
                    target, arch, is_test_target)
        else:
            if type == 'cc_genrule':
                return JniGeneratorSanitizer(target, arch, is_test_target)
            else:
                return JavaJniGeneratorSanitizer(target, arch, is_test_target)
    else:
        raise Exception('Unsupported action %s from %s' %
                        (target.script, target.name))

def create_action_foreach_modules(blueprint, gn, target, is_test_target):
    """ The following assumes that rebase_path exists in the args.
  The args of an action_foreach contains hints about which output files are generated
  by which source files.
  This is copied directly from the args
  "gen/net/base/registry_controlled_domains/{{source_name_part}}-reversed-inc.cc"
  So each source file will generate an output whose name is the {source_name-reversed-inc.cc}
  """
    new_args = []
    for i, src in enumerate(sorted(target.sources)):
        # don't add script arg for the first source -- create_action_module
        # already does this.
        if i != 0:
            new_args.append('&&')
            new_args.append('python3 $(location %s)' %
                            gn_utils.label_to_path(target.script))
        for arg in target.args:
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
                for out in target.outputs:
                    if out.endswith(file_name):
                        new_args.append('$(location %s)' % out)

                for file in (target.sources | target.inputs):
                    if file.endswith(file_name):
                        new_args.append('$(location %s)' %
                                        gn_utils.label_to_path(file))
            else:
                new_args.append(arg)

    target.args = new_args
    return create_action_module(blueprint, gn, target, 'cc_genrule',
                                is_test_target)


def create_action_module_internal(gn,
                                  target,
                                  type,
                                  is_test_target,
                                  blueprint,
                                  arch=None):
    if target.script == '//build/android/gyp/gcc_preprocess.py':
        return create_gcc_preprocess_modules(blueprint, target)
    sanitizer = get_action_sanitizer(gn, target, type, arch, is_test_target)
    sanitizer.sanitize()

    module = Module(type, sanitizer.get_name(), target.name)
    module.cmd = sanitizer.get_cmd()
    module.out = sanitizer.get_outputs()
    if sanitizer.is_header_generated():
        module.genrule_headers.add(module.name)
    module.srcs = sanitizer.get_srcs()
    module.tool_files = sanitizer.get_tool_files()
    module.tools = sanitizer.get_tools()
    target.deps = sanitizer.get_deps()

    return module

def get_cmd_condition(arch):
    '''
  :param arch: archtecture name e.g. android_x86_64, android_arm64
  :return: condition that can be used in cc_genrule cmd to switch the behavior based on arch
  '''
    if arch == "android_x86_64":
        return "( $$CC_ARCH == 'x86_64' && $$CC_OS == 'android' )"
    elif arch == "android_x86":
        return "( $$CC_ARCH == 'x86' && $$CC_OS == 'android' )"
    elif arch == "android_arm":
        return "( $$CC_ARCH == 'arm' && $$CC_OS == 'android' )"
    elif arch == "android_arm64":
        return "( $$CC_ARCH == 'arm64' && $$CC_OS == 'android' )"
    elif arch == "android_riscv64":
        return "( $$CC_ARCH == 'riscv64' && $$CC_OS == 'android' )"
    elif arch == "host":
        return "$$CC_OS != 'android'"
    else:
        raise Exception(f'Unknown architecture type {arch}')

def merge_cmd(modules, genrule_type):
    '''
  :param modules: dictionary whose key is arch name and value is module
  :param genrule_type: cc_genrule or java_genrule
  :return: merged command or common command if all the archs have the same command.
  '''
    commands = list({module.cmd for module in modules.values()})
    if len(commands) == 1:
        # If all the archs have the same command, return the command
        return commands[0]

    if genrule_type != 'cc_genrule':
        raise Exception(
            f'{genrule_type} can not have different cmd between archs')

    merged_cmd = []
    for arch, module in sorted(modules.items()):
        merged_cmd.append(f'if [[ {get_cmd_condition(arch)} ]];')
        merged_cmd.append('then')
        merged_cmd.append(module.cmd + ';')
        merged_cmd.append('fi;')
    return NEWLINE.join(merged_cmd)

def merge_modules(modules, genrule_type):
    '''
  :param modules: dictionary whose key is arch name and value is module
  :param genrule_type: cc_genrule or java_genrule
  :return: merged module of input modules
  '''
    merged_module = list(modules.values())[0]

    # Following attributes must be the same between archs
    for key in ('genrule_headers', 'srcs', 'tool_files'):
        if any([
                getattr(merged_module, key) != getattr(module, key)
                for module in modules.values()
        ]):
            raise Exception(
                f'{merged_module.name} has different values for {key} between archs'
            )

    merged_module.cmd = merge_cmd(modules, genrule_type)
    return merged_module

def create_action_module(blueprint, gn, target, genrule_type, is_test_target):
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
        module = create_action_module_internal(gn, target, genrule_type,
                                               is_test_target, blueprint,
                                               target.arch['android_arm'])
        blueprint.add_module(module)
        return module

    modules = {
        arch_name:
        create_action_module_internal(gn, target, genrule_type, is_test_target,
                                      blueprint, arch)
        for arch_name, arch in target.get_archs().items()
    }
    module = merge_modules(modules, genrule_type)
    blueprint.add_module(module)
    return module


def _get_cflags(cflags, defines):
    cflags = {flag for flag in cflags if flag in cflag_allowlist}
    # Consider proper allowlist or denylist if needed
    cflags |= set("-D%s" % define.replace("\"", "\\\"") for define in defines)
    return cflags

def _set_linker_script(module, libs):
    for lib in libs:
        if lib.endswith(".lds"):
            module.ldflags.add(
                get_linker_script_ldflag(gn_utils.label_to_path(lib)))

def set_module_flags(module, module_type, cflags, defines, ldflags, libs):
    module.cflags.update(_get_cflags(cflags, defines))
    module.ldflags.update({
        flag
        for flag in ldflags
        if flag in ldflag_allowlist or flag.startswith("-Wl,-wrap,")
    })
    _set_linker_script(module, libs)
    # TODO: implement proper cflag parsing.
    for flag in cflags:
        if '-std=' in flag:
            module.cpp_std = flag[len('-std='):]
        if '-fexceptions' in flag:
            module.cppflags.add('-fexceptions')

def set_module_include_dirs(module, cflags, include_dirs):
    for flag in cflags:
        if '-isystem' in flag:
            module.include_dirs.add(
                f"external/cronet/{flag[len('-isystem../../'):]}")

    # Adding include_dirs is necessary due to source_sets / filegroups
    # which do not properly propagate include directories.
    # Filter any directory inside //out as a) this directory does not exist for
    # aosp / soong builds and b) the include directory should already be
    # configured via library dependency.
    # Note: include_dirs is used instead of local_include_dirs as an Android.bp
    # can't access other directories outside of its current directory. This
    # is worked around by using include_dirs.
    module.include_dirs.update([
        f"external/cronet/{gn_utils.label_to_path(d)}" for d in include_dirs
        if not d.startswith('//out')
    ])
    # Remove prohibited include directories
    module.include_dirs = [
        d for d in module.include_dirs if d not in include_dirs_denylist
    ]


def create_modules_from_target(blueprint, gn, gn_target_name, parent_gn_type,
                               is_test_target):
    """Generate module(s) for a given GN target.

    Given a GN target name, generate one or more corresponding modules into a
    blueprint. The only case when this generates >1 module is proto libraries.

    Args:
        blueprint: Blueprint instance which is being generated.
        gn: gn_utils.GnParser object.
        gn_target_name: GN target for module generation.
        parent_gn_type: GN type of the parent node.
    """
    bp_module_name = label_to_module_name(gn_target_name)
    target = gn.get_target(gn_target_name)

    # Append __java suffix to actions reachable from java_library. This is necessary
    # to differentiate them from cc actions.
    # This means that a GN action of name X will be translated to two different modules of names
    # X and X__java(only if X is reachable from a java target).
    if target.type == "action" and parent_gn_type == "java_library":
        bp_module_name += "__java"

    if target.type in ["rust_library", "rust_proc_macro"]:
        # "lib{crate_name}" must be a prefix of the module name, this is a Soong
        # restriction.
        # https://cs.android.com/android/_/android/platform/build/soong/+/31934a55a8a1f9e4d56d68810f4a646f12ab6eb5:rust/library.go;l=724;drc=fdec8723d574daf54b956cc0f6dc879087da70a6;bpv=0;bpt=0
        if len(target.crate_name) > 35:
            # Chromium will set the `crate_name` to some mangled string from the
            # GN target label, this is only the case when `crate_name` is not
            # explicitly defined. We don't want that as the names get too long and
            # exceeds the OS limit which results in compilation errors. This tries
            # to shorten the names by only using the target_name.
            bp_module_name = label_to_module_name(
                get_target_name(gn_target_name))
            target.crate_name = bp_module_name
            bp_module_name = f"lib{bp_module_name}"
        else:
            bp_module_name = f"lib{target.crate_name}_{bp_module_name}"

        if parent_gn_type in ["static_library", "shared_library"]:
            # CC modules must depend on a different type of modules that are
            # rust_ffi_static instead of rust_library_rlib
            bp_module_name += "__FFI"

    if bp_module_name in blueprint.modules:
        return blueprint.modules[bp_module_name]

    log.info('create modules for %s (%s)', target.name, target.type)

    if target.type == 'executable':
        if target.testonly:
            module_type = 'cc_test'
        else:
            # Can be used for both host and device targets.
            module_type = 'cc_binary'
        module = Module(module_type, bp_module_name, gn_target_name)
    elif target.type == 'rust_executable':
        module = Module("rust_binary", bp_module_name, gn_target_name)
    elif target.type == "rust_library":
        _type = "rust_library_rlib"
        if parent_gn_type in ["static_library", "shared_library"]:
            # CPP modules must depend on rust_ffi_static as this generates the
            # necessary static library that can be linked.
            _type = "rust_ffi_static"
        # Chromium only uses rlibs.
        module = Module(_type, bp_module_name, gn_target_name)
    elif target.type == "rust_proc_macro":
        module = Module("rust_proc_macro", bp_module_name, gn_target_name)
    elif target.type in ['static_library', 'source_set']:
        module = Module('cc_library_static', bp_module_name, gn_target_name)
    elif target.type == 'shared_library':
        module = Module('cc_library_shared', bp_module_name, gn_target_name)
    elif target.type == 'group':
        # "group" targets are resolved recursively by gn_utils.get_target().
        # There's nothing we need to do at this level for them.
        return None
    elif target.type == 'proto_library':
        module = create_proto_modules(blueprint, gn, target)
        if module is None:
            return None
    elif target.type == 'action':
        module = create_action_module(
            blueprint, gn, target, 'java_genrule' if parent_gn_type
            == "java_library" else 'cc_genrule', is_test_target)
    elif target.type == 'action_foreach':
        if target.script == "//third_party/rust/cxx/chromium_integration/run_cxxbridge.py":
            module = create_rust_cxx_module(blueprint, target)
        else:
            module = create_action_foreach_modules(blueprint, gn, target,
                                                   is_test_target)
    elif target.type == 'copy':
        # TODO: careful now! copy targets are not supported yet, but this will stop
        # traversing the dependency tree. For //base:base, this is not a big
        # problem as libicu contains the only copy target which happens to be a
        # leaf node.
        return None
    elif target.type == 'java_library':
        if target.jar_path:
            module = Module('java_import', bp_module_name, gn_target_name)
            module.jars.add(target.jar_path)
        else:
            module = Module('java_library', bp_module_name, gn_target_name)
            # Don't remove GEN_JNI from those modules as they have the real GEN_JNI that we want to include
            if gn_target_name not in [
                    '//components/cronet/android:cronet_jni_registration_java',
                    '//components/cronet/android:cronet_jni_registration_java__testing',
                    '//components/cronet/android:cronet_tests_jni_registration_java__testing'
            ]:
                module.jarjar_rules = REMOVE_GEN_JNI_JARJAR_RULES_FILE
        module.min_sdk_version = 30
        module.apex_available = [tethering_apex]
        if is_test_target:
            module.sdk_version = target.sdk_version
        else:
            module.defaults.add(java_framework_defaults_module)
    else:
        raise Exception('Unknown target %s (%s)' % (target.name, target.type))

    blueprint.add_module(module)
    if target.type not in ['action', 'action_foreach']:
        # Actions should get their srcs from their corresponding ActionSanitizer as actionSanitizer
        # filters srcs differently according to the type of the action.
        module.srcs.update(
            gn_utils.label_to_path(src) for src in target.sources
            if is_supported_source_file(src))

    # Add arch-specific properties
    for arch_name, arch in target.get_archs().items():
        module.target[arch_name].srcs.update(
            gn_utils.label_to_path(src) for src in arch.sources
            if is_supported_source_file(src))

    module.rtti = target.rtti

    if target.type in gn_utils.LINKER_UNIT_TYPES:
        set_module_flags(module, module.type, target.cflags, target.defines,
                         target.ldflags, target.libs)
        set_module_include_dirs(module, target.cflags, target.include_dirs)
        # TODO: set_module_xxx is confusing, apply similar function to module and target in better way.
        for arch_name, arch in target.get_archs().items():
            # TODO(aymanm): Make libs arch-specific.
            set_module_flags(module.target[arch_name], module.type,
                             arch.cflags, arch.defines, arch.ldflags, [])
            # -Xclang -target-feature -Xclang +mte are used to enable MTE (Memory Tagging Extensions).
            # Flags which does not start with '-' could not be in the cflags so enabling MTE by
            # -march and -mcpu Feature Modifiers. MTE is only available on arm64. This is needed for
            # building //base/allocator/partition_allocator:partition_alloc for arm64.
            if '+mte' in arch.cflags and arch_name == 'android_arm64':
                module.target[arch_name].cflags.add('-march=armv8-a+memtag')
            set_module_include_dirs(module.target[arch_name], arch.cflags,
                                    arch.include_dirs)

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
    module.visibility = {"//external/cronet:__subpackages__"}

    if module.type.startswith("rust"):
        module.crate_name = target.crate_name
        module.crate_root = gn_utils.label_to_path(target.crate_root)
        module.min_sdk_version = 30
        module.apex_available = [tethering_apex]
        for arch_name, arch in target.get_archs().items():
            _set_rust_flags(module.target[arch_name], arch.rust_flags,
                            arch_name)

    if module.is_genrule():
        module.apex_available.add(tethering_apex)

    if module.type == "java_library":
        if gn_utils.contains_aidl(target.sources):
            # frameworks/base/core/java includes the source files that are used to compile framework.aidl.
            # framework.aidl is added implicitly as a dependency to every AIDL GN action, this can be
            # identified by third_party/android_sdk/public/platforms/android-34/framework.aidl.
            module.aidl["include_dirs"] = {"frameworks/base/core/java/"}
            module.aidl["local_include_dirs"] = target.local_aidl_includes

    if (module.is_compiled() and not module.type.startswith("java")
            and not module.type.startswith("rust")):
        # Don't try to inject library/source dependencies into genrules or
        # filegroups because they are not compiled in the traditional sense.
        module.defaults = [cc_defaults_module]
        for lib in target.libs:
            # Generally library names should be mangled as 'libXXX', unless they
            # are HAL libraries (e.g., android.hardware.health@2.0) or AIDL c++ / NDK
            # libraries (e.g. "android.hardware.power.stats-V1-cpp")
            android_lib = lib if '@' in lib or "-cpp" in lib or "-ndk" in lib \
              else 'lib' + lib
            if lib in shared_library_allowlist:
                module.add_android_shared_lib(android_lib)

    # If the module is a static library, export all the generated headers.
    if module.type == 'cc_library_static':
        module.export_generated_headers = module.generated_headers

    if module.name in [
            'cronet_aml_components_cronet_android_cronet',
            'cronet_aml_components_cronet_android_cronet' +
            gn_utils.TESTING_SUFFIX
    ]:
        if target.output_name is None:
            raise Exception('Failed to get output_name for libcronet name')
        # .so file name needs to match with CronetLibraryLoader.java (e.g. libcronet.109.0.5386.0.so)
        # So setting the output name based on the output_name from the desc.json
        module.stem = 'libmainline' + target.output_name
    elif module.is_test() and module.type == 'cc_library_shared':
        if target.output_name:
            # If we have an output name already declared, use it.
            module.stem = 'lib' + target.output_name
        else:
            # Tests output should be a shared library in the format of 'lib[module_name]'
            module.stem = 'lib' + target.get_target_name(
            )[:target.get_target_name().find(gn_utils.TESTING_SUFFIX)]

    # dep_name is an unmangled GN target name (e.g. //foo:bar(toolchain)).
    all_deps = [(dep_name, 'common') for dep_name in target.proto_deps]
    for arch_name, arch in target.arch.items():
        all_deps += [(dep_name, arch_name) for dep_name in arch.deps]

    # Sort deps before iteration to make result deterministic.
    for (dep_name, arch_name) in sorted(all_deps):
        module_target = module.target[
            arch_name] if arch_name != 'common' else module
        # |builtin_deps| override GN deps with Android-specific ones. See the
        # config in the top of this file.
        if dep_name in builtin_deps:
            builtin_deps[dep_name](module, arch_name)
            continue

        # This is like the builtin_deps with always_disable except that it matches
        # a string.
        if "_build_script" in dep_name:
            continue

        dep_module = create_modules_from_target(blueprint, gn, dep_name,
                                                target.type, is_test_target)

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
            module_target.genrule_headers.update(dep_module.genrule_headers)

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

        if dep_module.type == 'cc_library_shared':
            module_target.shared_libs.add(dep_module.name)
        elif dep_module.type == 'cc_library_static':
            if module.type in ['cc_library_shared', 'cc_binary']:
                module_target.whole_static_libs.add(dep_module.name)
            elif module.type == 'cc_library_static':
                module_target.generated_headers.update(
                    dep_module.generated_headers)
                module_target.shared_libs.update(dep_module.shared_libs)
                module_target.header_libs.update(dep_module.header_libs)
            elif module.type in [
                    "rust_library_rlib", "rust_binary", "rust_ffi_static"
            ]:
                module_target.static_libs.add(dep_module.name)
            else:
                raise Exception(
                    f"Trying to add an unknown type {dep_module.type} to a type of {module.type}"
                )
        elif dep_module.type == "rust_library_rlib":
            module_target.rustlibs.add(dep_module.name)
        elif dep_module.type == "rust_ffi_static":
            assert module.type in [
                "cc_library_static", "cc_library_shared"
            ], "Only CC libraries can depend on rust_ffi_static"
            # CPP libraries must not depend on rust_library_rlib, they must depend
            # on rust_ffi_rlib as per aosp/3094614 and go/android-made-to-order-rust-staticlibs.
            module_target.static_libs.add(dep_module.name)
        elif dep_module.type == "rust_proc_macro":
            module_target.proc_macros.add(dep_module.name)
        elif dep_module.type == 'cc_genrule':
            module_target.generated_headers.update(dep_module.genrule_headers)
            module_target.srcs.update(dep_module.genrule_srcs)
            module_target.shared_libs.update(dep_module.genrule_shared_libs)
            module_target.header_libs.update(dep_module.genrule_header_libs)
        elif dep_module.type in ['java_library', 'java_import']:
            # A module depending on a module with system_current sdk version should also compile against
            # the system sdk. This is because a module's SDK API surface should be >= its deps SDK API surface.
            # And system_current has a larger API surface than current or module_current.
            if dep_module.sdk_version == 'system_current':
                module.sdk_version = 'system_current'
            if module.type not in ["cc_library_static"]:
                # This is needed to go around the case where `url` component depends
                # on `url_java`.
                # TODO(aymanm): Remove the if condition once crrev/4902547 has been imported downstream
                module_target.static_libs.add(dep_module.name)
        elif dep_module.type in ['genrule', 'java_genrule']:
            module_target.srcs.add(":" + dep_module.name)
        else:
            raise Exception(
                'Unsupported arch-specific dependency %s of target %s with type %s'
                % (dep_module.name, target.name, dep_module.type))
    return module


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

def create_cc_defaults_module():
    defaults = Module('cc_defaults', cc_defaults_module, '//gn:default_deps')
    defaults.cflags = [
        '-DGOOGLE_PROTOBUF_NO_RTTI',
        '-DBORINGSSL_SHARED_LIBRARY',
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
    ]
    defaults.build_file_path = ""
    defaults.include_build_directory = False
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
    defaults.stl = 'none'
    defaults.cpp_std = 'c++17'
    defaults.min_sdk_version = 29
    defaults.apex_available.add(tethering_apex)
    return defaults

def create_blueprint_for_targets(gn, targets, test_targets):
    """Generate a blueprint for a list of GN targets."""
    blueprint = Blueprint()

    # Default settings used by all modules.
    blueprint.add_module(create_cc_defaults_module())

    for target in targets:
        module = create_modules_from_target(blueprint,
                                            gn,
                                            target,
                                            parent_gn_type=None,
                                            is_test_target=False)
        if module:
            module.visibility.update(root_modules_visibility)

    for test_target in test_targets:
        module = create_modules_from_target(blueprint,
                                            gn,
                                            test_target +
                                            gn_utils.TESTING_SUFFIX,
                                            parent_gn_type=None,
                                            is_test_target=True)
        if module:
            module.visibility.update(root_modules_visibility)

    # Merge in additional hardcoded arguments.
    for module in blueprint.modules.values():
        for key, add_val in additional_args.get(module.name, []):
            curr = getattr(module, key)
            if add_val and isinstance(add_val, set) and isinstance(curr, set):
                curr.update(add_val)
            elif isinstance(add_val, str) and (not curr
                                               or isinstance(curr, str)):
                setattr(module, key, add_val)
            elif isinstance(add_val, bool) and (not curr
                                                or isinstance(curr, bool)):
                setattr(module, key, add_val)
            elif isinstance(add_val, dict) and isinstance(curr, dict):
                curr.update(add_val)
            elif isinstance(add_val[1], dict) and isinstance(
                    curr[add_val[0]], Module.Target):
                curr[add_val[0]].__dict__.update(add_val[1])
            elif isinstance(curr, dict):
                curr[add_val[0]] = add_val[1]
            else:
                raise Exception(
                    'Unimplemented type %r of additional_args: %r' %
                    (type(add_val), key))

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
def _rebase_module(module: Module, blueprint_path: str) -> Union[Module, None]:
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
    if module_copy.crate_root:
        module_copy.crate_root = _rebase_file(module_copy.crate_root,
                                              blueprint_path)
        if module_copy.crate_root is None:
            return None

    if module_copy.srcs:
        module_copy.srcs = _rebase_files(module_copy.srcs, blueprint_path)
        if module_copy.srcs is None:
            return None

    if module_copy.jars:
        module_copy.jars = _rebase_files(module_copy.jars, blueprint_path)
        if module_copy.jars is None:
            return None

    for (arch_name, arch) in module_copy.target.items():
        module_copy.target[arch_name].srcs = (_rebase_files(
            module_copy.target[arch_name].srcs, blueprint_path))
        if module_copy.target[arch_name].srcs is None:
            return None

    return module_copy

def _path_to_name(path: str) -> str:
    return "external_cronet_%s_license" % (path.replace("/", "_").lower())


def _maybe_create_license_module(path: str) -> Union[Module, None]:
    """
  Creates a module license if a README.chromium exists in the path provided
  otherwise just returns None.

  :param path: Path to check for README.chromium
  :return: Module or None.
  """
    readme_chromium_file = Path(os.path.join(path, "README.chromium"))
    if (not readme_chromium_file.exists()
            or license_utils.is_ignored_readme_chromium(
                str(readme_chromium_file))):
        return None

    license_module = Module("license", _path_to_name(path),
                            "License-Artificial")
    license_module.visibility = {":__subpackages__"}
    # Assume that a LICENSE file always exist as we run the
    # create_android_metadata_license.py script each time we run GN2BP.
    license_module.license_text = {"LICENSE"}
    metadata = license_utils.parse_chromium_readme_file(
        str(readme_chromium_file))
    for license in metadata.get_licenses():
        license_module.license_kinds.add(
            license_utils.get_license_bp_name(license))
    return license_module


def _get_longest_matching_blueprint(
        current_blueprint_path: str,
        all_blueprints: Dict[str, Blueprint]) -> Union[Blueprint, None]:
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

def finalize_package_modules(blueprints: Dict[str, Blueprint]):
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

        package_module = Module("package", None, "Package-Artificial")
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
        blueprints: Dict[str, Blueprint]) -> Dict[str, Module]:
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
            blueprint.get_readme_location())
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


def _locate_android_bp_destination(module: Module) -> str:
    """Returns the appropriate location of the generated Android.bp for the
  specified module. Sometimes it is favourable to relocate the Android.bp to
  a different location other than next to BUILD.gn (eg: rust's BUILD.gn are
  defined in a different directory than the source code).

  :returns the appropriate location for the blueprint
  """
    crate_root_dir = _get_rust_crate_root_directory_from_crate_root(
        module.crate_root)
    if module.build_file_path in BLUEPRINTS_MAPPING:
        return BLUEPRINTS_MAPPING[module.build_file_path]
    elif crate_root_dir:
        return crate_root_dir
    else:
        return module.build_file_path


def _break_down_blueprint(top_level_blueprint: Blueprint):
    """
  This breaks down the top-level blueprint into smaller blueprints in
  different directory. The goal here is to break down the huge Android.bp
  into smaller ones for compliance with SBOM. At the moment, not all targets
  can be easily rebased to a different directory as GN does not respect
  package boundaries.

  :returns A dictionary of path -> Blueprint, the path is relative to repository
  root.
  """
    blueprints = {"": Blueprint()}
    for (module_name, module) in top_level_blueprint.modules.items():
        if module.type in [
                "package", "genrule", "cc_genrule", "java_genrule", "cc_object"
        ] and not module.allow_rebasing:
            # Exclude the genrules from the rebasing as there is no support for them.
            # cc_object is created only for the sake of genrules as an intermediate
            # target.
            blueprints[""].add_module(module)
            continue

        android_bp_path = _locate_android_bp_destination(module)
        if android_bp_path is None:
            # Raise an exception if the module does not specify a BUILD file path.
            raise Exception(
                f"Found module {module_name} without a build file path.")

        rebased_module = _rebase_module(module, android_bp_path)
        if rebased_module:
            if not android_bp_path in blueprints.keys():
                blueprints[android_bp_path] = Blueprint(module.build_file_path)
            blueprints[android_bp_path].add_module(rebased_module)
        else:
            # Append to the top-level blueprint.
            blueprints[""].add_module(module)

    for blueprint in blueprints.values():
        if blueprint.get_buildgn_location() in README_MAPPING:
            blueprint.set_readme_location(
                README_MAPPING[blueprint.get_buildgn_location()])
    return blueprints

def main():
    parser = argparse.ArgumentParser(
        description='Generate Android.bp from a GN description.')
    parser.add_argument(
        '--desc',
        help=
        'GN description (e.g., gn desc out --format=json --all-toolchains "//*".'
        + 'You can specify multiple --desc options for different target_cpu',
        required=True,
        action='append')
    parser.add_argument('--repo_root',
                        required=True,
                        help='Path to the root of the repistory')
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
    args = parser.parse_args()

    if args.verbose:
        log.basicConfig(format='%(levelname)s:%(funcName)s:%(message)s',
                        level=log.DEBUG)

    targets = args.targets or DEFAULT_TARGETS
    build_scripts_output = None
    with open(args.build_script_output) as f:
        build_scripts_output = json.load(f)
    gn = gn_utils.GnParser(builtin_deps, build_scripts_output)
    for desc_file in args.desc:
        with open(desc_file) as f:
            desc = json.load(f)
        for target in targets:
            gn.parse_gn_desc(desc, target)
        for test_target in DEFAULT_TESTS:
            gn.parse_gn_desc(desc, test_target, is_test_target=True)
    top_level_blueprint = create_blueprint_for_targets(gn, targets,
                                                       DEFAULT_TESTS)
    project_root = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
    tool_name = os.path.relpath(os.path.abspath(__file__), project_root)

    final_blueprints = _break_down_blueprint(top_level_blueprint)
    license_modules = create_license_modules(final_blueprints)
    for (path, module) in license_modules.items():
        final_blueprints[path].set_license_module(module)

    finalize_package_modules(final_blueprints)

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
""" % (tool_name)

    for (path, blueprint) in final_blueprints.items():
        android_bp_file = Path(os.path.join(args.repo_root, path,
                                            "Android.bp"))
        android_bp_file.write_text(
            "\n".join([header] + BLUEPRINTS_EXTRAS.get(path, []) +
                      blueprint.to_string()))

    return 0


if __name__ == '__main__':
    sys.exit(main())
