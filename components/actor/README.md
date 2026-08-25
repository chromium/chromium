# Actor Component

This component contains common types for the Chrome Actor API
(//chrome/browser/actor) for use outside of //chrome.

This is a [layered component](https://www.chromium.org/developers/design-documents/layered-components-design/)
to allow it to be shared on iOS.

## Directory Structure

- `core/`: Core cross-platform code shared by all platforms, including iOS.
  Contains common types, flags, and base interfaces.
- `public/`: Public interfaces and Mojo definitions (`mojom/`). Shared across
  all platforms, including iOS.
- `renderer/`: Code running within the renderer processes. Used only on
  platforms that use the Blink rendering engine.
