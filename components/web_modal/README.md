# Web Modal (`components/web_modal`)

The `web_modal` component manages web-contents modal dialogs in Chromium—prompts and dialogs scoped to a specific [`content::WebContents`](../../content/public/browser/web_contents.h) rather than the entire browser window (e.g., HTTP basic authentication, print preview, specialized permission prompts like File System Access confirmations or Page Embedded Permission Control modals, WebAuthn/passkey dialogs, and device choosers).

Because `components/web_modal` is decoupled from concrete desktop UI frameworks (Views browser windows, auxiliary bubbles, WebUI pickers, or embedders like CEF), containers rely on [`WebContentsModalDialogManagerDelegate`](web_contents_modal_dialog_manager_delegate.h) and [`WebContentsModalDialogHost`](web_contents_modal_dialog_host.h) to manage dialog hosting, bounds clipping, input blocking, and visibility lifecycle.

---

## Contract for Supporting Web-Contents Modal Dialogs

Any UI container or window hosting a `WebContents` that may display web-modal dialogs must adhere to a strict four-part lifecycle contract:

1. **Manager Creation (`CreateForWebContents`)**:
   Immediately upon `WebContents` creation, instantiate the per-WebContents dialog manager (`web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents)`).
2. **Attachment Wiring (`SetDelegate(delegate)`)**:
   Synchronously assign the container's delegate (`manager->SetDelegate(delegate)`) when attaching a `WebContents` to a window or view, strictly before any layout or activation runs.
3. **Dialog Hosting (`WebContentsModalDialogHost`)**:
   Implement `WebContentsModalDialogManagerDelegate::GetWebContentsModalDialogHost()` to supply a valid `WebContentsModalDialogHost` (`ModalDialogHost`).
4. **Detachment & Teardown Unsetting (`SetDelegate(nullptr)`)**:
   Explicitly clear the delegate (`manager->SetDelegate(nullptr)`) before tab reparenting between windows, C++ ownership hand-off (`std::move`), or window teardown.

---

### Requirement 1: Manager Creation (`CreateForWebContents`)

#### The Rule
Immediately upon `WebContents` creation (or during tab-helper initialization), the container must instantiate the per-WebContents dialog manager:
```cpp
web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
```

#### Why It Is Required
[`WebContentsModalDialogManager`](web_contents_modal_dialog_manager.h) is stored as a `content::WebContentsUserData<WebContentsModalDialogManager>` and is **not** automatically created by `content::WebContents::Create()`.

When a web feature or page requests a web-modal prompt, call sites query:
```cpp
web_modal::WebContentsModalDialogManager* manager =
    web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
```
If `CreateForWebContents()` was never called on that `WebContents`, `FromWebContents()` returns `nullptr`, causing prompt requests to silently drop or trigger null-pointer crashes.

#### Canonical Reference
* [`TabHelpers::AttachTabHelpers()`](../../chrome/browser/ui/tab_helpers.cc): Calls `web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents)` for standard browser tabs upon initialization.

---

### Requirement 2: Attachment Wiring & Synchronous Timing (`SetDelegate(delegate)`)

#### The Rule
When attaching a `WebContents` to a window or container view, the container must synchronously assign its delegate **before any layout or tab activation occurs**:
```cpp
web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
    ->SetDelegate(delegate);
```

#### Why Strict Synchronous Timing Is Required
Assigning the delegate synchronously prior to layout or activation prevents two classes of errors:

* **Blocked Display or Creation Crashes (Initial Attachment):**
  If a web feature requests a modal prompt before a container assigns its delegate, dialog creation either crashes immediately (when querying the unassigned delegate for a host) or leaves the requested prompt queued and invisible.
* **Mispositioning During Container Transitions (Tab Dragging / Reparenting):**
  Mispositioning specifically occurs when reparenting a `WebContents` that already has an active dialog across windows. While detached between containers, an active dialog temporarily has no modal host. If the destination window defers delegate assignment until after its initial layout pass, open dialogs cannot query the new host's bounds during layout, leaving them unpositioned or anchored to stale geometry.

