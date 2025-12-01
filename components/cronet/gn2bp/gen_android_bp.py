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

PARENT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                           os.pardir))

sys.path.insert(0, os.path.join(PARENT_ROOT, "license"))
import license_utils
import constants as license_constants

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
sys.path.insert(0, REPOSITORY_ROOT)

import components.cronet.tools.utils as cronet_utils
import components.cronet.gn2bp.common as gn2bp_common  # pylint: disable=wrong-import-position
import build.gn_helpers

CRONET_LICENSE_NAME = "external_cronet_license"

CPP_VERSION = 'c++17'

EXTRAS_ANDROID_BP_FILE = "Android.extras.bp"

# TODO: crbug.com/xxx - Relying on (and modifying this) this global variable is bad. Refactor and properly inject this into what requires it.
IMPORT_CHANNEL = 'MODIFIED_BY_MAIN_AFTER_PARSING_ARGS_IF_YOU_SEE_THIS_SOMETHING_BROKE_'
# All module names are prefixed with this string to avoid collisions.
MODULE_PREFIX = 'MODIFIED_BY_MAIN_AFTER_PARSING_ARGS_IF_YOU_SEE_THIS_SOMETHING_BROKE_'
# Include directories that will be removed from all targets.
include_dirs_denylist = None
# Name of the module which settings such as compiler flags for all other modules.
cc_defaults_module = 'MODIFIED_BY_MAIN_AFTER_PARSING_ARGS_IF_YOU_SEE_THIS_SOMETHING_BROKE_'
# Additional arguments to apply to Android.bp rules.
additional_args = None
# Name of the java default module for non-test java modules defined in Android.extras.bp
java_framework_defaults_module = 'MODIFIED_BY_MAIN_AFTER_PARSING_ARGS_IF_YOU_SEE_THIS_SOMETHING_BROKE_'
# Location of the project in the Android source tree.
tree_path = 'MODIFIED_BY_MAIN_AFTER_PARSING_ARGS_IF_YOU_SEE_THIS_SOMETHING_BROKE_'


def initialize_globals(import_channel: str):
  global IMPORT_CHANNEL
  IMPORT_CHANNEL = import_channel

  global MODULE_PREFIX
  MODULE_PREFIX = f'{IMPORT_CHANNEL}_cronet_'

  global include_dirs_denylist
  include_dirs_denylist = [
      f'external/cronet/{IMPORT_CHANNEL}/third_party/zlib/',
  ]

  global cc_defaults_module
  cc_defaults_module = f'{MODULE_PREFIX}cc_defaults'

  global java_framework_defaults_module
  java_framework_defaults_module = f'{MODULE_PREFIX}java_framework_defaults'

  global tree_path
  tree_path = f'external/cronet/{IMPORT_CHANNEL}'

  global additional_args
  additional_args = {
      # TODO: operating on the final module names means we have to use short
      # names which are less readable. Find a better way.
      f'{MODULE_PREFIX}39ea1a33_quiche_net_quic_test_tools_proto_gen_h': [
          ('export_include_dirs', {
              "net/third_party/quiche/src",
          })
      ],
      f'{MODULE_PREFIX}39ea1a33_quiche_net_quic_test_tools_proto_gen__testing_h':
      [('export_include_dirs', {
          "net/third_party/quiche/src",
      })],
      # TODO: fix upstream. Both //base:base and
      # //base/allocator/partition_allocator:partition_alloc do not create a
      # dependency on gtest despite using gtest_prod.h.
      f'{MODULE_PREFIX}base_base': [
          ('header_libs', {
              'libgtest_prod_headers',
          }),
          ('export_header_lib_headers', {
              'libgtest_prod_headers',
          }),
      ],
      f'{MODULE_PREFIX}base_allocator_partition_allocator_partition_alloc': [
          ('header_libs', {
              'libgtest_prod_headers',
          }),
      ],
      # TODO(b/309920629): Remove once upstreamed.
      f'{MODULE_PREFIX}components_cronet_android_cronet_api_java__unfiltered': [
          ('srcs', {
              'components/cronet/android/api/src/org/chromium/net/UploadDataProviders.java',
              'components/cronet/android/api/src/org/chromium/net/apihelpers/UploadDataProviders.java',
          }),
      ],
      f'{MODULE_PREFIX}components_cronet_android_cronet_api_java__testing__unfiltered':
      [
          ('srcs', {
              'components/cronet/android/api/src/org/chromium/net/UploadDataProviders.java',
              'components/cronet/android/api/src/org/chromium/net/apihelpers/UploadDataProviders.java',
          }),
      ],
      f'{MODULE_PREFIX}components_cronet_android_cronet_javatests__testing__unfiltered':
      [
          # Needed to @SkipPresubmit annotations
          ('static_libs', {
              'net-tests-utils-host-device-common',
          }),
      ],
      f'{MODULE_PREFIX}components_cronet_android_cronet__testing': [
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
      f'{MODULE_PREFIX}third_party_netty_tcnative_netty_tcnative_so__testing': [
          ('cflags', {"-Wno-error=pointer-bool-conversion"})
      ],
      f'{MODULE_PREFIX}third_party_apache_portable_runtime_apr__testing': [
          ('cflags', {
              "-Wno-incompatible-pointer-types-discards-qualifiers",
          })
      ],
      # TODO(b/324872305): Remove when gn desc expands public_configs and update code to propagate the
      # include_dir from the public_configs
      # We had to add the export_include_dirs for each target because soong generates each header
      # file in a specific directory named after the target.
      f'{MODULE_PREFIX}base_allocator_partition_allocator_src_partition_alloc_chromecast_buildflags':
      [('export_include_dirs', {
          "base/allocator/partition_allocator/src/",
      })],
      f'{MODULE_PREFIX}base_allocator_partition_allocator_src_partition_alloc_chromecast_buildflags__testing':
      [('export_include_dirs', {
          "base/allocator/partition_allocator/src/",
      })],
      f'{MODULE_PREFIX}base_allocator_partition_allocator_src_partition_alloc_chromeos_buildflags':
      [('export_include_dirs', {
          "base/allocator/partition_allocator/src/",
      })],
      f'{MODULE_PREFIX}base_allocator_partition_allocator_src_partition_alloc_chromeos_buildflags__testing':
      [('export_include_dirs', {
          "base/allocator/partition_allocator/src/",
      })],
      f'{MODULE_PREFIX}base_allocator_partition_allocator_src_partition_alloc_debugging_buildflags':
      [('export_include_dirs', {
          "base/allocator/partition_allocator/src/",
      })],
      f'{MODULE_PREFIX}base_allocator_partition_allocator_src_partition_alloc_debugging_buildflags__testing':
      [('export_include_dirs', {
          "base/allocator/partition_allocator/src/",
      })],
      f'{MODULE_PREFIX}base_allocator_partition_allocator_src_partition_alloc_buildflags':
      [('export_include_dirs', {
          ".",
          "base/allocator/partition_allocator/src/",
      })],
      f'{MODULE_PREFIX}base_allocator_partition_allocator_src_partition_alloc_buildflags__testing':
      [('export_include_dirs', {
          ".",
          "base/allocator/partition_allocator/src/",
      })],
      f'{MODULE_PREFIX}base_allocator_partition_allocator_src_partition_alloc_raw_ptr_buildflags':
      [('export_include_dirs', {
          "base/allocator/partition_allocator/src/",
      })],
      f'{MODULE_PREFIX}base_allocator_partition_allocator_src_partition_alloc_raw_ptr_buildflags__testing':
      [('export_include_dirs', {
          "base/allocator/partition_allocator/src/",
      })],
      # Protobuf depends on Unsafe class which is used to perform unsafe native methods. This class is not
      # available in the public API provided by the android platform. It's only available by compiling
      # against `core_current` and adding `libcore_private.stubs` as a dependency.
      # defaults have to be removed to prevent sdk_version collision.
      f'{MODULE_PREFIX}third_party_protobuf_proto_runtime_lite_java__testing__unfiltered':
      [
          ('libs', {
              "libcore_private.stubs",
          }),
          ('defaults', None),
          ('sdk_version', 'core_current'),
      ],
      # Protobuf depends on Unsafe class which is used to perform unsafe native methods. This class is not
      # available in the public API provided by the android platform. It's only available by compiling
      # against `core_current` and adding `libcore_private.stubs` as a dependency.
      # defaults have to be removed to prevent sdk_version collision.
      f'{MODULE_PREFIX}third_party_protobuf_proto_runtime_lite_java__unfiltered':
      [
          ('libs', {
              "libcore_private.stubs",
          }),
          ('defaults', None),
          ('sdk_version', 'core_current'),
      ],
      f'{MODULE_PREFIX}base_base_java_test_support__testing': [
          ('errorprone', ('javacflags', {
              "-Xep:ReturnValueIgnored:WARN",
          }))
      ],
      f'{MODULE_PREFIX}third_party_perfetto_gn_gen_buildflags': [
          ('export_include_dirs', {
              "third_party/perfetto/build_config/",
          })
      ],
      f'{MODULE_PREFIX}third_party_perfetto_gn_gen_buildflags__testing': [
          ('export_include_dirs', {
              "third_party/perfetto/build_config/",
          })
      ],
      # end export_include_dir.
      # TODO: https://crbug.com/418746360 - Handle //base:build_date_internal
      # for os:linux_glibc.
      f'{MODULE_PREFIX}base_build_date_internal__testing': [('host_supported',
                                                             True)],
  }


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
    # BoringSSL's Android.bp is manually maintained and generated via a template,
    # see run_gen2bp.py's _gen_boringssl.
    "third_party/boringssl": "",
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++": "third_party/libc++",
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++abi": "third_party/libc++abi",
}

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


