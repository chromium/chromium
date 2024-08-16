// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/blocked_content/popunder_preventer.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace {

gfx::Size RestrictToPlatformMinimumSize(const gfx::Size& min_size) {
#if BUILDFLAG(IS_MAC)
  // http://crbug.com/78973 - MacOS does not handle zero-sized windows well.
  gfx::Size adjusted_min_size(1, 1);
  adjusted_min_size.SetToMax(min_size);
  return adjusted_min_size;
#else
  return min_size;
#endif
}

class ConstrainedWebDialogDelegateViews;

// The specialized WebView that lives in a constrained dialog.
class ConstrainedDialogWebView : public views::WebView,
                                 public ConstrainedWebDialogDelegate,
                                 public views::WidgetDelegate {
  METADATA_HEADER(ConstrainedDialogWebView, views::WebView)

 public:
  ConstrainedDialogWebView(content::BrowserContext* browser_context,
                           std::unique_ptr<ui::WebDialogDelegate> delegate,
                           content::WebContents* web_contents,
                           const gfx::Size& min_size,
                           const gfx::Size& max_size);
  ConstrainedDialogWebView(const ConstrainedDialogWebView&) = delete;
  ConstrainedDialogWebView& operator=(const ConstrainedDialogWebView&) = delete;
  ~ConstrainedDialogWebView() override;

  // ConstrainedWebDialogDelegate:
  const ui::WebDialogDelegate* GetWebDialogDelegate() const override;
  ui::WebDialogDelegate* GetWebDialogDelegate() override;
  void OnDialogCloseFromWebUI() override;
  std::unique_ptr<content::WebContents> ReleaseWebContents() override;
  gfx::NativeWindow GetNativeDialog() override;
  content::WebContents* GetWebContents() override;
  gfx::Size GetConstrainedWebDialogPreferredSize() const override;
  gfx::Size GetConstrainedWebDialogMinimumSize() const override;
  gfx::Size GetConstrainedWebDialogMaximumSize() const override;

  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override;
  void WindowClosing() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  std::u16string GetWindowTitle() const override;
  std::u16string GetAccessibleWindowTitle() const override;
  views::View* GetContentsView() override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  bool ShouldShowCloseButton() const override;

  // views::WebView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

 private:
  base::WeakPtr<content::WebContents> initiator_web_contents_;

  // Showing a dialog should not activate, but on the Mac it does
  // (https://crbug.com/1073587). Make sure it cannot be used to generate a
  // popunder.
  PopunderPreventer popunder_preventer_;

  std::unique_ptr<ConstrainedWebDialogDelegateViews> impl_;
};

BEGIN_METADATA(ConstrainedDialogWebView)
END_METADATA

class WebDialogWebContentsDelegateViews
    : public ui::WebDialogWebContentsDelegate {
 public:
  WebDialogWebContentsDelegateViews(
      content::BrowserContext* browser_context,
      content::WebContents* initiator_web_contents,
      ConstrainedDialogWebView* web_view)
      : ui::WebDialogWebContentsDelegate(
            browser_context,
            std::make_unique<ChromeWebContentsHandler>()),
        initiator_web_contents_(initiator_web_contents->GetWeakPtr()),
        web_view_(web_view) {}

  WebDialogWebContentsDelegateViews(const WebDialogWebContentsDelegateViews&) =
      delete;
  WebDialogWebContentsDelegateViews& operator=(
      const WebDialogWebContentsDelegateViews&) = delete;

  ~WebDialogWebContentsDelegateViews() override = default;

  // ui::WebDialogWebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    // Forward shortcut keys in dialog to our initiator's delegate.
    // http://crbug.com/104586
    if (!initiator_web_contents_)
      return false;

    auto* delegate = initiator_web_contents_->GetDelegate();
    if (!delegate)
      return false;
    return delegate->HandleKeyboardEvent(initiator_web_contents_.get(), event);
  }

  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override {
    if (source != web_view_->GetWebContents())
      return;

    if (!initiator_web_contents_)
      return;

    // views::WebView is only a delegate for a WebContents it creates itself via
    // views::WebView::GetWebContents(). ConstrainedDialogWebView's constructor
    // sets its own WebContents (via an override of WebView::GetWebContents()).
    // So forward this notification to views::WebView.
    web_view_->ResizeDueToAutoResize(source, new_size);

    content::WebContents* top_level_web_contents =
        constrained_window::GetTopLevelWebContents(
            initiator_web_contents_.get());
    if (top_level_web_contents) {
      constrained_window::UpdateWebContentsModalDialogPosition(
          web_view_->GetWidget(),
          web_modal::WebContentsModalDialogManager::FromWebContents(
              top_level_web_contents)
              ->delegate()
              ->GetWebContentsModalDialogHost());
    }
  }

 private:
  base::WeakPtr<content::WebContents> initiator_web_contents_;
  raw_ptr<ConstrainedDialogWebView> web_view_;
};

