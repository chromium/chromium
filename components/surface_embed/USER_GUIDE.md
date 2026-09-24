# Surface Embed User Guide

`//components/surface_embed` implements a mechanism to embed a
`content::WebContents` (the inner/embedded page) inside another
`content::WebContents` (the outer/embedder page) by using a custom
`blink::WebPlugin`. It is utilized by **WebUI Browser** to embed tab contents
directly.

---

## 1. Setup in C++

### 1.1. Allow Your Frame to Use Surface-Embed
Currently, surface-embed is only enabled for `chrome://webui-browser`. If you
wish to allow another frame or WebUI domain to use surface-embed, you must:

1. Register the binder for `SurfaceEmbedHost` in
   [chrome_content_browser_client_receiver_bindings.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/chrome_content_browser_client_receiver_bindings.cc;l=426;drc=8600f0c44d101f10c0989b1d618205475f4b6aba):
   ```cpp
   #include "components/surface_embed/common/features.h"
   #include "components/surface_embed/browser/surface_embed_host.h"

   // ...
   if (base::FeatureList::IsEnabled(surface_embed::features::kSurfaceEmbed)) {
     associated_registry.AddInterface<surface_embed::mojom::SurfaceEmbedHost>(
         base::BindRepeating(
             [](content::RenderFrameHost* render_frame_host,
                mojo::PendingAssociatedReceiver<
                    surface_embed::mojom::SurfaceEmbedHost> receiver) {
               auto* web_ui = render_frame_host->GetWebUI();
               // Security check: only allow surface-embed in WebUIBrowserUI.
               // Add your WebUIController here.
               if (!web_ui ||
                   !web_ui->GetController()->GetAs<WebUIBrowserUI>()) {
                 return;
               }
               surface_embed::SurfaceEmbedHost::Create(render_frame_host,
                                                       std::move(receiver));
             },
             &render_frame_host));
   }
   ```
2. Update [ChromeContentRendererClient::OverrideCreatePlugin](https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/chrome_content_renderer_client.cc;l=971;drc=6f467ee465ea6f7403464161c86e42afcb15e0ee)
   to recognize your WebUI URL and allow the creation of the plugin:
   ```cpp
   if (url.SchemeIs(content::kChromeUIScheme) &&
       url.host() == chrome::kChromeUIYourCustomHost) {
     if (surface_embed::MaybeCreatePlugin(render_frame, params, plugin)) {
       return true;
     }
   }
   ```

### 1.2. Configure Content Security Policy (CSP)
By default, WebUI blocks `<object>`s and `<embed>`s. You must explicitly
override the CSP `object-src` rule to allow `'self'` via [WebUIDataSource](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_ui_data_source.h;l=147;drc=d673b3a7277668f3e619493d8f91deca6616b07a).
```cpp
source->OverrideContentSecurityPolicy(
    network::mojom::CSPDirectiveName::ObjectSrc, "object-src 'self';");
```

### 1.3. How to Get the Content ID for a WebContents
To embed a child `content::WebContents`, the frontend needs a unique identifier
corresponding to that embedded `WebContents`. This is managed by
`SurfaceEmbedHandle`.

On the C++ side, associate a handle with the embedded `WebContents` and retrieve
its assigned unique ID token (`base::UnguessableToken`):
```cpp
#include "components/surface_embed/browser/surface_embed_handle.h"

// 1. Create or retrieve the handle for the target WebContents
surface_embed::SurfaceEmbedHandle* embedded_handle =
    surface_embed::SurfaceEmbedHandle::CreateForWebContents(web_contents);

// 2. Get the unique ID token and pass its serialized string to the frontend
// (e.g. via Mojo message)
std::string content_id = embedded_handle->id().ToString();
```

Surface Embed does not manage the lifecycle of `WebContents`. If the child
WebContents is destroyed, the `<embed>` will become blank. If the `<embed>` is
removed, the previously attached `WebContents` will be detached. Changing
`data-content-id` on an existing plugin is not currently supported
([crbug.com/561637127](https://crbug.com/561637127)). To embed different
contents, update `data-content-id`, then remove and restore the `type` attribute
to force the plugin to be recreated with the new content ID.

---

## 2. Setup in HTML / JS (Frontend)

To embed the child page on your WebUI frontend, simply instantiate the `<embed>`
tag with your embedded `WebContents`'s content ID.

### 2.1. Render the `<embed>` Element
In your template:

```typescript
import {html} from '//resources/lit/v3_0/lit.rollup.js';

// Inside your custom WebUI component render() helper:
return html`
  <embed class="content"
         type="application/x-chromium-surface-embed"
         data-content-id="${this.contentId}">
  </embed>
`;
```

### 2.2. Requirements for Attributes
* **`type`**: Must match [kInternalPluginMimeType](https://source.chromium.org/chromium/chromium/src/+/main:components/surface_embed/common/constants.h;l=11;drc=da966bf8542039f60d23ff8d922166ef30725b5d),
which is `"application/x-chromium-surface-embed"`.
* **`data-content-id`**: Must contain the serialized string representation of
the `surface_embed::SurfaceEmbedHandle` token corresponding to the nested
`WebContents` (the `content_id` retrieved in Section 1.3).