def get_linker_script_ldflag(script_path):
  return f'-Wl,--script,{tree_path}/{script_path}'


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


class JniZeroTargetType(enum.Enum):
  GENERATOR = enum.auto()
  REGISTRATION_GENERATOR = enum.auto()


def get_jni_zero_target_type(target):
  if target.script != '//third_party/jni_zero/jni_zero.py':
    return None
  if target.args[0] == 'generate-final':
    return JniZeroTargetType.REGISTRATION_GENERATOR
  return JniZeroTargetType.GENERATOR


# Given a jni_zero generator module, returns the path to the generated proxy
# and placeholders srcjars.
def get_jni_zero_generator_proxy_and_placeholder_paths(module):
  assert module.jni_zero_target_type == JniZeroTargetType.GENERATOR

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


def always_disable(_, __):
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
  if module.is_genrule():
    return
  # Lets keep statically linking BoringSSL for testing target for now. This should be fixed.
  if module.name.endswith(gn_utils.TESTING_SUFFIX):
    return
  if arch == 'common':
    shared_libs = module.shared_libs
  else:
    shared_libs = module.target[arch].shared_libs
  shared_libs.add(f'{MODULE_PREFIX}libcrypto')
  shared_libs.add(f'{MODULE_PREFIX}libssl')
  shared_libs.add(f'{MODULE_PREFIX}libpki')


def add_androidx_experimental_java_deps(module, _):
  module.libs.add("androidx.annotation_annotation-experimental")


def add_androidx_annotation_java_deps(module, _):
  module.libs.add("androidx.annotation_annotation")


def add_androidx_core_java_deps(module, _):
  module.libs.add("androidx.core_core")


def add_jsr305_java_deps(module, _):
  module.static_libs.add("jsr305")


def add_errorprone_annotation_java_deps(module, _):
  module.libs.add("error_prone_annotations")


def add_androidx_collection_java_deps(module, _):
  module.libs.add("androidx.collection_collection")


def add_junit_java_deps(module, _):
  module.static_libs.add("junit")


def add_truth_java_deps(module, _):
  module.static_libs.add("truth")


def add_hamcrest_java_deps(module, _):
  module.static_libs.add("hamcrest-library")
  module.static_libs.add("hamcrest")


def add_mockito_java_deps(module, _):
  module.static_libs.add("mockito")


def add_guava_java_deps(module, _):
  module.static_libs.add("guava")


def add_androidx_junit_java_deps(module, _):
  module.static_libs.add("androidx.test.ext.junit")


def add_androidx_test_runner_java_deps(module, _):
  module.static_libs.add("androidx.test.runner")


def add_androidx_test_rules_java_deps(module, _):
  module.static_libs.add("androidx.test.rules")


def add_android_test_base_java_deps(module, _):
  module.libs.add("android.test.base")


def add_accessibility_test_framework_java_deps(_, __):
  # BaseActivityTestRule.java depends on this but BaseActivityTestRule.java is not used in aosp.
  pass


def add_espresso_java_deps(module, _):
  module.static_libs.add("androidx.test.espresso.contrib")


def add_android_test_mock_java_deps(module, _):
  module.libs.add("android.test.mock.stubs")


def add_androidx_multidex_java_deps(_, __):
  # Androidx-multidex is disabled on unbundled branches.
  pass


def add_androidx_test_monitor_java_deps(module, _):
  module.libs.add("androidx.test.monitor")


def add_androidx_ui_automator_java_deps(module, _):
  module.static_libs.add("androidx.test.uiautomator_uiautomator")


def add_androidx_test_annotation_java_deps(module, _):
  module.static_libs.add("androidx.test.rules")


def add_androidx_test_core_java_deps(module, _):
  module.static_libs.add("androidx.test.core")


def add_androidx_activity_activity(module, _):
  module.static_libs.add("androidx.activity_activity")


def add_androidx_fragment_fragment(module, _):
  module.static_libs.add("androidx.fragment_fragment")


def add_rustversion_deps(module, _):
  module.proc_macros.add("librustversion")


