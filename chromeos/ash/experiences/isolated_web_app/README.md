# ChromeOS Mojo Services and Allowlisting for IWAs

This directory hosts the Mojo services for features restricted to
allowlisted Isolated Web Apps (IWAs) on ChromeOS.

Currently, it implements the `window.setShape` API.

* The Blink frontend lives in
  `//third_party/blink/renderer/modules/set_shape/`.
* The browser tests verifying this API and its allowlist
  behavior live in `//chrome/browser/ash/set_shape/`.
