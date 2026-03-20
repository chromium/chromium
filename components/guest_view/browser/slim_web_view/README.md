# SlimWebview

`SlimWebview` is a minimal, non-extensions-based implementation of `<webview>`
for Chrome WebUIs.

It is used **only on Android mobile**, where extensions are unavailable.

## Enabling in C++

When implementing a WebUI that runs both on desktop platforms and
Android mobile, you should check the `ENABLE_EXTENSIONS_CORE` buildflag to
decide which implementation of `<webview>` to use.

To enable `SlimWebview` in your WebUI:

### 1. WebUI Configuration

In your `MojoWebUIController` implementation:
- Set the `content::BindingsPolicyValue::kSlimWebView` bindings policy.
- Add `kGuestViewSharedResources` from
  [guest_view_shared_resources_map.h](https://source.chromium.org/chromium/chromium/src/+/main:chrome/grit/guest_view_shared_resources_map.h)
  to your `WebUIDataSource`.

Example from [glic_ui.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/glic/host/glic_ui.cc):

```cpp
#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  auto bindings = web_ui->GetBindings();
  bindings.Put(content::BindingsPolicyValue::kSlimWebView);
  web_ui->SetBindings(bindings);
  source->AddResourcePaths(kGuestViewSharedResources);
#endif
```

### 2. Mojo Interface

Your `WebUIController` should directly inherit from `guest_view::SlimWebViewPageHandlerFactory`.

Example from [glic_ui.h](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/glic/host/glic_ui.h):

```cpp
class GlicUI : public ui::MojoWebUIController,
#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
               public guest_view::SlimWebViewPageHandlerFactory,
#endif
               ...
 public:
#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  using SlimWebViewPageHandlerFactory::BindInterface;
#endif

  ...
 private:
#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  using SlimWebViewPageHandlerFactory::CreatePageHandler;
#endif
```

### 3. Interface Binding

Register the factory in `chrome/browser/chrome_browser_interface_binders_webui_parts_features.cc`.

```cpp
#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  RegisterWebUIControllerInterfaceBinder<guest_view::mojom::PageHandlerFactory,
                                         glic::GlicUI,
                                         your_namespace::YourUI>(map);
#endif
```

## Frontend Configuration

### BUILD.gn

You need to add `ts_path_mappings` for the shared `SlimWebview` resources and include the dependencies conditionally.

Example from [chrome/browser/resources/glic/BUILD.gn](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/glic/BUILD.gn):
```gn
  if (enable_guest_view && !enable_extensions_core) {
    ts_deps += [ "//chrome/browser/resources/guest_view_shared:build_ts" ]

    ts_path_mappings +=
        [ "/shared/guest_view/*|" + rebase_path(
              "$root_gen_dir/chrome/browser/resources/guest_view_shared/tsc/*",
              target_gen_dir) ]
  }
```

## Frontend Usage

### Shared Types

You can use a shared type to support both webview implementations conditionally.

Example from
[web_view_type.ts](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/glic/shared/web_view_type.ts):
```typescript
// <if expr="not enable_extensions_core">
import '/shared/guest_view/slim_webview.js';
// </if>
import type {SlimWebviewElement} from '/shared/guest_view/slim_webview.js';

export type WebViewType = chrome.webviewTag.WebView|SlimWebviewElement;
```

### Conditional Import

Use preprocess `if` expressions to import `slim_webview.js` only when extensions
are not enabled.

```typescript
// <if expr="not enable_extensions_core">
import '/shared/guest_view/slim_webview.js';
// </if>
```

The markup remains the same as a standard `<webview>`, but it will be backed by
`SlimWebviewElement` on Android.