# Android equivalents for third-party libraries that the upstream project
# depends on. This will be applied to normal and testing targets.
_builtin_deps = {
    '//buildtools/third_party/libunwind:libunwind':
    always_disable,
    # rustc_print_cfg is used to print rustc compiler default assumption
    # for a specific CPU architecture (e.g. target_feature="ssse3"). Those
    # features only changes from one CPU architecture to another. It's used
    # when generating the cxxbindings and build scripts (build scripts is a
    # rust concept, it's an rust binary that generates flags used to compile
    # other rust binaries).
    # For CXXBindings, we use AOSP's binary which already have the configuration
    # specified depending on which arch it's building for.
    # For build scripts, we generate those on Chromium side in a JSON file and
    # inject them in the pipeline.
    #
    # From the above reasoning, we can safely assume that we should not need
    # to build this target at all.
    '//build/rust/gni_impl:rustc_print_cfg':
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
    '//third_party/android_deps:com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework_java':
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
    # rustversion uses a build script. AOSP doesn't support build scripts, so
    # instead use the library from AOSP which has a workaround for it. See
    # https://crbug.com/394303030.
    '//third_party/rust/rustversion/v1:lib__proc_macro':
    add_rustversion_deps,
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


def _parse_pydeps(repo_path):
  '''
  Parses a .pydeps file, returning a list of paths relative to REPOSITORY_ROOT.
  '''
  repo_dir_path = os.path.dirname(repo_path)
  with open(f"{REPOSITORY_ROOT}/{repo_path}", encoding='utf-8') as file:
    return [
        os.path.normpath(f"{repo_dir_path}/{line.strip()}") for line in file
        if not line.startswith('#')
    ]

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
  output.append(
      '    %s: "%s",' %
      (name,
       NEWLINE.join(
           str(line).replace('\\', '\\\\').replace('"', '\\"')
           for line in (value if isinstance(value, list) else [value]))))


def sorted_cflags(cflags_object: Iterable[str]) -> list[str]:
  """Sorts cflags.

  Some cflags are order-dependent (critically, `-U` flags and `-D` flags must
  have their relative order maintained if they reference the same macro name).
  This version of `sorted` respects that.
  """

  # Key creation here is a bit subtle.
  #
  # Basically, all `-D` and `-U` macros are trimmed to their macro names (plus
  # a leading `-D`, regardless of the original prefix). This relies on
  # Python's `sorted` function keeping a stable order, so an original `-UFOO
  # -DFOO=1 -UFOO` remains in that same relative order in the output.
  def flag_to_key(cflag: str) -> str:
    key = cflag
    if key.startswith("-U"):
      key = f"-D{key[2:]}"

    if key.startswith("-D"):
      # Remove any value this is set to, so that doesn't mess with
      # ordering.
      key = key.split("=", 1)[0]
    return key

  return sorted(cflags_object, key=flag_to_key)


class Module:
  """A single module (e.g., cc_binary, cc_test) in a blueprint."""

  class Target:
    """A target-scoped part of a module"""

    def __init__(self, name):
      self.name = name
      self.srcs = set()
      self.shared_libs = set()
      self.static_libs = set()
      self.whole_static_libs = set()
      self.header_libs = set()
      self.cflags = list()
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

    def to_string(self, output):
      nested_out = []
      self._output_field(nested_out, 'srcs')
      self._output_field(nested_out, 'shared_libs')
      self._output_field(nested_out, 'static_libs')
      self._output_field(nested_out, 'whole_static_libs')
      self._output_field(nested_out, 'header_libs')
      self._output_field(nested_out, 'cflags', is_cflags_like=True)
      self._output_field(nested_out, 'stl')
      self._output_field(nested_out, 'cppflags', is_cflags_like=True)
      self._output_field(nested_out, 'include_dirs')
      self._output_field(nested_out, 'generated_headers')
      self._output_field(nested_out, 'export_generated_headers')
      self._output_field(nested_out, 'ldflags', is_cflags_like=True)
      self._output_field(nested_out, 'compile_multilib')
      self._output_field(nested_out, 'stem')
      self._output_field(nested_out, "edition")
      self._output_field(nested_out, 'cfgs')
      self._output_field(nested_out, 'features')
      self._output_field(nested_out, 'flags', sort=False)
      self._output_field(nested_out, 'rustlibs')
      self._output_field(nested_out, 'proc_macros')

      if nested_out:
        output.append('    %s: {' % self.name)
        for line in nested_out:
          output.append('    %s' % line)
        output.append('    },')

    def _output_field(self,
                      output,
                      name,
                      sort=True,
                      list_to_multiline_string=False,
                      is_cflags_like=False):
      value = getattr(self, name)
      if sort and is_cflags_like:
        value = sorted_cflags(value)
        sort = False
      return write_blueprint_key_value(
          output,
          name,
          value,
          sort=sort,
          list_to_multiline_string=list_to_multiline_string)

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
    self.host_cross_supported = True
    self.device_supported = True
    self.init_rc = set()
    self.out = set()
    self.export_include_dirs = set()
    self.generated_headers = set()
    self.export_generated_headers = set()
    self.export_static_lib_headers = set()
    self.export_header_lib_headers = set()
    self.defaults = set()
    self.cflags = list()
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
    self.wrapper_src = ""
    self.source_stem = ""
    self.bindgen_flags = set()
    self.handle_static_inline = None
    self.static_inline_library = ""
    self.jni_zero_target_type = None
    self.unstable = ""
    self.path = ""
    self.post_processed = False
    # In the case of Java "top-level" modules, this points to the corresponding
    # "unfiltered" module. The top-level module is just a dependency holder;
    # it's the unfiltered module that does the actual compiling. For more
    # details, see `create_java_module()`.
    self.java_unfiltered_module = None
    self.transitive_generated_headers_modules = collections.defaultdict(set)
    self.cargo_env_compat = None
    self.cargo_pkg_version = None
    self.whole_program_vtables = False

  def variant(self, arch_name):
    return self if arch_name == 'common' else self.target[arch_name]

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
    self._output_field(output, 'cmd', sort=False, list_to_multiline_string=True)
    if self.host_supported:
      self._output_field(output, 'host_supported')
    if not self.host_cross_supported:
      self._output_field(output, 'host_cross_supported')
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
    self._output_field(output, 'cflags', is_cflags_like=True)
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
    self._output_field(output, 'ldflags', is_cflags_like=True)
    self._output_field(output, 'cppflags', is_cflags_like=True)
    self._output_field(output, 'unstable')
    self._output_field(output, 'path')
    self._output_field(output, 'libs')
    self._output_field(output, 'stem')
    self._output_field(output, 'compile_multilib')
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
    self._output_field(output, 'source_stem')
    self._output_field(output, 'bindgen_flags')
    self._output_field(output, 'wrapper_src')
    self._output_field(output, 'handle_static_inline')
    self._output_field(output, 'static_inline_library')
    self._output_field(output, 'cargo_env_compat')
    self._output_field(output, 'cargo_pkg_version')
    if self.whole_program_vtables:
      self._output_field(output, 'whole_program_vtables')
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
    if self.type == 'cc_binary_host':
      raise Exception('Adding Android shared lib for host tool is unsupported')

    if self.host_supported:
      self.target['android'].shared_libs.add(lib)
    else:
      self.shared_libs.add(lib)

  def is_test(self):
    if gn_utils.TESTING_SUFFIX in self.name:
      name_without_prefix = self.name[:self.name.find(gn_utils.TESTING_SUFFIX)]
      return any(name_without_prefix == label_to_module_name(target)
                 for target in gn2bp_targets.DEFAULT_TESTS)
    return False

  def _output_field(self,
                    output,
                    name,
                    sort=True,
                    list_to_multiline_string=False,
                    is_cflags_like=False):
    value = getattr(self, name)
    if sort and is_cflags_like:
      value = sorted_cflags(value)
      sort = False
    return write_blueprint_key_value(
        output,
        name,
        value,
        sort=sort,
        list_to_multiline_string=list_to_multiline_string)

  def is_compiled(self):
    return self.type not in ('cc_genrule', 'filegroup', 'java_genrule')

  def is_genrule(self):
    return self.type == "cc_genrule"

  def has_input_files(self):
    if self.type in ["java_library", "java_import", "rust_bindgen"]:
      return True
    if len(self.srcs) > 0:
      return True
    if any(len(target.srcs) > 0 for target in self.target.values()):
      return True
    # Allow cc_static_library with export_generated_headers as those are crucial for
    # the depending modules
    return len(self.export_generated_headers) > 0 or len(
        self.generated_headers) > 0

  def is_java_top_level_module(self):
    return self.java_unfiltered_module is not None


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


def label_to_module_name(label, short=False):
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

  if not module.startswith(MODULE_PREFIX):
    return MODULE_PREFIX + module
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
        args_mapping[rust_flag_split[0]].add("=".join(rust_flag_split[1:]))
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
      module.cfgs.add(cfg.replace("\"", "\\\""))

  pre_filter_flags = []
  for (key, values) in rust_flags_dict.items():
    if values is None:
      pre_filter_flags.append(key)
    else:
      pre_filter_flags.extend(f"{key}={param_val}" for param_val in values)

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


def get_protoc_module_name(gn):
  # Note we use Chromium's protoc, not AOSP's. AOSP protoc does not work for us
  # because that would require us to link against AOSP's protobuf C++ runtime
  # library as well (libprotobuf-cpp-lite) as the generated code is coupled to
  # the runtime library. Problem is, the protobuf C++ runtime library uses the
  # C++ STL extensively in its public API (e.g. public functions taking
  # std::string). Because libc++ does not guarantee ABI compatibility, this in
  # turn means that both the producer (libprotobuf-cpp-lite) and the consumer
  # (Cronet) of the API must link against the same libc++. Unfortunately that is
  # not currently the case - libprotobuf-cpp-lite links against AOSP libc++,
  # while Cronet links against its own libc++ from Chromium. Therefore we cannot
  # use the AOSP protobuf library - we have to use the Chromium one.
  protoc_gn_target_name = gn.get_target('//third_party/protobuf:protoc').name
  return label_to_module_name(protoc_gn_target_name)


def create_rust_cxx_modules(blueprint, gn, target, is_test_target):
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
        The source and headers genrule modules.
  """

  def _find_cxx_bridge_binary(deps: Set[str]) -> str:
    for dep in deps:
      if re.search(
          r"^//third_party/rust/cxxbridge_cmd/v.*:cxxbridge(__testing)?$", dep):
        return dep
    raise Exception(
        f"Failed to find a dependency on cxxbridge host binary! Target name: {target.name}, deps: {deps}",
    )

  cxx_bridge_module_name = create_modules_from_target(
      blueprint, gn, _find_cxx_bridge_binary(target.deps), target.type,
      is_test_target)[0].name
  header_genrule = Module("cc_genrule",
                          label_to_module_name(target.name) + "_header",
                          target.name)
  header_genrule.tools = {cxx_bridge_module_name}
  header_genrule.cmd = f"$(location {cxx_bridge_module_name}) $(in) --header > $(out)"
  header_genrule.srcs = {gn_utils.label_to_path(src) for src in target.sources}
  # The output of the cc_genrule is the input + ".h" suffix, this is because
  # the input to a CXX genrule is just one source file.
  header_genrule.out = {
      f"{gn_utils.label_to_path(out)}.h"
      for out in target.sources
  }

  cc_genrule = Module("cc_genrule", label_to_module_name(target.name),
                      target.name)
  cc_genrule.tools = {cxx_bridge_module_name}
  cc_genrule.cmd = f"$(location {cxx_bridge_module_name}) $(in) > $(out)"
  cc_genrule.srcs = {gn_utils.label_to_path(src) for src in target.sources}
  cc_genrule.genrule_srcs = {f":{cc_genrule.name}"}
  # The output of the cc_genrule is the input + ".cc" suffix, this is because
  # the input to a CXX genrule is just one source file.
  cc_genrule.out = {
      f"{gn_utils.label_to_path(out)}.cc"
      for out in target.sources
  }

  cc_genrule.genrule_headers.add(header_genrule.name)
  return (header_genrule, cc_genrule)


def create_proto_modules(blueprint, gn, target, is_test_target):
  """Generate genrules for a proto GN target.

    GN actions are used to dynamically generate files during the build. The
    Soong equivalent is a genrule. This function turns a specific kind of
    genrule which turns .proto files into source and header files into a pair
    equivalent genrules.

    Args:
        blueprint: Blueprint instance which is being generated.
        target: gn_utils.Target object.

    Returns:
        The .h and .cc genrule modules.
    """
  assert (target.type == 'proto_library')

  if any(output.endswith('.descriptor') for output in target.outputs):
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
    for arch in target.arch.values():
      args = arch.args
      if not args:
        continue
      arg_count = args.count(arg_name)
      if arg_count == 0:
        arch_values.add(None)
        continue
      assert arg_count == 1, (arg_name, target.name, arch_name)
      value_index = args.index(arg_name) + 1
      assert (value_index < len(args)), (arg_name, target.name, arch_name)
      arch_value = args[value_index]
      if sanitize is not None:
        arch_value = sanitize(arch_value)
      arch_values.add(arch_value)
    assert len(arch_values) == 1, (target.name, arg_name, arch_values)
    (single_value, ) = arch_values
    return single_value

  protoc_module_name = get_protoc_module_name(gn) + (gn_utils.TESTING_SUFFIX
                                                     if is_test_target else '')
  # Bring in any executable binary dependencies. Typically these would be protoc
  # plugins (more on plugins below).
  tools = {protoc_module_name} | {
      dep_module.name
      for dep_modules in (create_modules_from_target(
          blueprint, gn, dep, target.type, is_test_target)
                          for dep in target.deps)
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
  target_module_name = label_to_module_name(target.name, short=True)

  # In GN builds the proto path is always relative to the output directory
  # (out/tmp.xxx).
  cmd = ['$(location %s)' % protoc_module_name]
  cmd += ['--proto_path=%s/%s' % (tree_path, target.proto_in_dir)]

  sorted_proto_paths = sorted(target.proto_paths)
  for proto_path in sorted_proto_paths:
    cmd += [f'--proto_path={tree_path}/{proto_path}']
  if buildtools_protobuf_src in sorted_proto_paths:
    cmd += ['--proto_path=%s' % android_protobuf_src]

  sources = {gn_utils.label_to_path(src) for src in target.sources}
  absolute_sources = sorted(
      [f"external/cronet/{IMPORT_CHANNEL}/{src}" for src in sources])

  # We create two genrules for each proto target: one for the headers and
  # another for the sources. This is because the module that depends on the
  # generated files needs to declare two different types of dependencies --
  # source files in 'srcs' and headers in 'generated_headers' -- and it's not
  # valid to generate .h files from a source dependency and vice versa.
  source_module_name = target_module_name
  source_module = Module('cc_genrule', source_module_name, target.name)
  blueprint.add_module(source_module)
  source_module.srcs.update(sources)

  header_module = Module('cc_genrule', source_module_name + '_h', target.name)
  blueprint.add_module(header_module)
  header_module.srcs = set(source_module.srcs)

  header_module.export_include_dirs = {'.', 'protos'}
  # Since the .cc file and .h get created by a different gerule target, they
  # are not put in the same intermediate path, so local includes do not work
  # without explictily exporting the include dir.
  header_module.export_include_dirs.add(cpp_out_dir)

  # This function does not return header_module so setting apex_available attribute here.
  header_module.apex_available.add(tethering_apex)

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

  source_module.out.update(output for output in target.outputs
                           if output.endswith('.cc'))
  header_module.out.update(output for output in target.outputs
                           if output.endswith('.h'))

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
  return (header_module, source_module)


def create_gcc_preprocess_modules(blueprint, target):
  # gcc_preprocess.py internally execute host gcc which is not allowed in genrule.
  # So, this function create multiple modules and realize equivalent processing
  assert (len(target.sources) == 1)
  source = list(target.sources)[0]
  assert (Path(source).suffix == '.template')
  stem = Path(source).stem

  bp_module_name = label_to_module_name(target.name)

  # Rename .template to .cc since cc_preprocess_no_configuration does
  # not accept .template file as srcs
  rename_module = Module('genrule', bp_module_name + '_rename', target.name)
  rename_module.srcs.add(gn_utils.label_to_path(source))
  rename_module.out.add(stem + '.cc')
  rename_module.cmd = 'cp $(in) $(out)'
  blueprint.add_module(rename_module)

  # Preprocess template file and generates java file
  preprocess_module = Module('cc_preprocess_no_configuration',
                             bp_module_name + '_preprocess', target.name)
  # -E: stop after preprocessing.
  # -P: disable line markers, i.e. '#line 309'
  preprocess_module.cflags.extend(['-E', '-P', '-DANDROID'])
  preprocess_module.srcs.add(':' + rename_module.name)
  defines = [
      '-D' + target.args[i + 1] for i, arg in enumerate(target.args)
      if arg == '--define'
  ]
  preprocess_module.cflags.extend(defines)
  blueprint.add_module(preprocess_module)

  # Generates srcjar using soong_zip
  module = Module('genrule', bp_module_name, target.name)
  module.srcs.add(':' + preprocess_module.name)
  module.out.add(stem + '.srcjar')
  module.cmd = [
      f'cp $(in) $(genDir)/{stem}.java &&',
      f'$(location soong_zip) -o $(out) -srcjar -C $(genDir) -f $(genDir)/{stem}.java'
  ]
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
    normalized_args = []
    for arg in self.target.args:
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
    return [i for i, arg in enumerate(self.target.args) if arg == target_arg]

  # Whether an arg value pair appears once or more times
  def _is_list_arg(self, arg):
    indices = self._get_arg_indices(arg)
    return len(indices) > 0 and all(not self.target.args[i + 1].startswith('--')
                                    for i in indices)

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
    return pre_cmd

  def get_base_cmd(self):
    # TODO: most sanitizer logic does not really handle "$" characters very
    # well, and will likely do the wrong thing if the GN target contains args
    # with literal "$" characters in them. Also, if a sanitizer deliberately
    # shoves a $() macro in an arg, we still run that through shell quoting,
    # which does preserve the "$" but that's mostly luck. We should design
    # a better mechanism for handling "$" and $() macros.
    return (([f"echo {shlex.quote(self.target.response_file_contents)} |"]
             if self.target.response_file_contents else []) +
            [f"$(location {gn_utils.label_to_path(self.target.script)})"] +
            [shlex.quote(arg) for arg in self.target.args])

  def get_cmd(self):
    # Note: don't be confused by the return type. This function returns a list,
    # but the list is *NOT* an argv array, it's a list of lines for Blueprint
    # file formatting for cosmetic purposes. The actual command is the list
    # elements concatenated together into a single string, which is ultimately
    # fed as a shell command at build time. This means what we are returning
    # here is expected to have been properly shell-escaped beforehand.
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
        for file in files
        if not is_supported_source_file(file) and not file.startswith("//out/")
    }
    tool_files.add(gn_utils.label_to_path(self.target.script))
    # Make sure there is no duplication between `srcs` and `tool_files` - Soong
    # fails with a "multiple locations for label" error otherwise.
    tool_files -= self.get_srcs()
    return tool_files

  def _sanitize_args(self):
    # Handle passing parameters via response file by piping them into the script
    # and reading them from /dev/stdin.

    use_response_file = gn_utils.RESPONSE_FILE in self.target.args
    if use_response_file:
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
    return any(os.path.splitext(it)[1] == '.h' for it in self.target.outputs)


class WriteBuildDateHeaderSanitizer(BaseActionSanitizer):

  def _sanitize_args(self):
    self._set_arg_at(0, '$(out)')
    super()._sanitize_args()


class WriteBuildFlagHeaderSanitizer(BaseActionSanitizer):

  def _sanitize_args(self):
    self._set_value_arg('--gen-dir', '.')
    self._set_value_arg('--output', '$(out)')
    super()._sanitize_args()


class PerfettoWriteBuildFlagHeaderSanitizer(BaseActionSanitizer):

  def _sanitize_args(self):
    self._set_value_arg('--out', '$(out)')
    super()._sanitize_args()


class GnRunBinarySanitizer(BaseActionSanitizer):

  def __init__(self, target, arch):
    super().__init__(target, arch)
    self.binary_to_target = {
        "clang_x64/transport_security_state_generator":
        f"{MODULE_PREFIX}net_tools_transport_security_state_generator_transport_security_state_generator__testing",
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
    self.target.args = [arg for arg in self.target.args if "python3" not in arg]

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
    return self.get_pre_cmd() + [shlex.quote(arg) for arg in self.target.args]


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
    self._set_value_arg('--jar-file', '$(location :current_android_jar)', False)
    if self._has_arg('--jar-file'):
      self._set_value_arg('--javap', '$(location :javap)')
    self._update_value_arg('--srcjar-path', self._sanitize_filepath, False)
    self._update_value_arg('--output-dir', self._sanitize_filepath)
    self._update_value_arg('--extra-include', self._sanitize_filepath, False)
    self._update_value_arg('--placeholder-srcjar-path', self._sanitize_filepath,
                           False)
    self._update_list_arg('--input-file', self._sanitize_filepath)
    self._update_list_arg('--input-file', self._add_location_tag_to_filepath)

    self._delete_value_arg('--package-prefix', throw_if_absent=False)
    self._delete_value_arg('--package-prefix-filter', throw_if_absent=False)
    if not self.is_test_target and not self._has_arg('--jar-file'):
      # Don't jarjar classes that already exists within the java SDK. The headers generated
      # from those genrule can simply call into the original class as it exists outside
      # of cronet's jar.
      # Only jarjar platform code
      self._append_arg('--package-prefix', 'android.net.http.internal')
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
        file if not file.endswith('android.jar') else ':current_android_jar'
        for file in tool_files
    }
    # Filter bin/javap
    tool_files = {file for file in tool_files if not file.endswith('bin/javap')}

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
    self.target.outputs = [out for out in outputs if out.endswith(".srcjar")]
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
        file for file in self.target.inputs if not file.startswith('//out/')
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
    self._update_value_arg('--placeholder-srcjar-path', self._sanitize_filepath,
                           False)
    self._delete_value_arg('--depfile', False)
    self._set_value_arg('--java-sources-file', '$(genDir)/java.sources')

    self._delete_value_arg('--package-prefix', throw_if_absent=False)
    self._delete_value_arg('--package-prefix-filter', throw_if_absent=False)
    if not self.is_test_target:
      # Only jarjar platform code
      self._append_arg('--package-prefix', 'android.net.http.internal')
    super()._sanitize_args()

  def get_cmd(self):
    base_cmd = super().get_base_cmd()
    # Path in the original sources file does not work in genrule.
    # So creating sources file in cmd based on the srcs of this target.
    # Adding ../$(current_dir)/ to the head because jni_registration_generator.py uses the files
    # whose path startswith(..)
    base_cmd = ([
        "current_dir=`basename \\`pwd\\``;",
        "for f in $(in);",
        "do",
        "echo \"../$$current_dir/$$f\" >> $(genDir)/java.sources;",
        "done;",
    ] +
                # jni_registration_generator.py doesn't work with python2
                [f"python3 {base_cmd[0]}"] + base_cmd[1:])

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

  def get_outputs(self):
    return [out for out in super().get_outputs() if out.endswith(".srcjar")]

  def get_deps(self):
    return {}


class VersionSanitizer(BaseActionSanitizer):

  def _sanitize_args(self):
    self._set_value_arg('-o', '$(out)')
    # args for the version.py contain file path without leading --arg key. So apply sanitize
    # function for all the args.
    self._update_all_args(self._sanitize_filepath_with_location_tag)
    super()._sanitize_args()

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


class CopyActionSanitizer(BaseActionSanitizer):

  def get_tool_files(self):
    # CopyAction makes use of no tools, it simply relies on cp.
    return set()

  def get_cmd(self):
    return (super().get_pre_cmd() + ['cp'] +
            [shlex.quote(arg) for arg in self.target.args])

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
    return set(f':{label_to_module_name(dep)}' for dep in deps)

  def sanitize(self):
    # By convention, copy targets use their deps as args for the copy (see get_srcs).
    if len(self.target.args) > 1:
      raise Exception(
          f'CopyAction {self.target.name} specifies {self.target.args=}. Only deps are supported'
      )
    self.target.args = [f'$(location {src})' for src in self.get_srcs()]
    self.target.args.append('$(out)')
    super().sanitize()


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

  def _sanitize_inputs(self):
    super()._sanitize_inputs()
    # https://crrev.com/c/5840231 adds
    #   //third_party/android_build_tools/protoc/cipd/protoc
    # to the input list. We don't import that protoc prebuilt binary; instead we
    # build protoc from source from //third_party/protobuf:protoc. We don't
    # need to add that as an input because it's already a tool dependency in
    # the generated module.
    self.target.inputs.remove(
        "//third_party/android_build_tools/protoc/cipd/protoc")

  def get_tools(self):
    tools = super().get_tools()
    tools.add(self._protoc)
    return tools


def get_action_sanitizer(gn, target, gn_type, arch, is_test_target):
  if target.script == "//build/write_buildflag_header.py" or target.script == "//base/allocator/partition_allocator/src/partition_alloc/write_buildflag_header.py":
    # PartitionAlloc has forked the same write_buildflag_header.py script from
    # Chromium to break its dependency on //build.
    return WriteBuildFlagHeaderSanitizer(target, arch)
  if target.script == "//third_party/perfetto/gn/write_buildflag_header.py":
    return PerfettoWriteBuildFlagHeaderSanitizer(target, arch)
  if target.script == "//base/write_build_date_header.py":
    return WriteBuildDateHeaderSanitizer(target, arch)
  if target.script == "//build/util/version.py":
    return VersionSanitizer(target, arch)
  if target.script == "//build/android/gyp/java_cpp_enum.py":
    return JavaCppEnumSanitizer(target, arch)
  if target.script == "//net/tools/dafsa/make_dafsa.py":
    return MakeDafsaSanitizer(target, arch)
  if target.script == '//build/android/gyp/java_cpp_features.py':
    return JavaCppFeatureSanitizer(target, arch)
  if target.script == '//build/android/gyp/java_cpp_strings.py':
    return JavaCppStringSanitizer(target, arch)
  if target.script == '//build/android/gyp/write_native_libraries_java.py':
    return WriteNativeLibrariesJavaSanitizer(target, arch)
  if target.script == '//cp':
    return CopyActionSanitizer(target, arch)
  if target.script == '//build/gn_run_binary.py':
    return GnRunBinarySanitizer(target, arch)
  if target.script == '//build/protoc_java.py':
    return ProtocJavaSanitizer(target, arch, gn)
  if jni_zero_target_type := get_jni_zero_target_type(target):
    if jni_zero_target_type == JniZeroTargetType.REGISTRATION_GENERATOR:
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
          target.sources.update(gn.jni_java_sources)
        return JavaJniRegistrationGeneratorSanitizer(target, arch,
                                                     is_test_target)
      return JniRegistrationGeneratorSanitizer(target, arch, is_test_target)
    if gn_type == 'cc_genrule':
      return JniGeneratorSanitizer(target, arch, is_test_target)
    return JavaJniGeneratorSanitizer(target, arch, is_test_target)
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

  # We create one genrule per individual source, with numbered names (e.g.
  # "foo_0", "foo_1", etc.).
  # Note: currently we return the collection of the resulting genrules, instead
  # of a single module. Arguably this is a bit cumbersome. We could centralize
  # the outputs into a single "cp everything" genrule so that dependent modules
  # only have to depend on a single module.

  def create_subtarget(i, src):
    subtarget = copy.deepcopy(target)
    subtarget.name += f"_{i}"
    subtarget.sources = {src}
    new_args = []
    for arg in target.args:
      if '{{source}}' in arg:
        new_args.append('$(location %s)' % (gn_utils.label_to_path(src)))
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
            subtarget.outputs = {out}

        for file in (target.sources | target.inputs):
          if file.endswith(file_name):
            new_args.append('$(location %s)' % gn_utils.label_to_path(file))
      else:
        new_args.append(arg)
    subtarget.args = new_args
    return subtarget

  return [
      create_action_module(blueprint, gn, create_subtarget(i, src),
                           'cc_genrule', is_test_target)
      for i, src in enumerate(sorted(target.sources))
  ]


def create_action_module_internal(gn,
                                  target,
                                  gn_type,
                                  is_test_target,
                                  blueprint,
                                  arch=None):
  if target.script == '//build/android/gyp/gcc_preprocess.py':
    return create_gcc_preprocess_modules(blueprint, target)
  sanitizer = get_action_sanitizer(gn, target, gn_type, arch, is_test_target)
  sanitizer.sanitize()

  module = Module(gn_type, sanitizer.get_name(), target.name)
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
    raise Exception(f'{genrule_type} can not have different cmd between archs')

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
          f'{merged_module.name} has different values for {key} between archs')

  merged_module.cmd = merge_cmd(modules, genrule_type)
  return merged_module


def create_java_module(bp_module_name, target, blueprint):

  def add_java_library_properties(module):
    module.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
    module.apex_available = [tethering_apex]
    module.defaults.add(java_framework_defaults_module)
    module.build_file_path = target.build_file_path

  # As hinted in `parse_gn_desc()`, Java GN targets are... complicated.
  #
  # Here the main source of complexity is the need to support the
  # jar_excluded_patterns and jar_included_patterns options of Chromium's
  # `java_library()` GN rule. Most java_library targets don't really use this
  # feature, but there are notable exceptions: for example some jni_zero
  # generator targets rely on this to remove placeholder classes, which would
  # conflict with the real classes otherwise.
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

  sources = target.sources
  source_is_jar = any(source.endswith('.jar') for source in sources)
  unfiltered_module = Module("java_import" if source_is_jar else "java_library",
                             f"{bp_module_name}__unfiltered", target.name)
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
  filtered_module = Module("java_genrule", f"{bp_module_name}__filtered",
                           target.name)
  filtered_module.srcs = [f":{unfiltered_module.name}"]

  jar_excluded_patterns = target.java_jar_excluded_patterns
  # HACK: don't strip the placeholder org.chromium.build.NativeLibraries from
  # //build/android:build_java, as we don't generate the real one.
  # TODO(https://crbug.com/405373567): generate a proper NativeLibraries
  # instead.
  if target.name in ("//build/android:build_java",
                     "//build/android:build_java__testing"):
    jar_excluded_patterns = [
        jar_excluded_pattern for jar_excluded_pattern in jar_excluded_patterns
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

  top_module = Module("java_library", bp_module_name, target.name)
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
  if "--bindgen-flags" not in args:
    return []

  bindgen_flags = []
  for arg in args[args.index("--bindgen-flags") + 1:]:
    if arg.startswith("--"):
      # This is a new argument for the python script and not a bindgen argument.
      break
    bindgen_flags.append("--" + arg)

  return bindgen_flags


def create_bindgen_module(blueprint: Blueprint, target,
                          module_name: str) -> Module:
  module = Module("rust_bindgen", "lib" + module_name, target.name)
  if len(target.sources) > 1:
    raise ValueError(
        f"Expected a single source file for bindgen but found {target.sources}."
    )

  if len(target.outputs) > 2:
    raise ValueError(
        f"Expected at most two output files for bindgen but found {target.outputs}"
    )
  module.wrapper_src = gn_utils.label_to_path(list(target.sources)[0])
  module.crate_name = module_name

  if "c++" in target.args:
    # This is defined in the rust_bindgen templates where "C++" will
    # be added to the args if `cpp` field is defined. Soong depends
    # on `cpp_std` field to identify that this is a C++ header.
    module.cpp_std = CPP_VERSION

  module.source_stem = get_bindgen_source_stem(target.outputs)

  if "--wrap-static-fns" in target.args:
    module.handle_static_inline = True

  module.bindgen_flags = get_bindgen_flags(target.args)
  # This ensures that any CC file that is being processed through the
  # rust_bindgen module is able to #include files relative to the root of the
  # repository.
  #
  # Note: this module is not part of the generated build rules; it is expected
  # to already be present in AOSP (currently, in Android.extras.bp). See
  # https://r.android.com/3413202.
  module.header_libs = {f"{MODULE_PREFIX}repository_root_include_dirs_anchor"}
  module.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
  module.apex_available = [tethering_apex]
  blueprint.add_module(module)
  return module


def create_generated_headers_export_module(blueprint: Blueprint,
                                           cc_genrule_module: Module) -> Module:
  '''
  Creates a cc_library_headers module that merely re-exports headers that are
  generated by a cc_genrule module. This is useful in scenarios where a module
  has no way of directly depending on generated headers.
  '''
  cc_genrule_module_name = cc_genrule_module.name
  module = Module("cc_library_headers",
                  f"{cc_genrule_module_name}_export_generated_headers",
                  cc_genrule_module.gn_target)
  module.export_generated_headers = module.generated_headers = [
      cc_genrule_module_name
  ]
  module.build_file_path = cc_genrule_module.build_file_path
  module.defaults = [cc_defaults_module]
  module.host_supported = cc_genrule_module.host_supported
  module.host_cross_supported = cc_genrule_module.host_cross_supported
  blueprint.add_module(module)
  return module


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
  if re.match('//build/android:native_libraries_gen(__testing)?$', target.name):
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


def create_jni_zero_proxy_only_module(jni_zero_generator_module):
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
  assert jni_zero_generator_module.jni_zero_target_type == JniZeroTargetType.GENERATOR
  proxy_path, _ = get_jni_zero_generator_proxy_and_placeholder_paths(
      jni_zero_generator_module)

  proxy_only_module = Module(jni_zero_generator_module.type,
                             f"{jni_zero_generator_module.name}_proxy_only",
                             jni_zero_generator_module.gn_target)
  proxy_only_module.cmd = "cp $(in) $(genDir)"
  proxy_only_module.srcs = [f":{jni_zero_generator_module.name}"]
  proxy_only_module.out = [os.path.basename(proxy_path)]

  return proxy_only_module


def _get_cflags(cflags, defines):
  cflags = [flag for flag in cflags if flag in cflag_allowlist]

  # Android _may_ set a platform default for _LIBCPP_HARDENING_MODE. If that
  # conflicts with the level specified on this target, we'll get build errors.
  #
  # Allow Android's default to apply to builds where we don't specify one, but
  # prefer our default for builds that do.
  libcpp_hardening_flag = "_LIBCPP_HARDENING_MODE"
  if any(define.startswith(libcpp_hardening_flag) for define in defines):
    cflags.append(f"-U{libcpp_hardening_flag}")

  # Consider proper allowlist or denylist if needed
  cflags.extend(["-D%s" % define.replace("\"", "\\\"") for define in defines])
  return cflags


def _set_linker_script(module, libs):
  for lib in libs:
    if lib.endswith(".lds"):
      module.ldflags.add(get_linker_script_ldflag(gn_utils.label_to_path(lib)))


def create_concatenated_generated_headers_module(bp_module_name,
                                                 headers_modules, blueprint,
                                                 gn_target_name):
  """Aggregates the output of multiple generated_headers genrules into a single
  one. This is created to shorten the command-line length of the build command
  as to not exceed the allowed length. Instead of exposing each generated header
  individually, they're combined into a single target and only that target is
  exposed.

  Args:
    bp_module_name: Name of the aggregated module generated.
    headers_modules: Set of generated headers modules that will be aggregated.
    gn_target_name: Name of the original GN target. This is usually the name
    of the cc_library_static that is being processed.

  Returns:
    A Soong Module that aggregates all of the headers.
  """
  module = Module("cc_genrule", bp_module_name, gn_target_name)
  module.cmd = [
      "python $(location components/cronet/gn2bp/headers_copy.py) --gen-dir $(genDir) --headers"
  ]
  module.tool_files.add("components/cronet/gn2bp/headers_copy.py")
  for headers_module_name in sorted(headers_modules):
    headers_module_str = f":{headers_module_name}"
    headers_module = blueprint.modules[headers_module_name]
    module.tool_files.add(headers_module_str)
    module.export_include_dirs.update(headers_module.export_include_dirs)
    module.cmd.append(f"$(locations {headers_module_str})")
    # We have to copy-over some .cc files due to some C++ code doing #include "file.cc". See
    # crbug.com/421139881 for more information.
    module.out.update([
        output for output in headers_module.out
        if output.endswith(".h") or output.endswith(".cc")
    ])
  module.apex_available.add(tethering_apex)
  blueprint.add_module(module)
  return module


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


def _extract_linker_script(ldflags):
  new_ldflags = set()
  linker_script = None
  for flag in ldflags:
    if flag.startswith("-Wl,--version-script="):
      # Everything after the = is the path and delete all leading ../
      linker_path = re.sub('^(\.\./)+', '', flag.split("=", maxsplit=2)[1])
      assert linker_script is None, f"Found two different linker script for a single target! First script: {linker_script}, Second script: {linker_path}"
      linker_script = linker_path
    else:
      new_ldflags.add(flag)
  return new_ldflags, linker_script


def _create_linker_script_filegroup(linker_script_path):
  filegroup_name = linker_script_path.replace('/', '_').replace('.', '_')
  filegroup_module = Module("filegroup", f"{filegroup_name}_filegroup",
                            f"Created to reference {linker_script_path}")
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
      "-Wl,--strip-debug"
  ])


def configure_cc_module(module, cflags, defines, ldflags, libs, main_module,
                        blueprint):
  module.cflags.extend(_get_cflags(cflags, defines))
  ldflags, linker_script = _extract_linker_script(ldflags)
  module.ldflags.update({flag for flag in ldflags if _is_allowed_ldflag(flag)})
  if linker_script:
    # Unfortunately, Soong does not allow accessing linker scripts from parent
    # path. So create a filegroup at the top-level Android.bp and reference it instead.
    filegroup_module = _create_linker_script_filegroup(linker_script)
    blueprint.add_module(filegroup_module)
    module.version_script = f":{filegroup_module.name}"
  _set_linker_script(module, libs)
  for lib in libs:
    # Generally library names should be mangled as 'libXXX', unless they
    # are HAL libraries (e.g., android.hardware.health@2.0) or AIDL c++ / NDK
    # libraries (e.g. "android.hardware.power.stats-V1-cpp")
    android_lib = lib if '@' in lib or "-cpp" in lib or "-ndk" in lib \
      else 'lib' + lib
    if lib in shared_library_allowlist:
      module.shared_libs.add(android_lib)
  # TODO: implement proper cflag parsing.
  for flag in cflags:
    if '-fexceptions' in flag:
      module.cppflags.add('-fexceptions')
  cpp_std = _get_cpp_std(cflags)
  if cpp_std:
    assert main_module.cpp_std is None or main_module.cpp_std == cpp_std, f"Found different CPP version across different architectures!, target name: {main_module.name}, first cpp version: {main_module.cpp_std}, current cpp version: {cpp_std}"
    # The -std= compiler option has a dedicated property in Android.bp, called cpp_std. That property
    # can only be set at module top level; it cannot be set per-target. However in GN
    # cflags are arch-specific, so we will find -std= when running on the
    # arch-specific module. Hence we need to go back to the main module and set it there.
    main_module.cpp_std = cpp_std


def _create_rust_build_script_output_copy_genrule(module_name,
                                                  path_to_directory, files):
  module = Module(
      "genrule", module_name,
      "Copies generated Rust build script files somewhere the dependent code can find them"
  )
  module.srcs = [f"{path_to_directory}/{file_name}" for file_name in files]
  module.cmd = "cp $(in) $(genDir)"
  module.out = files
  return module

def set_module_include_dirs(module, cflags, include_dirs):
  for flag in cflags:
    if '-isystem' in flag:
      module.include_dirs.add(
          f"external/cronet/{IMPORT_CHANNEL}/{flag[len('-isystem../../'):]}")

  depends_on_binder_ndk = any("libbinder_ndk_cpp" in include_dir
                              for include_dir in include_dirs)
  if depends_on_binder_ndk:
    module.shared_libs.add("libbinder_ndk")
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
  module.include_dirs.update([
      f"external/cronet/{IMPORT_CHANNEL}/{gn_utils.label_to_path(d)}"
      for d in include_dirs if not d.startswith('//out')
  ])
  # Remove prohibited include directories
  module.include_dirs = [
      d for d in module.include_dirs if d not in include_dirs_denylist
  ]


def create_aidl_module(bp_module_name, target, blueprint):
  module = Module("aidl_interface", bp_module_name, target.name)
  module.unstable = True
  module.include_dirs = [
      f"external/cronet/{IMPORT_CHANNEL}/{path}"
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
  filegroup_module = Module("filegroup", filegroup_module_name, target.name)
  filegroup_module.srcs = [
      gn_utils.label_to_path(src) for src in sorted(target.sources)
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

def create_modules_from_target(blueprint, gn, gn_target_name, parent_gn_type,
                               is_test_target):
  """Generate module(s) for a given GN target.

    Given a GN target name, generate one or more corresponding modules into a
    blueprint. Most of the time this will only generate one module, with some
    exceptions such as protos and rust cxxbridge generation.

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
        bp_module_name.encode('utf-8')).hexdigest()[:2]
    bp_module_name = f"lib{target.crate_name}__{bp_module_hash}"

  if bp_module_name in blueprint.modules:
    return (blueprint.modules[bp_module_name], )

  log.info('create modules for %s (%s)', target.name, target.type)

  if gn2bp_common.is_rust_build_script(target.script):
    # Build scripts are generated via `generate_build_scripts_output.py`. See the header
    # of that script for more details.
    generated_files = [
        output.split("/")[-1] for output in target.outputs
        if output.endswith(".rs") and not output.endswith("/cargo_flags.rs")
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
        generated_files)
    blueprint.add_module(module)
    return (module, )

  if target.type == 'executable':
    if target.testonly:
      module_type = 'cc_test'
    else:
      # Can be used for both host and device targets.
      module_type = 'cc_binary'
    modules = (Module(module_type, bp_module_name, gn_target_name), )
  elif target.type == 'rust_executable':
    modules = (Module("rust_binary", bp_module_name, gn_target_name), )
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
    modules = (Module("rust_ffi_static", bp_module_name, gn_target_name), )
  elif target.type == "rust_proc_macro":
    modules = (Module("rust_proc_macro", bp_module_name, gn_target_name), )
  elif target.type in ['static_library', 'source_set']:
    modules = (Module('cc_library_static', bp_module_name, gn_target_name), )
  elif target.type == 'shared_library':
    modules = (Module('cc_library_shared', bp_module_name, gn_target_name), )
  elif target.type == 'proto_library':
    modules = create_proto_modules(blueprint, gn, target, is_test_target)
    if modules is None:
      return ()
  elif target.type == "rust_bindgen":
    modules = (create_bindgen_module(blueprint, target, bp_module_name), )
  elif target.type == 'action':
    module = create_action_module(
        blueprint, gn, target,
        'java_genrule' if parent_gn_type == "java_library" else 'cc_genrule',
        is_test_target)
    module.jni_zero_target_type = get_jni_zero_target_type(target)
    modules = (module, )
  elif target.type == 'action_foreach':
    if target.script == "//third_party/rust/cxx/chromium_integration/run_cxxbridge.py":
      modules = create_rust_cxx_modules(blueprint, gn, target, is_test_target)
    else:
      modules = create_action_foreach_modules(blueprint, gn, target,
                                              is_test_target)
  elif target.type == 'copy':
    # Copy targets are not supported: currently, we stop traversing the
    # dependency tree when we encounter one.
    return ()
  elif target.type == 'java_library':
    modules = (create_java_module(bp_module_name, target, blueprint), )
  elif target.type == 'aidl_interface':
    modules = create_aidl_module(bp_module_name, target, blueprint)
  else:
    # Note we don't have to handle `group` targets because parse_gn_desc() never
    # returns any; it just recurses through them and bubbles their dependencies
    # upwards.
    raise Exception('Unknown target %s (%s)' % (target.name, target.type))

  for module in modules:
    blueprint.add_module(module)
    if target.type not in ['action', 'action_foreach', 'aidl_interface']:
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
      configure_cc_module(module, target.cflags, target.defines, target.ldflags,
                          target.libs, module, blueprint)
      set_module_include_dirs(module, target.cflags, target.include_dirs)
      # TODO: set_module_xxx is confusing, apply similar function to module and target in better way.
      for arch_name, arch in target.get_archs().items():
        # TODO(aymanm): Make libs arch-specific.
        configure_cc_module(module.target[arch_name], arch.cflags, arch.defines,
                            arch.ldflags, arch.libs, module, blueprint)
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
    module.visibility.add("//external/cronet:__subpackages__")

    if module.type in ["rust_proc_macro", "rust_binary", "rust_ffi_static"]:
      module.crate_name = target.crate_name
      module.crate_root = gn_utils.label_to_path(target.crate_root)
      if target.rust_package_version:
        module.cargo_env_compat = True
        module.cargo_pkg_version = target.rust_package_version
      module.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
      module.apex_available = [tethering_apex]
      for arch_name, arch in target.get_archs().items():
        _set_rust_flags(module.target[arch_name], arch.rust_flags, arch_name)

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

    if module.is_genrule():
      module.apex_available.add(tethering_apex)

    if (module.is_compiled() and not module.type.startswith("java")
        and not module.type.startswith("rust")):
      # Don't try to inject library/source dependencies into genrules or
      # filegroups because they are not compiled in the traditional sense.
      module.defaults = [cc_defaults_module]

    if module.type == 'cc_library_shared':
      output_name = target.output_name
      if output_name is None:
        module.stem = 'lib' + target.get_target_name().removesuffix(
            gn_utils.TESTING_SUFFIX)
      elif output_name.startswith("cronet."):
        # The AOSP version of CronetLibraryLoader looks for the libcronet so
        # with an extra suffix. Make sure the shared library name matches what
        # the loader expects.
        module.stem = 'libmainline' + output_name
      else:
        module.stem = 'lib' + output_name

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
        builtin_deps[dep_name](module.java_unfiltered_module
                               if module.is_java_top_level_module() else module,
                               arch_name)
        continue

      for dep_module in create_modules_from_target(blueprint, gn, dep_name,
                                                   target.type, is_test_target):
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
            module.transitive_generated_headers_modules[arch_name].update(
                dep_module.transitive_generated_headers_modules[arch_name])
            # Deduplicating attributes from arch-specific ones into "common" is done on a
            # per-target basis: matching values from attributes are deduplicated via 'common' if they're present in all
            # architectures supported by a target. This leads to a deduplication which is
            # stable on a "per-target basis", but not "globally": being a common
            # attribute for a target X does not guarantee that it will also be for a target Y that depends on X
            # (Y could support more architecture than X).
            # A common scenario is a target that also build for hosts, but depend on targets
            # which do not: this dependency will not be present for arch_name == host,
            # but will be there for others. Now, due to the "deduplication mismatch"
            # mentioned above, module_target will be oblivious to the common attributes
            # which should be propagated into the arch-specific variants.
            module.transitive_generated_headers_modules[arch_name].update(
                dep_module.transitive_generated_headers_modules["common"])
            module_target.shared_libs.update(dep_module.shared_libs)
            module_target.header_libs.update(dep_module.header_libs)
          elif module.type in ('rust_ffi_static', 'rust_bindgen'):
            module_target.shared_libs.update(dep_module.shared_libs)
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
            module.generated_headers.add(f"{dep_module.name}-ndk-source")
            module.transitive_generated_headers_modules[arch_name].add(
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
                  create_generated_headers_export_module(blueprint,
                                                         dep_module).name)
            else:
              module.transitive_generated_headers_modules[arch_name].update(
                  dep_module.genrule_headers)
          module_target.srcs.update(dep_module.genrule_srcs)
          module_target.shared_libs.update(dep_module.genrule_shared_libs)
          module_target.header_libs.update(dep_module.genrule_header_libs)
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
          module_target.java_unfiltered_module.libs.add(
              dep_module.java_unfiltered_module.name)
          # As mentioned above, `libs` does not bubble up, so we have to
          # recurse and collect all the transitive dependencies ourselves. This
          # is not necessary when using `static_libs` as Soong does that for us
          # at build time.
          #
          # (You may wonder: "wait, doesn't Chromium already enforce that a Java
          # target list all the classes it refers to in its direct dependencies?
          # Why do we need to pull indirect dependencies then?" Well the problem
          # is javac needs to see some of the indirect dependencies in some
          # cases - see https://crbug.com/400952169#comment4 - which means the
          # direct dependencies may not be enough.)
          module_target.java_unfiltered_module.libs.update(
              dep_module.java_unfiltered_module.libs)
        elif dep_module.type in ['genrule', 'java_genrule']:
          if dep_module.jni_zero_target_type == JniZeroTargetType.GENERATOR:
            # TODO: we are special-casing jni_zero here. Ideally this should be
            # handled more generically, by making gn2bp understand the general
            # concept of a target depending on only a subset of the outputs of
            # an action.
            _, placeholder_path = get_jni_zero_generator_proxy_and_placeholder_paths(
                dep_module)
            if placeholder_path in target.inputs:
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
              proxy_only_module = create_jni_zero_proxy_only_module(dep_module)
              blueprint.add_module(proxy_only_module)
              module_target.srcs.add(f":{proxy_only_module.name}")
          else:
            module_target.srcs.add(":" + dep_module.name)
        else:
          raise Exception(
              'Unsupported arch-specific dependency %s of target %s with type %s'
              % (dep_module.name, target.name, dep_module.type))

    for arch_name, arch_generated_headers in module.transitive_generated_headers_modules.items(
    ):
      # We are capable of concatenating only internal dependencies (We don't know
      # what the output of external dependencies are).
      external_dependencies = {
          header_module
          for header_module in arch_generated_headers
          if header_module not in blueprint.modules.keys()
      }
      # Headers that are not generated via gn2bp should not be concatenated (e.g. aidl_interface).
      # Remove those from the set sent to `create_concatenated_generated_headers_module` while
      # keeping them in the transitive headers set to be propagated upward.
      headers_to_concatenate = arch_generated_headers - external_dependencies
      module.variant(arch_name).generated_headers.update(external_dependencies)
      if len(headers_to_concatenate) == 0:
        continue

      concatenated_hdrs_module = create_concatenated_generated_headers_module(
          f"{bp_module_name}__concatenated_headers_{arch_name}",
          headers_to_concatenate, blueprint, gn_target_name)
      concatenated_hdrs_module.host_supported = (arch_name == 'host'
                                                 or (arch_name == 'common'
                                                     and module.host_supported))
      # Disable cross host support for concatenated headers. By default all cc_genrule
      # modules disables this. However, this module is created manually which follows
      # a different codepath.
      if concatenated_hdrs_module.host_supported:
        concatenated_hdrs_module.host_cross_supported = False
      module.variant(arch_name).generated_headers.add(
          concatenated_hdrs_module.name)

    if module.is_java_top_level_module():
      # The Java top-level module is not the one doing the actual compiling; the
      # unfiltered module is, so it should get the srcs.
      module.java_unfiltered_module.srcs = module.srcs
      module.srcs = ()

    # post_processing has to be applied here as we need to ensure that the modules have the
    # correct properties in order to propagate them upward the tree. A common example is the
    # merging of intermediate headers into a single cc_genrule, the merging copies the `export_include_dirs`
    # of the descendant modules. However, if we apply the post_processing after we're done then it won't be
    # copied to the merged modules.
    apply_post_processing(module)

  return modules


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
      # TODO: this list is brittle and painful to maintain. We are too easily
      # broken by changes to Chromium cflags, e.g. https://crbug.com/406704769.
      # Ideally this list should be deduced from GN cflags.
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
  defaults.cpp_std = CPP_VERSION
  defaults.min_sdk_version = cronet_utils.MIN_SDK_VERSION_FOR_AOSP
  defaults.apex_available.add(tethering_apex)
  return defaults


