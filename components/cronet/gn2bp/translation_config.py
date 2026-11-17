# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gn_utils

CRONET_LICENSE_NAME = "external_cronet_license"

LIBCRYPTO_STRIPPED = "libcrypto"
LIBCRYPTO_UNSTRIPPED = "libcrypto_unstripped"


def always_disable(module, arch, context):
    return None


def _add_libs(*libs):
    def _apply(module, arch, context):
        module.libs.update(libs)

    return _apply


def _add_static_libs(*libs):
    def _apply(module, arch, context):
        module.static_libs.update(libs)

    return _apply


def enable_zlib(module, arch, context):
    # Requires crrev/c/4109079
    if arch == 'common':
        module.shared_libs.add('libz')
    else:
        module.target[arch].shared_libs.add('libz')


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


_builtin_deps = {
    '//buildtools/third_party/libunwind:libunwind': always_disable,
    # rustc_print_cfg is used to print rustc compiler default assumption
    # for a specific CPU architecture (e.g. target_feature="ssse3"). Those
    # features only changes from one CPU architecture to another. It's used
    # when generating the cxxbindings and build scripts (build scripts is a
    # rust concept, it's an rust binary that generates flags used to compile
    # other rust binaries).
    # For CXXBindings, create_rust_cxx_modules in translators.py invokes
    # cxxbridge directly and passes the required --cfg flags explicitly.
    # For build scripts, we generate those on Chromium side in a JSON file and
    # inject them in the pipeline.
    #
    # From the above reasoning, we can safely assume that we should not need
    # to build this target at all.
    '//build/rust/gni_impl:rustc_print_cfg': always_disable,
    # allow_all_warnings generates allow_all_warnings.rsp dynamically at build
    # time using Chromium's host rustc toolchain. In AOSP Soong, crates are
    # compiled with AOSP's own Rust compiler flags (and response files are
    # stripped by gn2bp), so this group is not needed.
    '//build/rust/gni_impl:allow_all_warnings': always_disable,
    '//third_party/zstd:headers': always_disable,
    '//testing/buildbot/filters:net_unittests_filters': always_disable,
    '//third_party/boringssl/src/third_party/fiat:fiat_license': always_disable,
    '//third_party/zlib:zlib': enable_zlib,
    '//third_party/androidx:androidx_annotation_annotation_java': _add_libs(
        "androidx.annotation_annotation"
    ),
    '//third_party/androidx:androidx_annotation_annotation_experimental_java': _add_libs(
        "androidx.annotation_annotation-experimental"
    ),
    '//third_party/androidx:androidx_core_core_java': _add_libs(
        "androidx.core_core"
    ),
    '//third_party/android_deps:com_google_code_findbugs_jsr305_java': _add_static_libs(
        "jsr305"
    ),
    '//third_party/android_deps:com_google_errorprone_error_prone_annotations_java': _add_libs(
        "error_prone_annotations"
    ),
    '//third_party/junit:junit': _add_static_libs("junit"),
    '//third_party/google-truth:google_truth_java': _add_static_libs("truth"),
    '//third_party/hamcrest:hamcrest_core_java': _add_static_libs(
        "hamcrest-library", "hamcrest"
    ),
    '//third_party/mockito:mockito_java': _add_static_libs("mockito"),
    '//third_party/android_deps:guava_android_java': _add_static_libs("guava"),
    '//third_party/androidx:androidx_test_ext_junit_java': _add_static_libs(
        "androidx.test.ext.junit"
    ),
    '//third_party/androidx:androidx_test_runner_java': _add_static_libs(
        "androidx.test.runner"
    ),
    # BaseActivityTestRule.java depends on this, and BaseActivityTestRule.java
    # is not used in AOSP.
    '//third_party/android_deps:com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework_java': always_disable,
    '//third_party/android_deps:espresso_java': _add_static_libs(
        "androidx.test.espresso.contrib"
    ),
    '//third_party/android_sdk:android_test_mock_java': _add_libs(
        "android.test.mock.stubs"
    ),
    # Androidx-multidex is disabled on unbundled branches.
    '//third_party/androidx:androidx_multidex_multidex_java': always_disable,
    '//third_party/androidx:androidx_test_monitor_java': _add_libs(
        "androidx.test.monitor"
    ),
    '//third_party/androidx:androidx_test_core_java': _add_static_libs(
        "androidx.test.core"
    ),
    '//third_party/androidx:androidx_test_uiautomator_uiautomator_java': _add_static_libs(
        "androidx.test.uiautomator_uiautomator"
    ),
    '//third_party/hamcrest:hamcrest_java': _add_static_libs(
        "hamcrest-library", "hamcrest"
    ),
    '//third_party/androidx:androidx_test_rules_java': _add_static_libs(
        "androidx.test.rules"
    ),
    # Crubit GN helper targets for the Rust side of the bindings and the Rust
    # stdlib bindings. In Soong, `crubit_cpp_from_rust` natively provides
    # `libcrubit_bridge_rust` and the prebuilt toolchain's stdlib Crubit
    # bindings (`libcore_crubit`, `liballoc_crubit`, `libstd_crubit`), so
    # Chromium's GN targets for these are not needed.
    #
    # Note: the C++ side (`//build/rust/crubit:crubit` and
    # `//build/rust/crubit:cpp_api_from_rust_bindings_cpp_deps`) is *not*
    # disabled: gn_utils.py translates it into Cronet's own build of Crubit's
    # C++ support library, see `gn_utils.CRUBIT_CPP_SUPPORT_TARGET`.
    '//build/rust/crubit:cpp_api_from_rust_bindings_rs_deps': always_disable,
    '//build/rust/std:std_bindings': always_disable,
    # Chromium-internal GTest unittest Java support libraries pulled in by
    # `test()`/`cronet_test()` APK wrapper targets (`__apk__java`). AOSP runs
    # GTests via `//testing/android/native_test:native_test_java`, so these
    # APK-wrapper-only Java targets are not used in Soong.
    '//base/test:test_support_java': always_disable,
    '//base:base_unittest_support_java': always_disable,
}

builtin_deps = {
    "{}{}".format(key, suffix): value
    for key, value in _builtin_deps.items()
    for suffix in gn_utils.POSSIBLE_SUFFIXES
}

# Same as _builtin_deps but will only apply what is explicitly specified.
builtin_deps.update(
    {
        '//third_party/boringssl:boringssl_asm':
        # Due to FIPS requirements, downstream BoringSSL has a different "shape" than upstream's.
        # We're guaranteed that if X depends on :boringssl it will also depend on :boringssl_asm.
        # Hence, always drop :boringssl_asm and handle the translation entirely in :boringssl.
        always_disable,
    }
)

replace_deps = {
    '//third_party/boringssl:boringssl': enable_boringssl,
}

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
root_modules_visibility = {
    "//packages/modules/Connectivity:__subpackages__",
    "//external/cronet:__subpackages__",
}
