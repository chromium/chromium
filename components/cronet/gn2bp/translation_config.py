# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gn_utils

import build.gn_helpers

CRONET_LICENSE_NAME = "external_cronet_license"

EXTRAS_ANDROID_BP_FILE = "Android.extras.bp"

LIBCRYPTO_STRIPPED = "libcrypto"
LIBCRYPTO_UNSTRIPPED = "libcrypto_unstripped"


# pylint: disable=unused-argument
def always_disable(module, arch, context):
    return None


def enable_zlib(module, arch, context):
    # Requires crrev/c/4109079
    if arch == 'common':
        module.shared_libs.add('libz')
    else:
        module.target[arch].shared_libs.add('libz')


# pylint: enable=unused-argument


def enable_boringssl(module, arch, context):
    # Do not add boringssl targets to cc_genrules. This happens, because protobuf targets are
    # originally static_libraries, but later get converted to a cc_genrule.
    if module.is_genrule():
        return
    # Lets keep statically linking BoringSSL for testing target for now. This should be fixed.
    if module.name.endswith(gn_utils.TESTING_SUFFIX):
        return
    if arch == 'common':
        shared_libs = module.shared_libs
        static_libs = module.static_libs
        whole_static_libs = module.whole_static_libs
    else:
        shared_libs = module.target[arch].shared_libs
        static_libs = module.target[arch].static_libs
        whole_static_libs = module.target[arch].whole_static_libs
    shared_libs.add(f'{context.module_prefix}{LIBCRYPTO_UNSTRIPPED}')
    if module.type in ("cc_binary", "cc_library_shared", "rust_binary"):
        whole_static_libs.add(f'{context.module_prefix}ssl_and_pki')
    else:
        static_libs.add(f'{context.module_prefix}ssl_and_pki')


# pylint: disable=unused-argument
def add_androidx_experimental_java_deps(module, arch, context):
    module.libs.add("androidx.annotation_annotation-experimental")


def add_androidx_annotation_java_deps(module, arch, context):
    module.libs.add("androidx.annotation_annotation")


def add_androidx_core_java_deps(module, arch, context):
    module.libs.add("androidx.core_core")


def add_jsr305_java_deps(module, arch, context):
    module.static_libs.add("jsr305")


def add_errorprone_annotation_java_deps(module, arch, context):
    module.libs.add("error_prone_annotations")


def add_androidx_collection_java_deps(module, arch, context):
    module.libs.add("androidx.collection_collection")


def add_junit_java_deps(module, arch, context):
    module.static_libs.add("junit")


def add_truth_java_deps(module, arch, context):
    module.static_libs.add("truth")


def add_hamcrest_java_deps(module, arch, context):
    module.static_libs.add("hamcrest-library")
    module.static_libs.add("hamcrest")


def add_mockito_java_deps(module, arch, context):
    module.static_libs.add("mockito")


def add_guava_java_deps(module, arch, context):
    module.static_libs.add("guava")


def add_androidx_junit_java_deps(module, arch, context):
    module.static_libs.add("androidx.test.ext.junit")


def add_androidx_test_runner_java_deps(module, arch, context):
    module.static_libs.add("androidx.test.runner")


def add_androidx_test_rules_java_deps(module, arch, context):
    module.static_libs.add("androidx.test.rules")


def add_android_test_base_java_deps(module, arch, context):
    module.libs.add("android.test.base")


def add_accessibility_test_framework_java_deps(module, arch, context):
    # BaseActivityTestRule.java depends on this but BaseActivityTestRule.java is not used in aosp.
    pass


def add_espresso_java_deps(module, arch, context):
    module.static_libs.add("androidx.test.espresso.contrib")


def add_android_test_mock_java_deps(module, arch, context):
    module.libs.add("android.test.mock.stubs")


def add_androidx_multidex_java_deps(module, arch, context):
    # Androidx-multidex is disabled on unbundled branches.
    pass


def add_androidx_test_monitor_java_deps(module, arch, context):
    module.libs.add("androidx.test.monitor")


def add_androidx_ui_automator_java_deps(module, arch, context):
    module.static_libs.add("androidx.test.uiautomator_uiautomator")


def add_androidx_test_annotation_java_deps(module, arch, context):
    module.static_libs.add("androidx.test.rules")


def add_androidx_test_core_java_deps(module, arch, context):
    module.static_libs.add("androidx.test.core")


def add_androidx_activity_activity(module, arch, context):
    module.static_libs.add("androidx.activity_activity")


def add_androidx_fragment_fragment(module, arch, context):
    module.static_libs.add("androidx.fragment_fragment")


def add_rustversion_deps(module, arch, context):
    module.proc_macros.add("librustversion")


# pylint: enable=unused-argument

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
    # allow_all_warnings generates allow_all_warnings.rsp dynamically at build
    # time using Chromium's host rustc toolchain. In AOSP Soong, crates are
    # compiled with AOSP's own Rust compiler flags (and response files are
    # stripped by gn2bp), so this action and group are not needed.
    '//build/rust/gni_impl:allow_all_warnings':
    always_disable,
    '//build/rust/gni_impl:gen_allow_all_warnings_rsp':
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
    for suffix in gn_utils.POSSIBLE_SUFFIXES
}

# Same as _builtin_deps but will only apply what is explicitly specified.
builtin_deps.update({
    '//third_party/boringssl:boringssl_asm':
    # Due to FIPS requirements, downstream BoringSSL has a different "shape" than upstream's.
    # We're guaranteed that if X depends on :boringssl it will also depend on :boringssl_asm.
    # Hence, always drop :boringssl_asm and handle the translation entirely in :boringssl.
    always_disable,
})

replace_deps = {
    '//third_party/boringssl:boringssl': enable_boringssl,
}

BLUEPRINTS_EXTRAS = {"": ["build = [\"Android.extras.bp\"]"]}

BLUEPRINTS_MAPPING = {
    # BoringSSL's Android.bp is manually maintained and generated via a template,
    # see run_gen2bp.py's _gen_boringssl.
    "third_party/boringssl": "",
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++": "third_party/libc++",
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++abi": "third_party/libc++abi",
}

buildtools_protobuf_src = '//buildtools/protobuf/src'
android_protobuf_src = 'external/protobuf/src'

java_api_target_name = "//components/cronet/android:cronet_api_java"
package_default_visibility = ":__subpackages__"
root_modules_visibility = {
    "//packages/modules/Connectivity:__subpackages__",
    "//external/cronet:__subpackages__"
}