// Views implementation of ConstrainedWebDialogDelegate.
class ConstrainedWebDialogDelegateViews
    : public ConstrainedWebDialogDelegate,
      public ui::WebDialogWebContentsDelegate {
 public:
  ConstrainedWebDialogDelegateViews(
      content::BrowserContext* context,
      std::unique_ptr<ui::WebDialogDelegate> delegate,
      content::WebContents* initiator_web_contents,
      ConstrainedDialogWebView* view);
  // |browser_context| must outlive |this| instance.
  ConstrainedWebDialogDelegateViews(
      content::BrowserContext* browser_context,
      std::unique_ptr<ui::WebDialogDelegate> web_dialog_delegate,
      std::unique_ptr<WebDialogWebContentsDelegate> tab_delegate);

  ConstrainedWebDialogDelegateViews(const ConstrainedWebDialogDelegateViews&) =
      delete;
  ConstrainedWebDialogDelegateViews& operator=(
      const ConstrainedWebDialogDelegateViews&) = delete;

  ~ConstrainedWebDialogDelegateViews() override;

  bool closed_via_webui() const;

  // ConstrainedWebDialogDelegate interface.
  const ui::WebDialogDelegate* GetWebDialogDelegate() const override;
  ui::WebDialogDelegate* GetWebDialogDelegate() override;
  void OnDialogCloseFromWebUI() override;
  std::unique_ptr<content::WebContents> ReleaseWebContents() override;
  content::WebContents* GetWebContents() override;
  gfx::Size GetConstrainedWebDialogMinimumSize() const override;
  gfx::Size GetConstrainedWebDialogMaximumSize() const override;
  gfx::Size GetConstrainedWebDialogPreferredSize() const override;

  // Resize the dialog to the given size.
  virtual void ResizeToGivenSize(const gfx::Size size);

  // ui::WebDialogWebContentsDelegate:
  void CloseContents(content::WebContents* source) override {
    view_->GetWidget()->Close();
  }

  // contents::WebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, view_->GetFocusManager());
  }

  // ConstrainedWebDialogDelegate:
  gfx::NativeWindow GetNativeDialog() override {
    return view_->GetWidget()->GetNativeWindow();
  }

 private:
  std::unique_ptr<ui::WebDialogDelegate> web_dialog_delegate_;

  // Holds the HTML to display in the constrained dialog.
  std::unique_ptr<content::WebContents> web_contents_holder_;

  // WeakPtr to the WebContents in |web_contents_holder_| for the lifetime of
  // that object, even if ReleaseWebContents() gets called.
  base::WeakPtr<content::WebContents> web_contents_;

  // Was the dialog closed from WebUI (in which case |web_dialog_delegate_|'s
  // OnDialogClosed() method has already been called)?
  bool closed_via_webui_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  raw_ptr<views::WebView> view_;

  std::unique_ptr<WebDialogWebContentsDelegate> override_tab_delegate_;
};

using content::WebContents;
using input::NativeWebKeyboardEvent;
using ui::WebDialogDelegate;
using ui::WebDialogWebContentsDelegate;

