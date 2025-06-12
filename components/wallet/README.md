# Wallet

Wallet is a [layered
component](https://www.chromium.org/developers/design-documents/layered-components-design).
It has the following structure:

- [`core/`](https://source.chromium.org/chromium/chromium/src/+/main:components/wallet/core): Code shared by `content/` and `ios/` in future.
  - [`common/`](https://source.chromium.org/chromium/chromium/src/+/main:components/wallet/core/common): Code shared by the browser and the renderer.
- [`content/`](https://source.chromium.org/chromium/chromium/src/+/main:components/wallet/content): Driver using the `//content` layer (all platforms except iOS).
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/wallet/content/browser): Browser process code.