As explicitly documented in Chromium's [`Browser::SetAsDelegate()`](../../chrome/browser/ui/browser.cc):
```cpp
// The modal dialog delegate must be set during SetAsDelegate (not via a
// separate TabStripModelObserver) to ensure it is wired before layout
// triggered by OnActiveTabChanged.
```

#### Canonical Reference
* [`Browser::SetAsDelegate(web_contents, true)`](../../chrome/browser/ui/browser.cc): Synchronously wires `BrowserWindowModalDialogDelegate::From(this)` when a tab is inserted into a browser window.

---

### Requirement 3: Dialog Host Contract (`WebContentsModalDialogHost`)

#### The Rule
The delegate's [`GetWebContentsModalDialogHost(web_contents)`](web_contents_modal_dialog_manager_delegate.h) method must return a valid [`web_modal::WebContentsModalDialogHost`](web_contents_modal_dialog_host.h) (implementing [`ModalDialogHost`](modal_dialog_host.h)).

#### Host Implementation Requirements
Any object acting as a `WebContentsModalDialogHost` must guarantee five core behaviors:

1. **Parent Native View (`GetHostView()`):**
   Must return the `gfx::NativeView` against which modal dialog widgets are parented and positioned.
2. **Bounds Constraining (`GetMaximumDialogSize()`):**
   Must return the maximum bounding `gfx::Size` (typically matching the visible bounds of the `WebContents` container) so dialogs are clipped inside the web area or browser window.
3. **Dialog Origin Positioning (`GetDialogPosition()`):**
   Must return the `gfx::Point` origin relative to `GetHostView()` where the dialog of the given `gfx::Size` should be placed.
4. **Position Observer Notification (`OnPositionRequiresUpdate`):**
   When the container window or view moves or resizes, the host must iterate through registered [`ModalDialogHostObserver`](modal_dialog_host.h) instances and call `OnPositionRequiresUpdate()` so open dialogs recalculate their origin.
5. **Destruction Safety Notification (`OnHostDestroying`):**
   Before the host object is destroyed, it **must** call `OnHostDestroying()` on all registered observers. If omitted, active dialog managers or open dialog widgets will retain dangling pointers to the destroyed host view.

#### Canonical Reference
* [`BrowserView::GetWebContentsModalDialogHost()`](../../chrome/browser/ui/views/frame/browser_view.h): Entrypoint on standard browser Views windows. This delegates directly to [`BrowserViewLayout::GetWebContentsModalDialogHost()`](../../chrome/browser/ui/views/frame/layout/browser_view_layout.h), which returns its internal [`BrowserModalDialogHostViews`](../../chrome/browser/ui/views/frame/browser_modal_dialog_host_views.h) instance (`dialog_host_`).
* [`PaymentHandlerModalDialogManagerDelegate::GetWebContentsModalDialogHost()`](../../chrome/browser/ui/views/payments/payment_handler_modal_dialog_manager_delegate.cc): Demonstrates how a custom secondary container delegates to its parent window's modal host.
* [`WebUIBubbleDialogView::GetWebContentsModalDialogHost()`](../../chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h): Implements `WebContentsModalDialogHost` to position and clip web-modal dialogs within Top Chrome WebUI bubbles.

---

### Requirement 4: Detachment & Teardown Unsetting (`SetDelegate(nullptr)`)

#### The Rule
Before a `WebContents` is reparented between windows, transferred to another container via C++ ownership hand-off (`std::move`), or left outliving its container window, the container **must explicitly clear the delegate**:
```cpp
web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
    ->SetDelegate(nullptr);
```

#### Circumstances & Why Unsetting Is Required

