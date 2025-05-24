# README

This folder contains code to support W3C Encrypted Media Extensions (EME),
key systems and Content Decryption Modules (CDM). It's under `//components`
folder so they can be shared by multiple embedders of content, e.g. `//chrome`
and `//android_webview`. See
[`//components` use cases](https://source.chromium.org/chromium/chromium/src/+/main:components/README.md#use-cases)
for more context.

This means that only content embedders can depend on targets in this folder,
including `//chrome`, `//chromecast`, `//android_webview`, `//fuchsia_web`, and
`//content/shell` (test content embedder). Other folders, especially `//media`
and `//content`, should NOT depend on targets in this folder.

In general, key system specific logic should not be in `//media` or `//content`.
So it's also a good practice to put them under `//components/cdm`.
