# Surface Embed

`//components/surface_embed` implements a mechanism to embed
`content::WebContents` inside another `WebContents` using a `blink::WebPlugin`.

## Overview

SurfaceEmbed uses a `blink::WebPlugin` to embed `WebContents`. The plugin
provides the full graphical and interactive web browsing experience, and
intentionally excludes the scripting and other communication channels provided
by embedding elements such as `<iframe>`, `<fencedframe>`, and `<webview>`.

The primary goal is to provide a secure and simple way to embed web content
surfaces, minimizing the complexity and security risks associated with
full-featured iframe-based solutions.

This is the solution to host tab contents inside of the WebUI-Browser WebUI,
which lives in `//chrome/browser/ui/webui_browser` and currently uses
[GuestContents](https://source.chromium.org/chromium/chromium/src/+/main:components/guest_contents/README.md).

## Architecture

### Components

1.  **Renderer (Embedder):**
    *   `SurfaceEmbedWebPlugin` (in `//components/surface_embed/renderer`)
implements `blink::WebPlugin`.
    *   It manages the surface layer attachment and detachment, handles focus in
and out of the `<embed>` element , and communicates with the browser.
    *   It implements `surface_embed::mojom::SurfaceEmbed`, the renderer side
SurfaceEmbed mojo API.

2.  **Browser:**
    *   `SurfaceEmbedHost` (in `//components/surface_embed/browser`) acts as the
host for the plugin. It implements `surface_embed::mojom::SurfaceEmbedHost`, the
browser side SurfaceEmbed mojo API.
    *   `SurfaceEmbedConnector` (interface in `//content/public/browser`, impl
in `//content/browser`) connects the inner `WebContents`'s
`RenderWidgetHostView` to the embedder.

### Architecture Diagram

```
                                           //components/surface_embed/browser
                                                  +------------------+
                                      +---------->| SurfaceEmbedHost |
                                      |           +--------+---------+
                                      |                    |
                                      |                    |
//content/browser                     |                    v
+-------------------+                 |        +-----------------------+       +------------------+
| outer WebContents |<----------------|--------| SurfaceEmbedConnector |<>-----| inner WebContents|
+---------+---------+                 |        +-----------^-----------+  owns +--------+---------+
          |                           |                    |                            |
          v                           |                    |                            v
+-------------------+                 |                    |                   +------------------+
|   RWHVAura / Mac  |                 |                    +-------------------|  RWHVChildFrame  |
+---------+---------+                 |                                        +--------+---------+
          |           Browser Process |                                                 |
======================================|=================================================|==========
          |          Renderer Process |                                                 |
          v                           |                                                 v
//content/renderer                    |                                        +------------------+
+-------------------+                 |                                        |    RenderFrame   |
|    RenderFrame    |                 |                                        +------------------+
+---------+---------+                 |
          |                           |
          v                           |
+-------------------+                 |
|      <embed>      |                 |
+---------+---------+                 |
          | owns                      |
          v                           |
//components/surface_embed/renderer   |
+-------------------+                 |
|   SurfaceEmbed    |                 |
|     WebPlugin     |-----------------+
+-------------------+
```

## Usage

To use SurfaceEmbed, an embedder page includes an `<embed>` tag:

```html
<embed type="application/x-chromium-surface-embed"
data-content-id="[content-id]">
```

The `data-content-id` corresponds to a `guest_contents::GuestContentsHandle` ID,
which identifies the `WebContents` to be embedded.
