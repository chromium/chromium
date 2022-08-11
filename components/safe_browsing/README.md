//components/safe_browsing is a layered component
(https://sites.google.com/a/chromium.org/dev/developers/design-documents/layered-components-design)
to enable it to be shared cleanly on iOS.

The component has the following structure:

* core/: shared code that does not depend on either {src/content, src/ios} or {./content,
./ios}
    * browser/: Browser process code
    * common/: Code shared by the browser and the renderer
* content/: non-iOS code layered above core/ that integrates with the Content API
    *  browser/: Browser process code
    *  common/: Code shared by the browser and the renderer
    *  renderer/: Renderer process code
* ios/: iOS-specific code layered above core/ that integrates with //ios/web

A note for //content-based embedders of this component:

//content-based embedders should use the SafeBrowsingBlockingPage,
SafeBrowsingUIManager, and SafeBrowsingNavigationThrottle classes.
BaseBlockingPage and BaseUIManager exist for Android WebView and should not be
used further (crbug.com/1234027).
