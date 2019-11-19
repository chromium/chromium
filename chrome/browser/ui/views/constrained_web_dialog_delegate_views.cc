// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace {

class ConstrainedWebDialogDelegateViews;

// WebContentsObserver that tracks the lifetime of the WebContents to avoid
// potential use after destruction.
class InitiatorWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit InitiatorWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InitiatorWebContentsObserver);
};

gfx::Size RestrictToPlatformMinimumSize(const gfx::Size& min_size) {
#if defined(OS_MACOSX)
  // http://crbug.com/78973 - MacOS does not handle zero-sized windows well.
  gfx::Size adjusted_min_size(1, 1);
  adjusted_min_size.SetToMax(min_size);
  return adjusted_min_size;
#else
  return min_size;
#endif
}

// The specialized WebView that lives in a constrained dialog.
class ConstrainedDialogWebView : public views::WebView,
                                 public ConstrainedWebDialogDelegate,
                                 public views::WidgetDelegate {
 public:
  ConstrainedDialogWebView(content::BrowserContext* browser_context,
                           std::unique_ptr<ui::WebDialogDelegate> delegate,
                           content::WebContents* web_contents,
                           const gfx::Size& min_size,
                           const gfx::Size& max_size);
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
  base::string16 GetWindowTitle() const override;
  base::string16 GetAccessibleWindowTitle() const override;
  views::View* GetContentsView() override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool ShouldShowCloseButton() const override;
  ui::ModalType GetModalType() const override;

  // views::WebView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void DocumentOnLoadCompletedInMainFrame() override;

 private:
  InitiatorWebContentsObserver initiator_observer_;

  std::unique_ptr<ConstrainedWebDialogDelegateViews> impl_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedDialogWebView);
};

class WebDialogWebContentsDelegateViews
    : public ui::WebDialogWebContentsDelegate {
 public:
  WebDialogWebContentsDelegateViews(content::BrowserContext* browser_context,
                                    InitiatorWebContentsObserver* observer,
                                    ConstrainedDialogWebView* web_view)
      : ui::WebDialogWebContentsDelegate(
            browser_context,
            std::make_unique<ChromeWebContentsHandler>()),
        initiator_observer_(observer),
        web_view_(web_view) {}
  ~WebDialogWebContentsDelegateViews() override = default;

  // ui::WebDialogWebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    // Forward shortcut keys in dialog to our initiator's delegate.
    // http://crbug.com/104586
    // Disabled on Mac due to http://crbug.com/112173
#if !defined(OS_MACOSX)
    if (!initiator_observer_->web_contents())
      return false;

    auto* delegate = initiator_observer_->web_contents()->GetDelegate();
    if (!delegate)
      return false;
    return delegate->HandleKeyboardEvent(initiator_observer_->web_contents(),
                                         event);
#else
    return false;
#endif
  }

  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override {
    if (source != web_view_->GetWebContents())
      return;

    if (!initiator_observer_->web_contents())
      return;

    // views::WebView is only a delegate for a WebContents it creates itself via
    // views::WebView::GetWebContents(). ConstrainedDialogWebView's constructor
    // sets its own WebContents (via an override of WebView::GetWebContents()).
    // So forward this notification to views::WebView.
    web_view_->ResizeDueToAutoResize(source, new_size);

    content::WebContents* top_level_web_contents =
        constrained_window::GetTopLevelWebContents(
            initiator_observer_->web_contents());
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
  InitiatorWebContentsObserver* const initiator_observer_;
  ConstrainedDialogWebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(WebDialogWebContentsDelegateViews);
};