| Circumstance | Why `SetDelegate(nullptr)` Is Critical | Canonical Reference |
| :--- | :--- | :--- |
| **1. Tab Detachment / Dragging** (`TabDetachedAtImpl`) | While a tab is reparented between windows, it temporarily has no parent container. Unsetting prevents incoming prompts or layout updates during the drag from mutating or querying the source window. | [`Browser::TabDetachedAtImpl()`](../../chrome/browser/ui/browser.cc) calling [`SetAsDelegate(contents, false)`](../../chrome/browser/ui/browser.cc) |
| **2. Ownership Hand-off (`std::move`)** | When a secondary flow (`ProfilePickerSignInProvider`, onboarding window) transfers a `WebContents` to a normal browser window, it must unset its delegate prior to hand-off so the manager does not retain a pointer to the old container view. | [`ProfilePickerSignInProvider::FinishFlow()`](../../chrome/browser/ui/views/profiles/profile_picker_sign_in_provider.cc) calling [`ResetWebContentsDelegates()`](../../chrome/browser/ui/views/profiles/profile_picker_sign_in_provider.cc) |
| **3. Container Teardown** | `WebContentsModalDialogManager` stores [`raw_ptr<Delegate> delegate_`](web_contents_modal_dialog_manager.h). If a container window closes while the `WebContents` outlives it or tears down asynchronously, failing to clear `SetDelegate(nullptr)` triggers dangling pointer crashes (`DANGLING_PTR_DETECTED`). | [`DevToolsWindow::OnBrowserClosed()`](../../chrome/browser/devtools/devtools_window.cc) |
| **4. Tab Replacement** (`OnTabReplacedAt`) | When swapping out an existing `WebContents` in a tab slot for a new one, unhook the delegate on the discarded `WebContents` before attaching the incoming one. | [`Browser::OnTabReplacedAt()`](../../chrome/browser/ui/browser.cc) |

---

## Lifecycle & Page Navigations

Understanding how web-contents modal dialogs behave when the underlying `WebContents` navigates involves a clean division of responsibility between container hosts and dialog feature controllers:

* **Automatic Cross-Site Teardown (Containers)**:
  `WebContentsModalDialogManager` is a `content::WebContentsObserver`. When a primary main frame commits a cross-site (`!SameDomainOrHost(...)`) navigation, `DidFinishNavigation()` automatically notifies observers (`OnWillCloseOnNavigation`) and closes all active and queued child dialogs (`CloseAllDialogs()`). Container implementers **do not** need any navigation-handling code.
* **Automatic BackForwardCache (`BFCache`) Protection**:
  If a primary main frame navigates while a web-modal dialog is open, the manager automatically disables BFCache (`DisabledReasonId::kModalDialog`).
* **Same-Origin / Same-Site Dismissal (Dialog Features)**:
  Because the manager only auto-closes dialogs on cross-site navigations, dialogs remain open across same-origin or same-site primary main frame navigations. Dialog feature controllers whose prompts should dismiss on *any* page navigation (e.g., [`PrintPreviewDialogController::DidFinishNavigation()`](../../chrome/browser/printing/print_preview_dialog_controller.cc)) must observe `WebContentsObserver` directly and tear down their own prompt.
* **Pre-Teardown Navigation Hook**:
  Dialog feature controllers needing to distinguish whether their prompt closed due to a user clicking "Cancel" versus being forcibly closed by an automatic cross-site navigation teardown (e.g., [`DigitalIdentitySafetyInterstitialControllerDesktop::CloseOnNavigationObserver`](../../chrome/browser/ui/views/digital_credentials/digital_identity_safety_interstitial_controller_desktop.h)) can implement `WebContentsModalDialogManager::Observer::OnWillCloseOnNavigation()`.

---

## Summary Checklist for Implementers

```cpp
// 1. CREATION: Immediately after creating a WebContents
web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);

// 2. ATTACHMENT: Synchronously when attaching to window/container (before layout)
web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
    ->SetDelegate(this);

// 3. DIALOG HOSTING: Delegate implementation supplies the modal dialog host
web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
    content::WebContents* web_contents) override {
  return modal_dialog_host_;
}

// 4. DETACHMENT / TEARDOWN: Before detach, move, or window destruction
web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
    ->SetDelegate(nullptr);
```
