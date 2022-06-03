# Paint Preview Player

The player displays a paint preview that has been previously recorded.
Currently, the player is only fully implemented for Android. However, there
are a few platform-independent base classes ([`PlayerCompositorDelegate`](https://source.chromium.org/chromium/chromium/src/+/main:/components/paint_preview/player/player_compositor_delegate.cc)
, [`CompositorStatus`](https://source.chromium.org/chromium/chromium/src/+/main:/components/paint_preview/player/compositor_status.h)
, [`BitmapRequest `](https://source.chromium.org/chromium/chromium/src/+/main:/components/paint_preview/player/bitmap_request.cc)
) than can be used to extend the playback support for other platforms.

`PlayerCompositorDelegate` uses the [StartCompositorService](https://source.chromium.org/chromium/chromium/src/+/main:components/paint_preview/browser/compositor_utils.h;bpv=1;bpt=1;l=16?q=StartCompositorService&ss=chromium&gsn=StartCompositorService&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Dc%252B%252B%3Fpath%3Dsrc%2Fcomponents%2Fpaint_preview%2Fbrowser%2Fcompositor_utils.h%23E_S97S1y6_-ukZj8vR6XZUzOTYFPwOkwR0LybvqxVWg)
API. Alternatively, playback support for other platforms can be provided by
using `StartCompositorService` directly for more flexibility.

The remainder of this doc describes the Android-specific implementation.

## TL;DR

Want to use the player? Construct a [`PlayerManager`](https://source.chromium.org/chromium/chromium/src/+/main:/components/paint_preview/player/android/java/src/org/chromium/components/paintpreview/player/PlayerManager.java)
and use [`PlayerManager#getView`](https://source.chromium.org/chromium/chromium/src/+/main:components/paint_preview/player/android/java/src/org/chromium/components/paintpreview/player/PlayerManager.java;drc=c62b2799aeef847ccb9fbd5d1fc3e19e2938df9a;l=240)
to display it.

## Design

As mentioned in the main [`README`](https://source.chromium.org/chromium/chromium/src/+/main:/components/paint_preview/README.md)
, a paint preview can have multiple frames in a nested structure.
Consequently, the player is desinged in a nested structure as well, to
facilitate the display of mulitple nested frames.


* `android/java/src/.../player/`: This directory contains per-player classes.
In another word, these classes are aware that the player might have multiple
frames and are not involved in the logic for displaying a single frame.
* `android/java/src/.../player/frame`: This directory contains per-frame
classes. These are responsible for displaying a single frame.

### Important classes

* [`PlayerManager`](https://source.chromium.org/chromium/chromium/src/+/main:/components/paint_preview/player/android/java/src/org/chromium/components/paintpreview/player/PlayerManager.java):
Entry point for using the player. When created it initializes the compositor
and populates a hierarchy of player frames based on the paint preview.
* [`PlayerCompositorDelegateImpl`](https://source.chromium.org/chromium/chromium/src/+/main:/components/paint_preview/player/android/java/src/org/chromium/components/paintpreview/player/PlayerCompositorDelegateImpl.java)
: Communicates with the paint preview compositor. It requests bitmaps from the
comopsitor and provides them to palyer frames.
* [`PlayerFrameMediator`](https://source.chromium.org/chromium/chromium/src/+/main:/components/paint_preview/player/android/java/src/org/chromium/components/paintpreview/player/frame/PlayerFrameMediator.java)
: Handles the business logic for a single frame in the player, i.e.
maintaining a viewport, updating its sub-frame visibilities, requesting
bitmaps from the compositor delegate, etc.
* [`PlayerFrameView`](https://source.chromium.org/chromium/chromium/src/+/main:/components/paint_preview/player/android/java/src/org/chromium/components/paintpreview/player/frame/PlayerFrameView.java)
: The View that displays a single frame.