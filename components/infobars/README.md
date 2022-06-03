The infobars component contains the core types for infobars, a UI surface that
shows informative but generally nonblocking updates to users related to their
current page content.  This is used on both desktop and mobile, though the
presentation and available infobars both differ.  On desktop, for example,
infobars are a thin bar atop the page, while on Android an "infobar" is a
larger, popup-like surface at screen bottom.

Infobars are a problematic UI design for various reasons (spoofability,
dynamically modifying content area size, not being visually anchored and scoped
well), and are occasionally used for purposes outside their original intent
(e.g. the "default browser" infobar, which does not relate to the page content).
Be cautious about adding new ones.

Infobars is a layered component
(https://sites.google.com/a/chromium.org/dev/developers/design-documents/layered-components-design)
to enable it to be shared cleanly on iOS.

Directory structure:
android/: Android-specific specializations
core/: Shared code that does not depend on src/content/
content/: Driver for the shared code based on the content layer
