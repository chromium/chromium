# Paint Previews

_AKA Freeze Dried Tabs_

This is a WIP directory that contains code for generating and processing paint
previews of pages. A paint preview is a collection of Skia Pictures representing
the visual contents of a page along with the links on the page. The preview can
be composited and played without a renderer process as a low-fidelity and
low-cost alternative to a tab in various contexts.

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

## Directory Structure (WIP)

* `browser/` - Code related to managing and requesting paint previews.
* `common/` - Shared code; mojo, protos, and C++ code.
* `renderer/` - Code related to capturing paint previews within the renderer.
