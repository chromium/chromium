# Signed Web Bundles (`components/web_package/signed_web_bundles`)

This directory contains code related to _Signed Web Bundles_ (`.swbn`). Signed
Web Bundles are an extension of normal, unsigned Web Bundles. Signed Web Bundles
are encoded as a [CBOR Sequence](https://www.rfc-editor.org/rfc/rfc8742.html)
consisting of an _Integrity Block_ followed by a _Web Bundle_.

In contrast to individually signed responses and Signed Exchanges, signatures of
Signed Web Bundles provide a guarantee that the entire Web Bundle was not
modified, including that no responses have been added or removed.

## Companion Documentation

- **AI Agent Rules & Invariants:** [AGENTS.md](AGENTS.md)
- **Parent Component README:** [`//components/web_package/README.md`](../README.md)
- **Core IWA Component:**
  [`//components/webapps/isolated_web_apps`](/components/webapps/isolated_web_apps/README.md)

## Integrity Block v2 & Multi-Signature Support

The format of the Integrity Block is described in the
[Integrity Signature Explainer](https://github.com/WICG/webpackage/blob/main/explainers/integrity-signature.md).
Integrity Block v2 contains magic bytes, version `2b\0\0`, an `attributes` map
(containing the `webBundleId`), and a _signature stack_ with one or more
signatures and their corresponding public keys (Ed25519 and ECDSA P-256 SHA-256).

`SignedWebBundleSignatureVerifier` iterates through all recognized entries in
the signature stack, verifies the SHA-512 payload hash and cryptographic
signatures, and ignores unrecognized signature types
(`SignedWebBundleSignatureInfoUnknown`) for forward compatibility.

## Parsing & Rule of 2

Parsing Signed Web Bundles is a three-step process:

1. Parse the Integrity Block using `WebBundleParser::ParseIntegrityBlock`, which
   delegates CBOR decoding to the memory-safe Rust crate in `rust/`
   (`signed_web_bundles_rust`).
2. Verify that the signatures and `webBundleId` attribute match using
   `SignedWebBundleSignatureVerifier` and `IdentityValidator`.
3. Parse the metadata using `WebBundleParser::ParseMetadata` while providing the
   length of the Integrity Block as the `offset` parameter.

`WebBundleParser` can be used directly from non-sandboxed code such as the
browser process: the CBOR, Integrity Block and Web Bundle parsing it delegates
to is implemented in `#![forbid(unsafe_code)]` Rust, so the
[Rule of 2](/docs/security/rule-of-2.md) is satisfied without a sandboxed
utility process.

## Web Bundle ID (`SignedWebBundleId`)

Signed Web Bundles are identified by a validated `SignedWebBundleId` (lowercase
base32-encoded string with a 3-byte type suffix), supporting three ID types:

- **Ed25519 Public Key** (`SignedWebBundleId::Type::kEd25519PublicKey`)
- **ECDSA P-256 Public Key** (`SignedWebBundleId::Type::kEcdsaP256PublicKey`)
- **Development Proxy Mode** (`SignedWebBundleId::Type::kProxyMode`)

More information about Signed Web Bundle IDs can be found in the
[Isolated Web Apps Scheme Explainer](https://github.com/WICG/isolated-web-apps/blob/main/Scheme.md#signed-web-bundle-ids).
