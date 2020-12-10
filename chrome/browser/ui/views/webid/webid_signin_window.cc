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
  ModalDialog(content::WebContents* initiator_web_contents,
              content::WebContents* idp_web_contents,
              const GURL& provider)
      : initiator_web_contents_(initiator_web_contents), web_view_(nullptr) {
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
    SetModalType(ui::MODAL_TYPE_CHILD);
    SetLayoutManager(std::make_unique<views::FillLayout>());

    web_view_ = AddChildView(CreateWebView(idp_web_contents, provider));
    SetInitiallyFocusedView(web_view_);
  }

  std::unique_ptr<views::WebView> CreateWebView(
      content::WebContents* idp_web_contents,
      const GURL& provider) {
    auto web_view = std::make_unique<views::WebView>(
        initiator_web_contents_->GetBrowserContext());

    web_view->SetWebContents(idp_web_contents);
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

  views::Widget* Show() {
    // ShowWebModalDialogViews takes ownership of this, by way of the
    // DeleteDelegate method.
    return constrained_window::ShowWebModalDialogViews(this,
                                                       initiator_web_contents_);
  }

 private:
  content::WebContents* initiator_web_contents_;
  // The contents of the dialog, owned by the view hierarchy.
  views::WebView* web_view_;
};

WebIDSigninWindow::WebIDSigninWindow(
    content::WebContents* initiator_web_contents,
    content::WebContents* idp_web_contents,
    const GURL& provider,
    IdProviderWindowClosedCallback on_done) {
  // TODO(majidvp): What happens if we are handling multiple concurrent WebID
  // requests? At the moment we keep creating modal dialogs. This may be fine
  // when these requests belong to different tabs but may break down if they are
  // from the same tab or even share the same |initiator_web_contents| (e.g.,
  // two requests made from an iframe and its embedder frame). We need to
  // investigate this to ensure we are providing appropriate UX.
  // http://crbug.com/1141125
  auto* modal =
      new ModalDialog(initiator_web_contents, idp_web_contents, provider);

  // Set close callback to also call on_done. This ensures that if user closes
  // the IDP window the caller promise is rejected accordingly.
  modal->SetCloseCallback(std::move(on_done));

  // ModalDialog is a WidgetDelegate, owned by its views::Widget. It is
  // destroyed by `DeleteDelegate()` which is invoked by view hierarchy. Once
  // modal is deleted we should delete the window class as well.
  modal->RegisterDeleteDelegateCallback(
      base::BindOnce([](WebIDSigninWindow* window) { delete window; },
                     base::Unretained(this)));

  modal_ = modal->Show();
}

void WebIDSigninWindow::Close() {
  modal_->Close();
}

WebIDSigninWindow::~WebIDSigninWindow() = default;

WebIDSigninWindow* ShowWebIDSigninWindow(
    content::WebContents* initiator_web_contents,
    content::WebContents* idp_web_contents,
    const GURL& provider,
    IdProviderWindowClosedCallback on_done) {
  return new WebIDSigninWindow(initiator_web_contents, idp_web_contents,
                               provider, std::move(on_done));
}

void CloseWebIDSigninWindow(WebIDSigninWindow* window) {
  window->Close();
}
