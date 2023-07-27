# Security Interstitials

This directory contains the implementation of security interstitials -- warning
pages that are shown instead of web content when certain security events occur
(such as an invalid certificate on an HTTPS connection, or a URL that is flagged
by Safe Browsing).

This is a layered component that includes a `core/` implementation (which is
also used by `//ios/components/security_interstitials` for the iOS
implementation), and a `content/` implementation for Blink platforms.

Security interstitials are split between an HTML+JS front end (which defines
the actual contents shown) and a C++ backing implementation.

`core/common/resources/` contains the shared HTML+JS used across the various
interstitial types.

`core/common/mojom/` contains the Mojo IPC definitions that are used for the
interstitial JS to communicate back to the C++ interstitial code to execute
various actions the user can take on the interstitial page.

`core/browser/resources` contain the HTML+JS implementations of the various
interstitial types (such as the SSL interstitial or Safe Browsing interstitial).

When adding a new interstitial type, you should also add it to
`core/browser/resources/list_of_interstitials.html` and
`chrome/browser/ui/webui/interstitials/interstitial_ui.cc` so that it is listed
in the interstitial testing page at `chrome://interstitials`.

`ControllerClient` is the C++ logic that handles commands sent by the
interstitial JS. The specific implementation is extended by the embedder -- see
`content/security_interstitial_controller_client.h` and
`//ios/components/security_interstitials/ios_blocking_page_controller_client.h`.

Many interstitials follow the pattern of implementing a core “UI” class (like
`SSLErrorUI` for SSL interstitials), which configures details for the
interstitial HTML, and connects the specific blocking page implementation with
the controller client implementation.

In `content/`, the central classes are:

*   `SecurityInterstitialControllerClient`, which handles commands from security
    interstitial pages. This is used by and extended for each interstitial type.
*   `SecurityInterstitialPage`, which handles the state of the interstitial page.
    This is extended for each interstitial type.
*   `SecurityInterstitialTabHelper`, which connects an interstitial page to a
    WebContents, and owns the underlying interstitial page.

`//ios/components/security_interstitials/` has parallel implementations, but for
iOS where we can’t use `content/`.

This directory is not an exhaustive container of all security interstitials.
Some interstitial types build on the core component classes but are implemented
outside of this directory (e.g., `chrome/browser/lookalikes/`).