def apply_post_processing(module):
  if module.post_processed:
    return
  for key, add_val in additional_args.get(module.name, []):
    curr = getattr(module, key)
    if add_val and isinstance(add_val, set) and isinstance(curr, set):
      curr.update(add_val)
    elif isinstance(curr, list):
      curr.extend(add_val)
    elif isinstance(add_val, str) and (not curr or isinstance(curr, str)):
      setattr(module, key, add_val)
    elif isinstance(add_val, bool) and (not curr or isinstance(curr, bool)):
      setattr(module, key, add_val)
    elif isinstance(add_val, dict) and isinstance(curr, dict):
      curr.update(add_val)
    elif add_val is None:
      setattr(module, key, None)
    elif isinstance(add_val[1], dict) and isinstance(curr[add_val[0]],
                                                     Module.Target):
      curr[add_val[0]].__dict__.update(add_val[1])
    elif isinstance(curr, dict):
      curr[add_val[0]] = add_val[1]
    else:
      raise Exception('Unimplemented type %r of additional_args: %r' %
                      (type(add_val), key))

def create_blueprint_for_targets(gn, targets, test_targets):
  """Generate a blueprint for a list of GN targets."""
  blueprint = Blueprint()

  # Default settings used by all modules.
  blueprint.add_module(create_cc_defaults_module())

  for target in targets:
    modules = create_modules_from_target(blueprint,
                                         gn,
                                         target,
                                         parent_gn_type=None,
                                         is_test_target=False)
    for module in modules:
      module.visibility.update(root_modules_visibility)

  for test_target in test_targets:
    modules = create_modules_from_target(blueprint,
                                         gn,
                                         test_target + gn_utils.TESTING_SUFFIX,
                                         parent_gn_type=None,
                                         is_test_target=True)
    for module in modules:
      module.visibility.update(root_modules_visibility)

  # Merge in additional hardcoded arguments.
  for module in blueprint.modules.values():
    # post_processing is applied here again after we have finished creating all the modules as
    # some modules shortcut the `create_modules_from_target` which means that the previous
    # post processing does not apply to them. Re-apply the post-processing here.
    # It's safe to reapply the post processing more than once as it appends to sets or
    # overwrite previous values.
    apply_post_processing(module)

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

  if module_copy.path:
    module_copy.path = _rebase_file(module_copy.path, blueprint_path)
    if module_copy.path is None:
      return None

  if module_copy.wrapper_src:
    module_copy.wrapper_src = _rebase_file(module_copy.wrapper_src,
                                           blueprint_path)
    if module_copy.wrapper_src is None:
      return None

  if module_copy.srcs:
    module_copy.srcs = _rebase_files(module_copy.srcs, blueprint_path)
    if module_copy.srcs is None:
      return None

  if module_copy.jars:
    module_copy.jars = _rebase_files(module_copy.jars, blueprint_path)
    if module_copy.jars is None:
      return None

  for (arch_name, _) in module_copy.target.items():
    module_copy.target[arch_name].srcs = (_rebase_files(
        module_copy.target[arch_name].srcs, blueprint_path))
    if module_copy.target[arch_name].srcs is None:
      return None

  return module_copy


