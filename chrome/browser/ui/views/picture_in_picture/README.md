# Picture-in-Picture Views

This directory contains Views code that is used for both
[video picture-in-picture](/chrome/browser/ui/views/overlay/video_overlay/window_views) and
[document picture-in-picture](/chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h)

## Standalone Document PiP Widget adapter

`DocumentPipBaseWindow` adapts a PiP Widget to `ui::BaseWindow`
without owning the Widget or introducing a Browser. The adapter
must be destroyed before its Widget. The host unit tests cover
window operations directly; extension-controller wiring is separate.

## Standalone Document PiP child identity

In extension-enabled builds, each opening gets a fresh window ID
and a separate tab ID. The child receives tab-contents classification,
extension scripting/observation helpers, and a ZoomController.
Its SessionTabHelper has no session-service delegate: the script-created
document must not enter session restore. WindowControllerList publication
is a separate integration step.
