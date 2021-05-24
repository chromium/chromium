# Paint Previews

_AKA Freeze Dried Tabs_

## What is a Paint Preview?

A paint preview is a collection of Skia Pictures representing the visual
contents of a webpage along with the links on the webpage stored in a protobuf.
The preview can be composited and played without a renderer process as a
low-fidelity and low-overhead alternative to a live page in various contexts.

## Architecture

There are three core components to the paint preview architecture;

* Capture - recording the content and links of a website as Skia Pictures +
  metadata.
* Compositing - converting Skia Pictures into bitmap tiles.
* Player - plays back contents using native UI.

### Capture

A paint preview is captured using a variation of the printing system in Blink.
The contents of the frame are captured as is. Images are not manipulated, but
fonts are subset to remove unused glyphs.

Capture supports both in-memory and file based approaches. While
performance-wise the in-memory approach should be used, on low-end devices the
memory overhead of capturing a webpage might be very expensive (100 MB+). To
circumvent this and avoid OOM scenarios, files may be used to store the Skia
Pictures.

To handle child frames (iframes), the renderer will request the browser
coordinate with the renderer of each child frame to capture it as an independent
Skia Picture. To stitch this together later, the parent frame inserts a
placeholder into its Skia Picture with the clip region of the child frame and an
ID mapping so that the child content can be inserted correctly during
compositing.

The child frame behavior applies to both same- and cross-process iframes so that
the content of each frame can be captured in its entirety. This allows them to
be scrollable for the purposes of playback. _Note: this only applies to
same-process iframes when they are scrollable._

As a side-effect of using the printing system there may be some visual
discrepencies;

* Viewport fixed elements are flattened into their current position in the
  content.
* JS driven dynamic elements will be frozen.
* Some GPU accelerated effect/animations may not be captured.
* Out-of-viewport content for which resource aren't loaded (e.g. images) may be
  missing.

### Compositing

To maintain the [Rule Of 2](https://chromium.googlesource.com/chromium/src/+/main/docs/security/rule-of-2.md)
compositing takes place in a sandboxed utility process. The Skia Pictures are
loaded into the compositor and from those pictures bitmaps can be generated. A
caller may elect to request the contents of the frame be turned into a single
combined Skia Picture or as a collection of independent Skia Pictures. The
split approach is desirable if scrollable child frames are used.

See `//components/services/paint_preview_compositor/` for more details.

### Playback

Per-architecture playback is possible to avoid the need for a renderer. Using
the bitmaps from the compoisitor in combination with links it is possible to
create a low-fidelity and lightweight representation of a webpage.

At present there is only a player available for Android. If something akin to a
screenshot of the whole webpage is desired it is possible to just use bitmaps
for it.

## Usage

Capture step is intended to be completed via
[PaintPreviewBaseService::CapturePaintPreview()](https://source.chromium.org/chromium/chromium/src/+/main:components/paint_preview/browser/paint_preview_base_service.h;bpv=1;bpt=1;l=127)
, although
[PaintPreviewClient](https://source.chromium.org/chromium/chromium/src/+/main:components/paint_preview/browser/paint_preview_client.h;bpv=1;bpt=1;l=36)
can be used directly if preferred.

Compositing should be started using [StartCompositorService()](https://source.chromium.org/chromium/chromium/src/+/main:components/paint_preview/browser/compositor_utils.h;bpv=1;bpt=1;l=16).
This should be followed by using the PaintPreviewCompositorService to create a
[PaintPreviewCompositorClient](https://source.chromium.org/chromium/chromium/src/+/main:components/paint_preview/public/paint_preview_compositor_client.h;bpv=1;bpt=1;l=24)
from which compositing can be controlled.

See the `player/` subdirectory for more details on playback.

## Codebase

Within this directory

* `browser/` - Code related to managing and requesting paint previews.
* `common/` - Shared code; mojo, protos, and C++ code.
* `features/` - Feature flags.
* `player/` - Code for playing back a preview. (Currently for Android).
* `public/` - Public interfaces for some implementations (refactoring WIP).
* `renderer/` - Code related to capturing paint previews within the renderer.

Outside of this directory there is some feature code in

* `chrome/android/java/src/org/chromium/chrome/browser/paint_preview/`
* `chrome/browser/paint_preview/`
* `components/services/paint_preview_compositor/`

## Why //components?

This directory facilitates sharing code between Blink and the browser process.
This has the additional benefit of keeping most of the code centralized to this
directory. Parts of the code are consumed in;

* `//cc`
* `//chrome`
* `//content`
* `//third_party/blink`

NOTE: This feature depends on working with `//content` and `//third_party/blink`
so it is incompatible with iOS.
