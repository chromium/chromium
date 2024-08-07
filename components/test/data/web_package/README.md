# Generating signed web bundles for testing
## In general
- [wbn-sign CLI tools](https://github.com/WICG/webpackage/tree/main/js/sign#cli) provide functionalities for signing the web bundles and inferring the signed web bundle ID from the key.
- *(deprecated, used prior and provided only for reference)* [go CLI tools](https://github.com/WICG/webpackage/blob/main/go/bundle/README.md) were used before. They do not support signing the web bundles using v2 integrity block, so are no longer applicable.
## For most of the C++ tests
EcdsaP256 signature is nondeterministic, which makes it impossible to perform at least some tests that rely on byte-wise comparison. To remediate it, WebBundleSigner for tests uses hardcoded [nonce](https://source.chromium.org/chromium/chromium/src/+/main:components/web_package/test_support/signed_web_bundles/web_bundle_signer.cc?q=symbol%3A%5Cbweb_package%3A%3AkEcdsaP256SHA256NonceForTestingOnly%5Cb%20case%3Ayes). Hence, `.swbn` files in this directory that are used for such tests have to be generated using [this C++ code](/components/web_package/test_support/signed_web_bundles/web_bundle_signer.h).