ConstrainedWebDialogDelegateViews::ConstrainedWebDialogDelegateViews(
    content::BrowserContext* browser_context,
    std::unique_ptr<WebDialogDelegate> web_dialog_delegate,
    content::WebContents* initiator_web_contents,
    ConstrainedDialogWebView* view)
    : WebDialogWebContentsDelegate(
          browser_context,
          std::make_unique<ChromeWebContentsHandler>()),
      web_dialog_delegate_(std::move(web_dialog_delegate)),
      closed_via_webui_(false),
      view_(view),
      override_tab_delegate_(
          std::make_unique<WebDialogWebContentsDelegateViews>(
              browser_context,
              initiator_web_contents,
              view)) {
  DCHECK(web_dialog_delegate_);
  web_contents_holder_ =
      WebContents::Create(WebContents::CreateParams(browser_context));
  web_contents_ = web_contents_holder_->GetWeakPtr();
  zoom::ZoomController::CreateForWebContents(web_contents_.get());
  web_contents_->SetDelegate(override_tab_delegate_.get());
  blink::RendererPreferences* prefs = web_contents_->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(
      prefs, Profile::FromBrowserContext(browser_context));

  web_contents_->SyncRendererPrefs();

  // Set |this| as a delegate so the ConstrainedWebDialogUI can retrieve it.
  ConstrainedWebDialogUI::SetConstrainedDelegate(web_contents_.get(), this);

  web_contents_->GetController().LoadURL(
      web_dialog_delegate_->GetDialogContentURL(), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
}

ConstrainedWebDialogDelegateViews::~ConstrainedWebDialogDelegateViews() {
  if (web_contents_) {
    // Remove reference to |this| in the WebContent since it will becomes
    // invalid and the lifetime of the WebContent may exceed the one of this
    // object.
    ConstrainedWebDialogUI::ClearConstrainedDelegate(web_contents_.get());
  }
}

const WebDialogDelegate*
ConstrainedWebDialogDelegateViews::GetWebDialogDelegate() const {
  return web_dialog_delegate_.get();
}

WebDialogDelegate* ConstrainedWebDialogDelegateViews::GetWebDialogDelegate() {
  return web_dialog_delegate_.get();
}

void ConstrainedWebDialogDelegateViews::OnDialogCloseFromWebUI() {
  closed_via_webui_ = true;
  CloseContents(web_contents_.get());
}

bool ConstrainedWebDialogDelegateViews::closed_via_webui() const {
  return closed_via_webui_;
}

std::unique_ptr<content::WebContents>
ConstrainedWebDialogDelegateViews::ReleaseWebContents() {
  return std::move(web_contents_holder_);
}

WebContents* ConstrainedWebDialogDelegateViews::GetWebContents() {
  return web_contents_.get();
}

gfx::Size
ConstrainedWebDialogDelegateViews::GetConstrainedWebDialogMinimumSize() const {
  NOTREACHED();
}

gfx::Size
ConstrainedWebDialogDelegateViews::GetConstrainedWebDialogMaximumSize() const {
  NOTREACHED();
}

gfx::Size
ConstrainedWebDialogDelegateViews::GetConstrainedWebDialogPreferredSize()
    const {
  NOTREACHED();
}

void ConstrainedWebDialogDelegateViews::ResizeToGivenSize(
    const gfx::Size size) {
  NOTREACHED();
}

ConstrainedDialogWebView::ConstrainedDialogWebView(
    content::BrowserContext* browser_context,
    std::unique_ptr<ui::WebDialogDelegate> delegate,
    content::WebContents* web_contents,
    const gfx::Size& min_size,
    const gfx::Size& max_size)
    : views::WebView(browser_context),
      initiator_web_contents_(web_contents->GetWeakPtr()),
      popunder_preventer_(web_contents),
      impl_(std::make_unique<ConstrainedWebDialogDelegateViews>(
          browser_context,
          std::move(delegate),
          web_contents,
          this)) {
  SetModalType(ui::mojom::ModalType::kChild);
  SetWebContents(GetWebContents());
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  if (!max_size.IsEmpty()) {
    EnableSizingFromWebContents(RestrictToPlatformMinimumSize(min_size),
                                max_size);
  }
  SetProperty(views::kElementIdentifierKey, kConstrainedDialogWebViewElementId);
}
ConstrainedDialogWebView::~ConstrainedDialogWebView() {}

const ui::WebDialogDelegate* ConstrainedDialogWebView::GetWebDialogDelegate()
    const {
  return impl_->GetWebDialogDelegate();
}

ui::WebDialogDelegate* ConstrainedDialogWebView::GetWebDialogDelegate() {
  return impl_->GetWebDialogDelegate();
}

void ConstrainedDialogWebView::OnDialogCloseFromWebUI() {
  return impl_->OnDialogCloseFromWebUI();
}

std::unique_ptr<content::WebContents>
ConstrainedDialogWebView::ReleaseWebContents() {
  return impl_->ReleaseWebContents();
}

gfx::NativeWindow ConstrainedDialogWebView::GetNativeDialog() {
  return impl_->GetNativeDialog();
}

content::WebContents* ConstrainedDialogWebView::GetWebContents() {
  return impl_->GetWebContents();
}

gfx::Size ConstrainedDialogWebView::GetConstrainedWebDialogPreferredSize()
    const {
  return GetPreferredSize();
}

gfx::Size ConstrainedDialogWebView::GetConstrainedWebDialogMinimumSize() const {
  return GetMinimumSize();
}

gfx::Size ConstrainedDialogWebView::GetConstrainedWebDialogMaximumSize() const {
  return GetMaximumSize();
}

views::View* ConstrainedDialogWebView::GetInitiallyFocusedView() {
  return this;
}

void ConstrainedDialogWebView::WindowClosing() {
  if (!impl_->closed_via_webui())
    GetWebDialogDelegate()->OnDialogClosed(std::string());
}

views::Widget* ConstrainedDialogWebView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* ConstrainedDialogWebView::GetWidget() const {
  return View::GetWidget();
}

std::u16string ConstrainedDialogWebView::GetWindowTitle() const {
  return impl_->closed_via_webui() ? std::u16string()
                                   : GetWebDialogDelegate()->GetDialogTitle();
}

std::u16string ConstrainedDialogWebView::GetAccessibleWindowTitle() const {
  return impl_->closed_via_webui()
             ? std::u16string()
             : GetWebDialogDelegate()->GetAccessibleDialogTitle();
}

views::View* ConstrainedDialogWebView::GetContentsView() {
  return this;
}

std::unique_ptr<views::NonClientFrameView>
ConstrainedDialogWebView::CreateNonClientFrameView(views::Widget* widget) {
  return views::DialogDelegate::CreateDialogFrameView(widget);
}

bool ConstrainedDialogWebView::ShouldShowCloseButton() const {
  // No close button if the dialog doesn't want a title bar.
  return impl_->GetWebDialogDelegate()->ShouldShowDialogTitle();
}

bool ConstrainedDialogWebView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // Pressing ESC closes the dialog.
  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
  GetWebDialogDelegate()->OnDialogClosingFromKeyEvent();
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  return true;
}

