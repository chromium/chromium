# Signed Web Bundles

This directory contains code related to _Signed Web Bundles_. Signed Web Bundles
are an extension of normal, unsigned Web Bundles. Signed Web Bundles are encoded
as a [CBOR Sequence](https://www.rfc-editor.org/rfc/rfc8742.html) consisting of
an _Integrity Block_ followed by a _Web Bundle_.

In contrast to individually signed responses and Signed Exchanges, signatures of
Signed Web Bundles provide a guarantee that the entire Web Bundle was not
modified, including that no responses have been added or removed.

## Integrity Block

The format of the Integrity Block is described in [this
explainer](https://github.com/WICG/webpackage/blob/main/explainers/integrity-signature.md).
It contains magic bytes and version, similar to unsigned Web Bundles, as well as
a _signature stack_. The signature stack contains one or more signatures and
their corresponding public keys.

**Note: Support for more than one signature is not yet fully designed and
implemented (crbug.com/1366303).**

## Parsing

Parsing Signed Web Bundles is a three step process:

1. Parse the Integrity Block using `WebBundleParser::ParseIntegrityBlock`.
2. Verify that the signatures match using `SignedWebBundleSignatureVerifier`.
3. Parse the metadata using `WebBundleParser::ParseMetadata` while providing the
   length of the Integrity Block as the `offset` parameter.

Due to the [rule of 2](../../../docs/security/rule-of-2.md), you may need to use
`data_decoder::SafeWebBundleParser` instead of using `WebBundleParser` directly
if your code runs in a non-sandboxed process.

## Web Bundle ID

Signed Web Bundles can be identified by a Web Bundle ID (see
`SignedWebBundleId`), which is derived from the public key of its first
signature. More information about the Web Bundle ID can be found in [this
explainer](https://github.com/WICG/isolated-web-apps/blob/main/Scheme.md#signed-web-bundle-ids).