def _path_to_name(path: str) -> str:
  path = path.replace("/", "_").lower()
  return f"{MODULE_PREFIX}{path}_license"


def _maybe_create_license_module(path: str) -> Union[Module, None]:
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

  license_module = Module("license", _path_to_name(path), "License-Artificial")
  license_module.visibility = {":__subpackages__"}
  # Assume that a LICENSE file always exist as we run the
  # create_android_metadata_license.py script each time we run GN2BP.
  license_module.license_text = {"LICENSE"}
  metadata = license_utils.parse_chromium_readme_file(
      str(readme_chromium_file),
      license_constants.POST_PROCESS_OPERATION.get(readme_relative_path,
                                                   lambda _metadata: _metadata))
  for license_name in metadata.get_licenses():
    license_module.license_kinds.add(
        license_utils.get_license_bp_name(license_name))
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
  if crate_root_dir:
    return crate_root_dir
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
      raise Exception(f"Found module {module_name} without a build file path.")

    rebased_module = _rebase_module(module, android_bp_path)
    if rebased_module:
      if android_bp_path not in blueprints.keys():
        blueprints[android_bp_path] = Blueprint(module.build_file_path)
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
  parser.add_argument('--repo_root',
                      required=True,
                      help='Path to the root of the repistory')
  parser.add_argument(
      '--build_script_output',
      help='JSON-formatted file containing output of build scripts broken down'
      + 'by architecture.',
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
      '--suffix',
      help='The suffix to the Android.bp filename. Pass "" if no suffix.',
      default='.gn2bp')
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

  initialize_globals(args.channel)
  targets = args.targets or gn2bp_targets.DEFAULT_TARGETS
  build_scripts_output = None
  with open(args.build_script_output) as f:
    build_scripts_output = json.load(f)
  gn = gn_utils.GnParser(builtin_deps, build_scripts_output)
  for desc_file in args.desc:
    with open(desc_file) as f:
      desc = json.load(f)
    for target in targets:
      gn.parse_gn_desc(desc, target)
    for test_target in gn2bp_targets.DEFAULT_TESTS:
      gn.parse_gn_desc(desc, test_target, is_test_target=True)
  top_level_blueprint = create_blueprint_for_targets(
      gn, targets, gn2bp_targets.DEFAULT_TESTS)

  final_blueprints = _break_down_blueprint(top_level_blueprint)
  if args.license:
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
""" % (Path(__file__).name)

  for (path, blueprint) in final_blueprints.items():
    # Copybara only includes the Android.bp files generated with .gn2bp suffix
    filename = "Android.bp" + args.suffix
    android_bp_file = Path(os.path.join(args.repo_root, path, filename))
    android_bp_file.write_text(
        "\n".join([header] + BLUEPRINTS_EXTRAS.get(path, []) +
                  blueprint.to_string()))

  return 0


if __name__ == '__main__':
  sys.exit(main())
