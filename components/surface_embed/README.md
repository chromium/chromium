# Surface Embed

`//components/surface_embed` implements a mechanism to embed
`content::WebContents` inside another `WebContents` using a `blink::WebPlugin`.

## Overview

SurfaceEmbed uses a `blink::WebPlugin` to embed `WebContents`. The plugin
provides the full graphical and interactive web browsing experience, and
intentionally excludes the scripting and other communication channels provided
by embedding elements such as `<iframe>` and `<webview>`.

The primary goal is to provide a secure and simple way to embed web content
surfaces, minimizing the complexity and security risks associated with
full-featured iframe-based solutions.

This is a solution to host tab contents inside of the WebUI-Browser WebUI,
which lives in `//chrome/browser/ui/webui_browser`. WebUI Browser uses
SurfaceEmbed when `kSurfaceEmbed` is enabled, which is the default, and falls
back to [GuestContents](../guest_contents/README.md) when the feature is
disabled.

## Architecture

### Components

1.  **Renderer (Embedder):**
    *   `SurfaceEmbedWebPlugin` (in `//components/surface_embed/renderer`)
implements `blink::WebPlugin`.
    *   It manages the surface layer attachment and detachment, handles focus in
and out of the `<embed>` element, and communicates with the browser.
    *   It implements `surface_embed::mojom::SurfaceEmbed`, the renderer side
SurfaceEmbed mojo API.

2.  **Browser:**
    *   `SurfaceEmbedHost` (in `//components/surface_embed/browser`) acts as the
host for the plugin. It implements `surface_embed::mojom::SurfaceEmbedHost`, the
browser side SurfaceEmbed mojo API.
    *   `SurfaceEmbedConnector` (interface in `//content/public/browser`,
implementation in `//content/browser/surface_embed`) connects the inner
`WebContents`'s
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
| outer WebContents |<----------------|--------| SurfaceEmbedConnector |------⬥| inner WebContents|
+---------+---------+                 |        +-----------^-----------+       +--------+---------+
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

See [USER_GUIDE.md](./USER_GUIDE.md) for the required browser, renderer, and
Content Security Policy setup. After completing that setup, include an
`<embed>` tag in the embedder page:

```html
<embed type="application/x-chromium-surface-embed"
data-content-id="[content-id]">
```

The `data-content-id` corresponds to a `surface_embed::SurfaceEmbedHandle` ID,
which identifies the `WebContents` to be embedded.
