# Web Packaging Component (`components/web_package`)

This directory contains shared code for parsing, validating, and building
[Web Bundles](https://github.com/WICG/webpackage) (`.wbn`) and
[Signed Web Bundles](signed_web_bundles/README.md) (`.swbn`), used from the
sandboxed `data_decoder` utility process, the Blink renderer process, and the
Isolated Web Apps (IWA) platform.

## Companion Documentation

- **AI Agent Rules & Invariants:** [AGENTS.md](AGENTS.md)
- **Signed Web Bundles Rules & README:**
  [`signed_web_bundles/AGENTS.md`](signed_web_bundles/AGENTS.md) ·
  [`signed_web_bundles/README.md`](signed_web_bundles/README.md)
- **Core IWA Component:**
  [`//components/webapps/isolated_web_apps`](/components/webapps/isolated_web_apps/README.md)

## Architecture & Subdirectory Map

- **Core Web Bundle Parser (`web_bundle_parser.{h,cc}`, `web_bundle_parser_factory.{h,cc}`):**
  Implements `web_package::mojom::WebBundleParser`, reading Web Bundle metadata
  and HTTP responses from a `BundleDataSource` and delegating low-level CBOR
  parsing to the memory-safe Rust library in [`rust/`](rust/README.md).
- **Web Bundle Builder (`web_bundle_builder.{h,cc}`):**
  Constructs valid CBOR Web Bundles in memory for tests and tooling.
- **[`signed_web_bundles/`](signed_web_bundles/README.md):**
  Implements Integrity Block v2 parsing, `SignedWebBundleId` validation, and
  multi-signature verification (`SignedWebBundleSignatureVerifier`) for Ed25519
  and ECDSA P-256 SHA-256 keys.
- **[`rust/`](rust/README.md):**
  Memory-safe Rust crate (`web_package_rust`) implementing low-level Web Bundle
  format parsing exposed to C++ via `cpp_api_from_rust` bindings.
- **`mojom/`:**
  Mojo interface definitions (`web_bundle_parser.mojom`) and Mojom traits for
  communicating with the sandboxed `data_decoder` utility service.
- **`test_support/`:**
  Bundle signing utilities (`WebBundleSigner`), key-pair wrappers
  (`Ed25519KeyPair`, `EcdsaP256KeyPair`), and mock parser factories
  (`MockWebBundleParserFactory`).
