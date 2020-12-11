// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/webid_signin_window.h"

#include "chrome/browser/ui/webid/identity_dialog_controller.h"
#include "chrome/browser/ui/webid/identity_dialogs.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_delegate.h"

// Dimensions of the dialog itself.
constexpr int kDialogMinWidth = 512;
constexpr int kDialogHeight = 450;
// Dimension of the header.
constexpr int kHeaderHeight = 75;

class ModalDialog : public views::DialogDelegateView {
 public:
  ModalDialog(content::WebContents* contents, const GURL& provider)
      : initiator_web_contents_(contents), web_view_(nullptr) {
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
    SetModalType(ui::MODAL_TYPE_CHILD);
    SetLayoutManager(std::make_unique<views::FillLayout>());

    web_view_ = AddChildView(CreateWebView(provider));
    SetInitiallyFocusedView(web_view_);
  }

  std::unique_ptr<views::WebView> CreateWebView(const GURL& provider) {
    auto web_view = std::make_unique<views::WebView>(
        initiator_web_contents_->GetBrowserContext());

    web_view->LoadInitialURL(provider);

    // The webview must get an explicitly set height otherwise the layout
    // doesn't make it fill its container. This is likely because it has no
    // content at the time of first layout (nothing has loaded yet). Because of
    // this, set it to. total_dialog_height - header_height. On the other hand,
    // the width will be properly set so it can be 0 here.
    web_view->SetPreferredSize(
        {kDialogMinWidth, kDialogHeight - kHeaderHeight});

    return web_view;
  }

  void Show() {
    // ShowWebModalDialogViews takes ownership of this, by way of the
    // DeleteDelegate method.
    constrained_window::ShowWebModalDialogViews(this, initiator_web_contents_);
  }

 private:
  content::WebContents* initiator_web_contents_;
  // The contents of the dialog, owned by the view hierarchy.
  views::WebView* web_view_;
};

WebIDSigninWindow::WebIDSigninWindow(
    content::WebContents* initiator_web_contents,
    const GURL& provider,
    base::OnceCallback<void(std::string)> on_done,
    base::OnceCallback<void()> on_close)
    : on_done_(std::move(on_done)) {
  auto* modal = new ModalDialog(initiator_web_contents, provider);

  modal->SetCloseCallback(std::move(on_close));
  // TODO(majidvp): Actually call on_done callback once we have a token.

  // ModalDialog is a WidgetDelegate, owned by its views::Widget. It is
  // destroyed by `DeleteDelegate()` which is invoked by view hierarchy. Once
  // modal is deleted we should delete the window class as well.
  modal->RegisterDeleteDelegateCallback(
      base::BindOnce([](WebIDSigninWindow* window) { delete window; },
                     base::Unretained(this)));

  modal->Show();
}

WebIDSigninWindow::~WebIDSigninWindow() = default;

void ShowWebIDSigninWindow(content::WebContents* initiator_web_contents,
                           const GURL& provider,
                           base::OnceCallback<void(std::string)> on_done,
                           base::OnceCallback<void()> on_close) {
  new WebIDSigninWindow(initiator_web_contents, provider, std::move(on_done),
                        std::move(on_close));
}
