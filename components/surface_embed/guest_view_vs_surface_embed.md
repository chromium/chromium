# GuestView vs. SurfaceEmbed

`GuestView` and `SurfaceEmbed` are two different mechanisms in Chromium for
nesting or embedding web content (a child document/view) inside an outer
document (such as a WebUI page or Chrome App). This doc describes their differences.

## Overview

### GuestView
`GuestView` (found in `//components/guest_view`) is the implementation
underpinning the `<webview>` tag in classic Chrome Apps, `<controlledframe>` in
isolated web apps (IWA), and `MimeHandlerView`. It reuses the Out-of-process
iframe (OOPIF) infrastructure.
The embedding relationship is recorded in the browser process in
`content::FrameTree` and `WebContentsTreeNode`.

Historically, it nested an inner `content::WebContents` inside an outer
`content::WebContents`. Modern `GuestView` implementations are migrating towards
a Multiple Page Architecture (MPArch) to avoid using inner `WebContents` and
instead structure the guest as a separate page within a `GuestPageHolder`.

### SurfaceEmbed
`SurfaceEmbed` (found in `//components/surface_embed`) is a modern, lightweight,
and highly secure alternative. It embeds an independent `content::WebContents`
(e.g., a browser tab's content) inside an outer WebUI or document using a
`blink::WebPlugin`.

Rather than reproducing standard iframe frame/proxy structures, `SurfaceEmbed`
only bridges the visual layer by binding the inner `WebContents`'s
`RenderWidgetHostViewChildFrame` directly to a compositor surface layer
(`cc::SurfaceLayer`) managed by the renderer-side plugin and the browser side
`content::SurfaceEmbedConnector`. It does not provide a scripting API between
the parent and the child document.

---

## Comparison Table

| Feature | GuestView (`<webview>`) | SurfaceEmbed (`<embed>`) |
| :--- | :--- | :--- |
| **DOM Element** | `<webview>` custom HTML element | `<embed>` with custom MIME type |
| **Browser-side Container** | `WebContents` (traditional) or `GuestPageHolder` (MPArch) | `WebContents` |
| **Compositing Attachment** | `RemoteFrame` + `CrossProcessFrameConnector` (reusing OOPIF) | `blink::WebPlugin` + `SurfaceEmbedConnector` |
| **Ownership** | Outer frame owns the guest (Tightly coupled) | Decoupled (User owns child `WebContents`) |
| **Scripting APIs (e.g., `postMessage`)** | Fully supported (uses iframe-like remote frame proxies) | **None** |
| **Frontend API** | Extensive (navigation, script injection, event listeners) | **None** |
| **Navigation Control** | JS-driven (via `src` attribute) | Browser-driven (via `WebContents` API) |

---

## Core Differences

### 1. Ownership Model

#### GuestView Ownership
In `GuestView`, the guest context is tightly coupled to the outer `WebContents`
that embeds it.
* **Pre-MPArch**: The outer `WebContents` owns the inner `WebContents` once
attached.
* **MPArch**: The guest's state is managed as a `GuestPageHolder` within the
parent's `FrameTree` / Page context. Therefore, the guest page is owned and
destroyed directly along with the parent frame/page hierarchy. Its lifetime
cannot be easily separated or reparented.
* **Preloading Limitation**: Because of this tight coupling, you cannot preload
an embedded `WebContents` for `<webview>` and then attach it when needed.

#### SurfaceEmbed Ownership
Under `SurfaceEmbed`, the lifetimes of the outer frame and child `WebContents`
are **completely decoupled**:
* The child `WebContents` is typically spawned and owned by the browser-side
client (e.g., the browser's tab strip or side panel coordinator), NOT the outer
`WebContents`.
* In `SurfaceEmbedConnector`, the connection to child is established via a
`raw_ptr<content::WebContents>`. It does not govern the inner page's lifecycle.
* Attachment is managed in the browser using the static helper
`content::SurfaceEmbedConnector::Attach` and detachment is done via
`content::SurfaceEmbedConnector::Detach`. This allows the inner `WebContents` to
be detached, moved, and re-attached to a completely different browser
window/WebUI without destroying the page or losing its state. This enables
preloading and deferred attachment.

---

### 2. Container (WebContents vs. GuestPageHolder)

#### GuestView Containers
* Historically, GuestView used an inner `WebContents` (attached via
`AttachAsInnerWebContents()`).
* In modern Chromium, to simplify the browser processes and improve security
isolation, GuestViews are migrating to **MPArch**. Under MPArch, the guest does
*not* get its own `WebContents` instance. Instead, it is represented as a
separate `Page` hosted in a `GuestPageHolder` inside the outer `WebContents`'s
`FrameTree`. This abstracts `WebContents` operations away from the guest and
restricts direct `WebContents` level API access (such as attaching arbitrary
tab helpers).
* *Note*: The `GuestPageHolder` work is currently behind a disabled-by-default
feature flag.

#### SurfaceEmbed Containers
* `SurfaceEmbed` **retains the use of a full, independent `content::WebContents`
instance** (the child WebContents) for the embedded child.
* Because the child remains a standard `WebContents` instance, the browser can
attach arbitrary `TabHelpers` (e.g., find-in-page, autofill, permission
managers, print preview, downloads) and interact directly with its navigation
and lifecycle APIs.
* However, unlike pre-MPArch `GuestView`, `SurfaceEmbed` does not route
input/layout/compositor frames through a nested frame tree or
`RenderFrameProxyHost`. Instead, `content::SurfaceEmbedConnector` coordinates
the mapping. When the child creates its `RenderWidgetHostViewChildFrame`, the
connector sends the `FrameSinkId` to the renderer's `SurfaceEmbedWebPlugin`,
which embeds the surface using `cc::SurfaceLayer`.

---

### 3. postMessage and OOPIF Scripting APIs

#### GuestView Scripting
Because `<webview>` / GuestView embeds the guest frame using cross-process frame
routing primitives (`RenderFrameProxyHost` in the browser, `blink::RemoteFrame`
in the renderer), it inherits the standard HTML frame hierarchy connectivity.
* This allows the parent document to acquire a reference to the child's
`contentWindow`.
* Scripts in the parent document can call `postMessage` to communicate with the
child.
* *Security Risk:* Reusing these general-purpose iframe-related primitives
exposes a broad IPC attack surface. A compromise in the parent or child renderer
may bypass isolation bounds if there're bugs in the frame proxy implementation.
This is considered risky for a parent frame that contains sensitive information
(e.g. the Browser UI).

#### SurfaceEmbed Scripting
`SurfaceEmbed` intentionally excludes all scripting and standard
cross-frame communication APIs:
* There is **no `contentWindow` exposure** and no JavaScript representation of
the child page in the parent DOM window tree.
* Scripting channels like `postMessage` or frame relation properties (like
`window.parent` or `window.opener`) **do not work** and are completely blocked.
* If communication is needed, the user must build their own custom messaging
channel between the parent and child (e.g., via browser-side routing).
* The interaction is solely visual and input-focused. This includes:
  1. The parent frame embeds the the plugin's surface.
  2. The browser routes mouse/keyboard input events and focus to the child view.
  3. The child frame posts its Compositor frame sink/surface to the plugin's
  `cc::SurfaceLayer`.

---

### 4. `<webview>`'s API vs. `<embed>`'s API

#### Navigation Control
* **`<webview>`**: Initial load and navigation are done by setting the `src`
attribute in JavaScript.
* **`<embed>`**: Loading and navigation are done by calling the `WebContents`
API on the embedded `WebContents` in the browser process.

#### Frontend API
The `<webview>` tag exposes a rich, comprehensive JavaScript API for scripting,
navigation, and state inspection:
```JavaScript
const webview = document.querySelector('webview');
webview.src = "https://example.com";
webview.addEventListener('contentload', () => { ... });
webview.executeScript({ code: "alert('Hello')" });
webview.insertCSS({ code: "body { background: red; }" });
webview.goBack();
```
These APIs allow the webpage to control navigation, intercept network requests,
customize context menus, inject scripts, and listen to detailed page load/error
states.

The `<embed>` tag used by `SurfaceEmbed` does **not expose any programmatic
API** to the embedding page's JavaScript:
```html
<embed type="application/x-chromium-surface-embed" data-content-id="[content-id]">
```
* The `content-id` is generated by
`SurfaceEmbedHandle::CreateForWebContents(web_contents)->id()`.
* The `<embed>` element is treated by JavaScript as a standard web plugin
(similar to a PDF reader).
* It does not have properties like `.src`, methods like `.goBack()`, or events
like `contentload`.
* Any navigation, history control, or scripting must be driven **out-of-band on
the browser side**. For example, the parent WebUI communicates with its C++
`WebUIController` (using Mojo or `chrome.send`), and the browser-side code
directly invokes the child `WebContents`'s `NavigationController`.

---

### 5. Web Capabilities Differences

This is a non-exhaustive list of differences.

* **`window.open(url, target)`**:
  * **`<webview>`**: Will navigate the embedded page, even if the target is
  `_blank` or `_top`.
  * **`<embed>`**: Can customize the behavior by overriding
  `WebContentsDelegate` on the embedded `WebContents` (not possible with
  `<webview>`).
* **Intersection Observer API**:
  * **`<webview>`**: The position of the `<webview>` in the parent document
  affects the visibility results for elements inside the guest.
  * **`<embed>`**: The positioning in the parent document does **not** affect
  the results inside the embedded page, as it is treated as an independent
  visual surface.
* **Screen Capturing**:
  * **`<webview>`**: Cannot be captured (via `navigator.mediaDevices`) if the
  parent document is invisible (e.g., in an inactive tab).
  * **`<embed>`**: Can always be captured regardless of parent visibility.
