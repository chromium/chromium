Supervised User is a multiprocess [layered
component](https://sites.google.com/a/chromium.org/dev/developers/design-documents/layered-components-design) that supports cross-platform access to services and
features that target supervised users in Chrome, including metrics
collection, application of parental controls, and the enforcement of browser
protections.

It has the following structure:
- [`core/`](https://source.chromium.org/chromium/chromium/src/+/main:components/supervised_user/core): Code shared by `content/` and `ios/`.
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/supervised_user/core/browser): Browser process code.
  - [`common/`](https://source.chromium.org/chromium/chromium/src/+/main:components/supervised_user/core/common): Code shared by the browser and the renderer.
