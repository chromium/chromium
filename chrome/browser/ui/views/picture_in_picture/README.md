# Picture-in-Picture Views

This directory contains Views code that is used for both
[video picture-in-picture](/chrome/browser/ui/views/overlay/video_overlay/window_views) and
[document picture-in-picture](/chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h)

## Standalone Document PiP Widget adapter

`DocumentPipBaseWindow` adapts a PiP Widget to `ui::BaseWindow`
without owning the Widget or introducing a Browser. The adapter
must be destroyed before its Widget. The host unit tests cover
window operations directly; extension-controller wiring is separate.
