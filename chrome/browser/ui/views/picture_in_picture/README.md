# Picture-in-Picture Views

This directory contains Views code that is used for both
[video picture-in-picture] and [document picture-in-picture].

## Standalone Document PiP extension foundation

With `DocumentPipStandaloneWindow` enabled, `DocumentPipHost` owns the Widget
and child WebContents without creating a Browser or tab strip. In builds with
extension support, it also owns `DocumentPipWindowController`, whose
`DocumentPipBaseWindow` adapter exposes Widget operations to extension window
infrastructure.

Each opening gets a fresh window ID and a separate tab ID. The child receives
tab-contents classification, extension scripting/observation helpers, and a
`ZoomController` before its controller is published in `WindowControllerList`.
Its `SessionTabHelper` has no session-service delegate: the script-created
document must not enter session restore.

The controller describes a single active tab in an always-on-top `"popup"`
window. Tab metadata uses the normal extension permission-scrubbing rules.
Bounds and activation changes notify `WindowControllerList`.
Both host-initiated and native close paths unregister the controller before
destroying the Widget or child.

[video picture-in-picture]:
    /chrome/browser/ui/views/overlay/video_overlay/window_views
[document picture-in-picture]:
    /chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h
