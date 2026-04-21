# Chrome WebUI Dialogs

This directory contains infrastructure for creating standardized, reusable WebUI-based dialogs in Chrome.

The goal is to reduce boilerplate code and provide a consistent, unified implementation for widget sizing, lifecycle management, and flicker prevention when building top-level WebUI dialogs. By using this framework, you avoid "reinventing the wheel" every time a new WebUI feature needs to be hosted in a native dialog window.

## Key Components

### `ChromeWebUIDialog`

This is the core reusable dialog delegate class. It acts as a bridge between the native Views system (`views::DialogDelegate`) and the web-based UI (`WebUIContentsWrapper::Host`).

Instead of writing a custom `DialogDelegate` for every new feature, you can use `ChromeWebUIDialog` to automatically handle:
*   Widget creation and modal configuration.
*   Hosting the `views::WebView`.
*   Auto-resizing the native window based on the web content's size without messy IPC sizing handshakes.
*   Applying rounded corners to prevent web content from bleeding over the native frame.
*   Safe, asynchronous memory teardown via `views::WidgetObserver`.

### `WebDialogSpec`

A declarative configuration struct used to customize the behavior of the dialog. Key options include:

*   **`min_size` / `max_size`**: Constraints for the auto-resizing behavior. The dialog will automatically adjust its bounds based on the web content between these two sizes. To lock a dimension to a fixed size, set `min == max`.
*   **`modal_type`**: Determines how the dialog interacts with the rest of the browser.
    *   `ui::mojom::ModalType::kWindow` (Default): A browser-modal dialog that locks the entire browser window.
    *   `ui::mojom::ModalType::kChild`: A tab-modal (web-modal) dialog that dims and locks a specific tab.
    *   `ui::mojom::ModalType::kNone`: A modeless, unanchored dialog that allows the user to interact with the rest of the browser while open.
*   **`parent_tab`**: If you set `modal_type = kChild`, you **must** provide the `base::WeakPtr<tabs::TabInterface>` of the tab you want the dialog to attach to.
*   **`wait_for_explicit_show`**: (Default: `true`) Keeps the dialog visually hidden until the JavaScript explicitly requests to be shown (via `ShowUI()`). This prevents jarring flashes of white or unstyled content while the DOM is initially rendering.
*   **`corner_radius`**: An optional override (`std::optional<int>`) for clipping the WebUI content to match the rounded corners of the dialog frame. If not set, it intelligently defaults to `views::DialogDelegate::GetCornerRadius()`.
*   **`show_close_button`**: (Default: `false`) Whether to show the native OS close button ('X') in the dialog frame.

## Understanding Widget Ownership

Historically, dialogs managed their own lifecycles (`NATIVE_WIDGET_OWNS_WIDGET`), which frequently led to memory leaks or use-after-free errors during teardown.

To guarantee memory safety, `ChromeWebUIDialog` uses the modern **`CLIENT_OWNS_WIDGET`** ownership model.
When you call `ChromeWebUIDialog::Show()`, it returns a `std::unique_ptr<views::Widget>`. **You (the caller) own this widget.**
*   When the `unique_ptr` goes out of scope, or you explicitly call `.reset()`, the dialog is immediately closed and safely destroyed.
*   If you need the dialog to stay open indefinitely, you must store the returned `unique_ptr` in a persistent class member.

## How to Use

To create a new WebUI dialog, you do not need to subclass `ChromeWebUIDialog`. Instead, you orchestrate the setup in three steps:

1.  **Define a `WebUIContentsWrapper`**: This wraps your specific WebUI page and its controller.
2.  **Configure a `WebDialogSpec`**: Define your sizing bounds, modality, and display preferences.
3.  **Call `ChromeWebUIDialog::Show()`**: Pass the configuration and wrapper to instantly generate your dialog.

### Example

```cpp
#include "chrome/browser/ui/views/web_dialogs/chrome_webui_dialog.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/base/mojom/ui_base_types.mojom.h"

// 1. Create or obtain your WebUIContentsWrapper.
// Typically you would use a factory or subclass specific to your feature.
auto contents_wrapper = std::make_unique<WebUIContentsWrapper>(
    GURL("chrome://my-feature/"), profile, ...);

// 2. Configure the spec.
webui_dialog::WebDialogSpec spec;
spec.min_size = gfx::Size(300, 200);
spec.max_size = gfx::Size(600, 500);
spec.wait_for_explicit_show = true; // Recommended to avoid load flicker.

// Example: Making it a tab-modal dialog
spec.modal_type = ui::mojom::ModalType::kChild;
spec.parent_web_contents = browser->tab_strip_model()->GetActiveTab()->GetWeakPtr();

// 3. Show the dialog.
// The caller takes ownership of the returned Widget. It will close automatically
// when my_widget_ptr is destroyed.
std::unique_ptr<views::Widget> my_widget_ptr =
    webui_dialog::ChromeWebUIDialog::Show(
        parent_native_window, std::move(contents_wrapper), spec);
```

## Best Practices

*   **Avoid Flicker**: Always set `wait_for_explicit_show = true` unless you have a specific reason not to. Ensure your WebUI JavaScript/TypeScript calls the equivalent of `Embedder.showUI()` on the host controller when the DOM is fully rendered and ready to be displayed.
*   **Auto-Resize Constraints**: Set reasonable `min_size` and `max_size` to prevent the dialog from becoming too small or overflowing the screen. The delegate automatically clamps the bounds to the display's work area natively to protect against untrusted renderer sizes.
*   **Modal Selection**: Do not use `kWindow` (browser-modal) unless your dialog truly needs to block the user from interacting with *all* tabs in the window. Prefer `kChild` (tab-modal) for page-specific actions.

## Implementation Details

For more details on the architecture and design decisions, see[go/unify-top-chrome-webui-dialog](http://go/unify-top-chrome-webui-dialog).