class ConstrainedWebDialogDelegateViews
    : public ConstrainedWebDialogDelegateBase {
 public:
  ConstrainedWebDialogDelegateViews(
      content::BrowserContext* context,
      std::unique_ptr<ui::WebDialogDelegate> delegate,
      InitiatorWebContentsObserver* observer,
      ConstrainedDialogWebView* view)
      : ConstrainedWebDialogDelegateBase(
            context,
            std::move(delegate),
            std::make_unique<WebDialogWebContentsDelegateViews>(context,
                                                                observer,
                                                                view)),
        view_(view) {
    chrome::RecordDialogCreation(chrome::DialogIdentifier::CONSTRAINED_WEB);
  }

  ~ConstrainedWebDialogDelegateViews() override = default;

  // ui::WebDialogWebContentsDelegate:
  void CloseContents(content::WebContents* source) override {
    view_->GetWidget()->Close();
  }

  // contents::WebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, view_->GetFocusManager());
  }

  // ConstrainedWebDialogDelegate:
  gfx::NativeWindow GetNativeDialog() override {
    return view_->GetWidget()->GetNativeWindow();
  }

 private:
  // Converts keyboard events on the WebContents to accelerators.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  views::WebView* view_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViews);
};

ConstrainedDialogWebView::ConstrainedDialogWebView(
    content::BrowserContext* browser_context,
    std::unique_ptr<ui::WebDialogDelegate> delegate,
    content::WebContents* web_contents,
    const gfx::Size& min_size,
    const gfx::Size& max_size)
    : views::WebView(browser_context),
      initiator_observer_(web_contents),
      impl_(std::make_unique<ConstrainedWebDialogDelegateViews>(
          browser_context,
          std::move(delegate),
          &initiator_observer_,
          this)) {
  SetWebContents(GetWebContents());
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  if (!max_size.IsEmpty()) {
    EnableSizingFromWebContents(RestrictToPlatformMinimumSize(min_size),
                                max_size);
  }
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

base::string16 ConstrainedDialogWebView::GetWindowTitle() const {
  return impl_->closed_via_webui() ? base::string16()
                                   : GetWebDialogDelegate()->GetDialogTitle();
}

base::string16 ConstrainedDialogWebView::GetAccessibleWindowTitle() const {
  return impl_->closed_via_webui()
             ? base::string16()
             : GetWebDialogDelegate()->GetAccessibleDialogTitle();
}

views::View* ConstrainedDialogWebView::GetContentsView() {
  return this;
}

views::NonClientFrameView* ConstrainedDialogWebView::CreateNonClientFrameView(
    views::Widget* widget) {
  return views::DialogDelegate::CreateDialogFrameView(widget);
}

bool ConstrainedDialogWebView::ShouldShowCloseButton() const {
  // No close button if the dialog doesn't want a title bar.
  return impl_->GetWebDialogDelegate()->ShouldShowDialogTitle();
}

ui::ModalType ConstrainedDialogWebView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool ConstrainedDialogWebView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // Pressing ESC closes the dialog.
  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
  GetWebDialogDelegate()->OnDialogClosingFromKeyEvent();
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  return true;
}

gfx::Size ConstrainedDialogWebView::CalculatePreferredSize() const {
  if (impl_->closed_via_webui())
    return gfx::Size();

  // If auto-resizing is enabled and the dialog has been auto-resized,
  // View::GetPreferredSize() won't try to calculate the size again, since a
  // preferred size has been set explicitly from the renderer.
  gfx::Size size = WebView::CalculatePreferredSize();
  GetWebDialogDelegate()->GetDialogSize(&size);
  return size;
}

gfx::Size ConstrainedDialogWebView::GetMinimumSize() const {
  return min_size();
}

gfx::Size ConstrainedDialogWebView::GetMaximumSize() const {
  return !max_size().IsEmpty() ? max_size() : WebView::GetMaximumSize();
}

void ConstrainedDialogWebView::DocumentOnLoadCompletedInMainFrame() {
  if (!max_size().IsEmpty() && initiator_observer_.web_contents()) {
    content::WebContents* top_level_web_contents =
        constrained_window::GetTopLevelWebContents(
            initiator_observer_.web_contents());
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