gfx::Size ConstrainedDialogWebView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (impl_->closed_via_webui()) {
    return gfx::Size();
  }

  // If auto-resizing is enabled and the dialog has been auto-resized,
  // View::GetPreferredSize() won't try to calculate the size again, since a
  // preferred size has been set explicitly from the renderer.
  gfx::Size size = WebView::CalculatePreferredSize(available_size);
  GetWebDialogDelegate()->GetDialogSize(&size);
  return size;
}

gfx::Size ConstrainedDialogWebView::GetMinimumSize() const {
  return min_size();
}

gfx::Size ConstrainedDialogWebView::GetMaximumSize() const {
  return !max_size().IsEmpty() ? max_size() : WebView::GetMaximumSize();
}

void ConstrainedDialogWebView::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (!max_size().IsEmpty() && initiator_web_contents_) {
    content::WebContents* top_level_web_contents =
        constrained_window::GetTopLevelWebContents(
            initiator_web_contents_.get());
    if (top_level_web_contents) {
      constrained_window::ShowModalDialog(GetWidget()->GetNativeWindow(),
                                          top_level_web_contents);
    }
  }
}

}  // namespace

ConstrainedWebDialogDelegate* ShowConstrainedWebDialog(
    content::BrowserContext* browser_context,
    std::unique_ptr<ui::WebDialogDelegate> delegate,
    content::WebContents* web_contents) {
  ConstrainedDialogWebView* dialog =
      new ConstrainedDialogWebView(browser_context, std::move(delegate),
                                   web_contents, gfx::Size(), gfx::Size());
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);
  return dialog;
}

ConstrainedWebDialogDelegate* ShowConstrainedWebDialogWithAutoResize(
    content::BrowserContext* browser_context,
    std::unique_ptr<ui::WebDialogDelegate> delegate,
    content::WebContents* web_contents,
    const gfx::Size& min_size,
    const gfx::Size& max_size) {
  DCHECK(!min_size.IsEmpty());
  DCHECK(!max_size.IsEmpty());
  ConstrainedDialogWebView* dialog = new ConstrainedDialogWebView(
      browser_context, std::move(delegate), web_contents, min_size, max_size);

  // For embedded WebContents, use the embedder's WebContents for constrained
  // window.
  content::WebContents* top_level_web_contents =
      constrained_window::GetTopLevelWebContents(web_contents);
  DCHECK(top_level_web_contents);
  constrained_window::CreateWebModalDialogViews(dialog, top_level_web_contents);
  return dialog;
}
