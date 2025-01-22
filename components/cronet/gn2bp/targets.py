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
    "//build/rust/tests/test_serde_json_lenient:test_serde_json_lenient__library",  # Added to make sure that rust still compiles
    "//build/rust/tests/bindgen_test:bindgen_test",  # Added to make sure that rust still compiles
    '//build/rust/tests/bindgen_static_fns_test:bindgen_static_fns_test',  # Added to make sure that rust still compiles
    "//net/android:dummy_spnego_authenticator_java",  # Required for net::HttpAuthHandlerNegotiateTest::SetUp() to work, due to https://crbug.com/389069158
]

# Usually, README.chromium lives next to the BUILD.gn. However, some cases are
# different, this dictionary allows setting a specific README.chromium path
# for a specific BUILD.gn
README_MAPPING = {
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++": "third_party/libc++",
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++abi": "third_party/libc++abi",
}
