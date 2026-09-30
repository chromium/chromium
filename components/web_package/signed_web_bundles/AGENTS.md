# Signed Web Bundles (`components/web_package/signed_web_bundles`)

**Parent:** [Web Packaging Guidelines](/components/web_package/AGENTS.md) · [WebApps Guidelines](/components/webapps/AGENTS.md)

Cryptographic identity, Integrity Block v2 parsing, and multi-signature
verification (Ed25519 and ECDSA P-256 SHA-256) for Signed Web Bundles (`.swbn`).

## Canonical Docs

- [Signed Web Bundles Overview](README.md)
- [Web Packaging Component README](/components/web_package/README.md)
- [Core IWA Component Guidelines](/components/webapps/isolated_web_apps/AGENTS.md)

## Security & Domain Invariants

- **Unforgeable `SignedWebBundleId`:** `SignedWebBundleId` is a validated strong
  identifier (base32-encoded lower-case representation with a 3-byte type
  suffix) supporting three types: development proxy mode, Ed25519 public key,
  and ECDSA P-256 public key. Always construct via `SignedWebBundleId::Create()`
  or the `CreateFor*()` factory methods.
- **Integrity Block v2 & Multi-Signature Verification:**
  `SignedWebBundleSignatureVerifier` validates the `webBundleId` attribute
  against the signature stack via `IdentityValidator` and verifies all
  recognized entries in the signature stack, ignoring
  `SignedWebBundleSignatureInfoUnknown` entries for forward compatibility.
- **Rule of 2:** Raw CBOR integrity block parsing and low-level cryptographic
  verification are implemented in the memory-safe Rust crate in `rust/`
  (`signed_web_bundles_rust`), with untrusted parsing executed inside
  `data_decoder`.

## Command Line Execution

- **Unit Tests:**
  `tools/autotest.py -C out/Default components/web_package/signed_web_bundles/`
- **Header & Visibility Check:**
  `gn check out/Default "//components/web_package/*"`
