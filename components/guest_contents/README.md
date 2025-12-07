`GuestContents` is an experimental component that allows you to embed an
independent `content::WebContents` (a "guest") within an outer
`content::WebContents`, which is typically a WebUI page. This is useful for
displaying external web content inside your WebUI in a sandboxed way, similar to
the functionality of the `<webview>` tag in Chrome Apps.

The core mechanism involves swapping an `<iframe>` in the outer WebUI's renderer
process with the main frame of the guest `WebContents` from the browser process.
This process is orchestrated via the `GuestContentsHost` Mojo interfaces.

`GuestContents` is **NOT** for production use due to privacy and security
reasons. See [Security Considerations](#security-considerations).

# Core Concepts

-   **Guest `WebContents`**: The WebContents instance that you want to embed.

-   **Outer `WebContents`**: The host WebContents, which is your WebUI page.

-   `GuestContentsHandle`: A browser-side handle for the guest `WebContents`. It
    assigns a unique `GuestId` to the guest and manages its attachment to an
    outer `WebContents`. Its lifetime is tied to the guest `WebContents`.

-   `guest_contents::mojom::GuestContentsHost`: A Mojo interface implemented in
    the browser process. The outer WebUI's renderer calls this interface to
    request the attachment of a guest. The WebUI's `WebUIController` handles the
    binding of this interface via a `BindInterface()` method.

-   `guest_contents::renderer::SwapRenderFrame`: A renderer-side C++ function
    that initiates the guest attachment process by calling the
    `GuestContentsHost` Mojo interface.

# How-To Guide

Here is a step-by-step guide to embedding a guest `WebContents` in your WebUI.
[webui_examples](https://source.chromium.org/chromium/chromium/src/+/main:ui/webui/examples/README.md)
uses this pattern and is a good example to follow.

### 1. Browser-Side Setup

In your WebUI's browser-side C++ code:

-   **Register the `GuestContentsHost` interface** in `ContentBrowserClient` for
    your `WebUIController`.

    ```c++
    // ui/webui/examples/browser/content_browser_client.cc
    void ContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(...) {
      // ...
      // `Browser` is a WebUIController.
      RegisterWebUIControllerInterfaceBinder<
          guest_contents::mojom::GuestContentsHost, Browser>(map);
    }
    ```

-   **Bind the `GuestContentsHost` Mojo Interface**: Your WebUI controller must
    expose the `GuestContentsHost` interface to its renderer.

    ```c++
    // ui/webui/examples/browser/ui/web/browser.h
    class Browser : public ui::MojoWebUIController, ... {
      // ...
      void BindInterface(
          mojo::PendingReceiver<guest_contents::mojom::GuestContentsHost> receiver);
      // ...
    };

    // ui/webui/examples/browser/ui/web/browser.cc
    void Browser::BindInterface(
        mojo::PendingReceiver<guest_contents::mojom::GuestContentsHost> receiver) {
      guest_contents::GuestContentsHostImpl::Create(web_ui()->GetWebContents(),
                                                    std::move(receiver));
    }
    ```

-   **Create and Own the Guest `WebContents`**: For example, in your
    WebUIController's constructor, create the guest WebContents.

    ```c++
    // ui/webui/examples/browser/ui/web/browser.h
    class Browser : public ui::MojoWebUIController, ... {
      // ...
      std::unique_ptr<content::WebContents> guest_contents_;
    };

    // ui/webui/examples/browser/ui/web/browser.cc
    Browser::Browser(content::WebUI* web_ui)
        : ui::MojoWebUIController(web_ui, false) {
      content::BrowserContext* browser_context =
          web_ui->GetWebContents()->GetBrowserContext();
      // ...
      content::WebContents::CreateParams params(browser_context);
      guest_contents_ = content::WebContents::Create(params);
      // ...
    }
    ```

-   **Create a `GuestContentsHandle` and Pass its ID to the Frontend**: The
    handle provides the unique ID needed to identify the guest. Pass this ID to
    your frontend JavaScript, for example, using `loadTimeData` or via a Mojo
    interface. The following example uses `loadTimeData`.

    ```c++
    // ui/webui/examples/browser/ui/web/browser.cc
    Browser::Browser(content::WebUI* web_ui) : ... {
      // ... (create guest_contents_)
      guest_contents::GuestContentsHandle::CreateForWebContents(
          guest_contents_.get());
      auto* guest_handle = guest_contents::GuestContentsHandle::FromWebContents(
          guest_contents_.get());
      html_source->AddInteger("guest-contents-id", guest_handle->id());
    }
    ```

### 2. Renderer-Side Setup (C++)

To bridge the gap between your frontend JavaScript and the browser process, you
need some C++ code in the renderer.

-   **Expose C++ Bindings to JavaScript**: Inject functions into the renderer's
    JavaScript context. The example uses a `RenderFrameObserver` to add a
    `webshell` object with C++-backed functions when the WebUI is ready.

    ```c++
    // ui/webui/examples/renderer/render_frame_observer.cc
    void RenderFrameObserver::ReadyToCommitNavigation(...) {
      V8BinderContext binder_context(render_frame());
      binder_context.CreateWebshellObject();
      binder_context.AddCallbackToWebshellObject(
          "attachIframeGuest", base::BindRepeating(&AttachIframeGuest));
      // ...
    }
    ```

-   **Implement the Binding**: The AttachIframeGuest function parses the
    arguments from JavaScript, gets the `content::RenderFrame*` for the
    `<iframe>`, and calls the `guest_contents` helper function.

    ```c++
    // ui/webui/examples/renderer/render_frame_observer.cc
    void AttachIframeGuest(const v8::FunctionCallbackInfo<v8::Value>& args) {
      // ... argument parsing ...
      int guest_contents_id = args[0].As<v8::Int32>()->Value();
      content::RenderFrame* render_frame = GetRenderFrame(args[1]);
      // ...
      guest_contents::renderer::SwapRenderFrame(render_frame, guest_contents_id);
    }
    ```

This `SwapRenderFrame` function handles the final step of calling the
[`GuestContentsHost.Attach`](https://source.chromium.org/chromium/chromium/src/+/main:components/guest_contents/common/guest_contents.mojom;l=16;drc=d6543287271f614b90e40373609a38a1092e3e63)
Mojo method, which completes the attachment in the browser process.

### 3. Renderer-Side Setup (TypeScript/HTML)

In your WebUI's frontend code, add a placeholder element that will be swapped
with the guest. An `<iframe>` is a good choice, although other frame-like
element might also work. In the following example, `<webview>` is a custom web
component that has a `<iframe>` child element.

```html
<!-- ui/webui/examples/resources/browser/index.html -->
<webview id="webview"></webview>
```

In your TypeScript/JavaScript, get the `guest-contents-id` from `loadTimeData`.
Then, call a C++ binding to trigger the attachment. The example uses a
`webshell` object injected into the renderer for this communication.

```typescript
// ui/webui/examples/resources/browser/index.ts
class WebviewElement extends HTMLElement {
  public iframeElement: HTMLIFrameElement;
  private guestContentsId: number;

  constructor() {
    super();
    this.iframeElement = document.createElement('iframe');
    this.appendChild(this.iframeElement);

    this.guestContentsId = loadTimeData.getInteger('guest-contents-id');
    const iframeContentWindow = this.iframeElement.contentWindow;

    // This is the key call that triggers the C++ logic.
    webshell.attachIframeGuest(this.guestContentsId,
                               iframeContentWindow);
  }
  // ...
}
```

### 4. Controlling the Guest

`GuestContents` provides only basic embedding functionalities, including sizing,
painting and event routing. The WebUI needs to provide their own implementation
for additional controls over the guest.

Navigation is a common and concrete example of additional control. To add
support for navigation, you can:

-   **Define a Mojo Interface**: Add methods like `Navigate`, `GoBack`, and
    `GoForward` to your WebUI's page handler Mojo interface.

-   **Implement in the Browser**: Implement these methods in your `PageHandler`
    class. They should retrieve the guest `WebContents` from your WebUI
    controller and use its `NavigationController`.

-   **Call from the Frontend**: Call these Mojo methods from your TypeScript
    code to control the guest's navigation, for example, from back/forward
    button events.

# GuestContents vs GuestView

While both `GuestContents` and
[`GuestView`](https://source.chromium.org/chromium/chromium/src/+/main:components/guest_view/README.md;l=1;drc=995605f8d603b59d99180674f304e595467d08dc)
are used for embedding web content, they are designed for different use cases
and have significant architectural differences, primarily due to the ongoing
migration of `GuestView` to MPArch.

### Core Distinction: Inner WebContents vs. MPArch

The fundamental difference lies in how the guest content is hosted:

*   **`GuestContents`** was created to explicitly **retain the use of inner
    `WebContents`**. This allows the embedder to have direct access to the
    guest's `WebContents` instance and its full API. This is critical for use
    cases that need to attach `TabHelpers` (e.g., for autofill, permissions,
    downloads) and interact deeply with the guest's state and navigation, such
    as when embedding a full-featured browser tab.

*   **`GuestView`** is migrating to MPArch (Multiple Page Architecture) and will
    no longer use inner `WebContents`. Instead, the guest is hosted in a
    `GuestPageHolder`. This abstracts the guest's `WebContents` away from the
    embedder, providing stronger isolation but preventing the direct API access
    that `GuestContents` allows.

### Lifetime Management

The ownership model for the guest `WebContents` is another key differentiator:

*   In **`GuestContents`**, the lifetimes of the inner (guest) and outer
    `WebContents` are **decoupled**. The client that creates the inner
    `WebContents` is responsible for its lifetime. The outer `WebContents` does
    not own the guest, which allows for flexible scenarios like detaching a
    guest and re-attaching it elsewhere (e.g., dragging a tab out of a window).

*   In **`GuestView`**, the outer `WebContents` typically **owns** the guest
    `WebContents` after it is attached.

### API and Complexity

*   **`GuestContents`** offers a **simpler, more direct API** focused purely on
    embedding.

*   **`GuestView`** can be more complex, especially when used via extension's
    `<webview>` tag, which brings in extension-specific concepts and
    dependencies that may be unnecessary for non-extension use cases.

# Security Considerations

`GuestContents` inherits the security posture of the underlying primitives in
`//content` and Blink. It is not inherently more or less secure than the
pre-MPArch `GuestView` model, as both rely on the same complex mechanisms. This
complexity can make security analysis difficult and may be a source of
vulnerabilities.

Under the hood,

*   **At the `WebContents` level**: The guest and outer `WebContents` are
    connected via `WebContentsTreeNode` after a call to
    `WebContents::Attach(Unowned)InnerWebContents()`. This creates a
    relationship (e.g., `WebContents::GetOuterWebContents()`) that adds
    complexity where greater isolation would be ideal. This is used by
    `GuestContents` and pre-MPArch `GuestView`.

*   **At the frame level**: The outer `<iframe>` element and the guest's main
    frame are connected by a
    [`RenderFrameProxyHost`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/renderer_host/render_frame_proxy_host.h;drc=b156bea6f54b90c33f24d96f2f75925f8997044c).
    In Blink, this is represented as a `blink::RemoteFrame`. This used by all
    embedding techniques, including `GuestContents`, standard `<iframe>`, and
    both pre- and post-MPArch `GuestView`.

Reusing the general-purpose `<iframe>`-related primitives is considered a
primary security risk. This IPC channel provides a much larger API surface than
is strictly necessary for embedding a guest, including features like
`window.opener` and `window.postMessage()`. Future changes to `<iframe>`
implementation could unintentionally introduce vulnerabilities or break the
security isolation between a guest and its embedder.

The long-term goal (https://crbug.com/416609971) is to develop more minimal,
purpose-built primitives for embedding that expose only the essential IPCs for
painting, sizing, and event routing, thereby reducing the potential attack
surface.
