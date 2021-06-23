// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/webid_dialog_views.h"

#include <memory>
#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/ui/views/webid/webid_permission_view.h"
#include "chrome/browser/ui/views/webid/webid_signin_page_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/common/content_features.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

// Dimensions of the dialog itself.
constexpr int kDialogMinWidth = 512;
constexpr int kDialogHeight = 450;

WebIdDialogViews::WebIdDialogViews(content::WebContents* rp_web_contents)
    : WebIdDialogViews(rp_web_contents, nullptr) {
  // https://crbug.com/1195781: It appears that there are crashes in this
  // file without the WebID flag enabled, which should be impossible.
  if (!base::FeatureList::IsEnabled(features::kWebID)) {
    base::debug::DumpWithoutCrashing();
  }
}

WebIdDialogViews::WebIdDialogViews(content::WebContents* rp_web_contents,
                                   gfx::NativeView parent)
    : WebIdDialog(rp_web_contents) {
  // WebIdDialogViews is a WidgetDelegate, owned by its views::Widget. It
  // is destroyed by `DeleteDelegate()` which is invoked by view
  // hierarchy. The below check ensures this is true.
  DCHECK(owned_by_widget());
  set_parent_window(parent);
}

WebIdDialogViews::~WebIdDialogViews() = default;

void WebIdDialogViews::ShowInitialPermission(const std::u16string& idp_hostname,
                                             const std::u16string& rp_hostname,
                                             PermissionCallback callback) {
  state_ = State::kInitialPermission;
  auto content_view = WebIdPermissionView::CreateForInitialPermission(
      this, idp_hostname, rp_hostname);
  permission_callback_ = std::move(callback);
  SetContent(std::move(content_view));
  ShowDialog();
}

void WebIdDialogViews::ShowTokenExchangePermission(
    const std::u16string& idp_hostname,
    const std::u16string& rp_hostname,
    PermissionCallback callback) {
  state_ = State::kTokenExchangePermission;
  auto content_view = WebIdPermissionView::CreateForTokenExchangePermission(
      this, idp_hostname, rp_hostname);
  permission_callback_ = std::move(callback);
  SetContent(std::move(content_view));
  ShowDialog();
}

void WebIdDialogViews::ShowSigninPage(content::WebContents* idp_web_contents,
                                      const GURL& idp_signin_url,
                                      CloseCallback on_close) {
  DCHECK(rp_web_contents());
  state_ = State::kSignIn;

  // TODO(majidvp): What happens if we are handling multiple concurrent WebId
  // requests? At the moment we keep creating modal dialogs. This may be fine
  // when these requests belong to different tabs but may break down if they
  // are from the same tab or even share the same |initiator_web_contents|
  // (e.g., two requests made from an iframe and its embedder frame). We need
  // to investigate this to ensure we are providing appropriate UX.
  // http://crbug.com/1141125
  auto content_view = std::make_unique<SigninPageView>(
      this, rp_web_contents(), idp_web_contents, idp_signin_url);

  close_callback_ = std::move(on_close);
  SetContent(std::move(content_view));
  ShowDialog();
}

void WebIdDialogViews::CloseSigninPage() {
  DCHECK_EQ(state_, State::kSignIn);
  std::move(close_callback_).Run();
  // Note that this does not close the dialog as we may want to show the token
  // exchange permission still.
}

void WebIdDialogViews::ShowDialog() {
  if (dialog_) {
    dialog_->Show();
    return;
  }

  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(true);
  set_margins(gfx::Insets());

  auto width =
      views::LayoutProvider::Get()->GetSnappedDialogWidth(kDialogMinWidth);
  set_fixed_width(width);
  SetPreferredSize({width, kDialogHeight});

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  SetCloseCallback(
      base::BindOnce(&WebIdDialogViews::OnClose, base::Unretained(this)));

  if (parent_window()) {
    // To make testing easier we use the parent window if provided instead of
    // showing the dialog with web contents as parent.
    dialog_ = CreateBubble(this);
    dialog_->Show();
  } else {
    // ShowWebModalDialogViews takes ownership of this, by way of the
    // DeleteDelegate method.
    dialog_ =
        constrained_window::ShowWebModalDialogViews(this, rp_web_contents());
  }
}

void WebIdDialogViews::SetContent(std::unique_ptr<views::View> content) {
  // TODO(majidvp): Animate the switch between old and new content views.
  if (content_)
    RemoveChildViewT(content_);

  content_ = AddChildView(std::move(content));
}

void WebIdDialogViews::OnClose() {
  switch (state_) {
    case State::kInitialPermission:
    case State::kTokenExchangePermission:
      if (permission_callback_) {
        // The dialog has closed without the user expressing an explicit
        // preference. The current permission request should be denied.
        std::move(permission_callback_).Run(UserApproval::kDenied);
      }
      break;
    case State::kSignIn:
      if (close_callback_) {
        // The IDP page has closed without the user completing the flow.
        std::move(close_callback_).Run();
      }
      break;
    case State::kUninitialized:
      break;
  }
}

bool WebIdDialogViews::Accept() {
  std::move(permission_callback_).Run(UserApproval::kApproved);
  // Accepting only closes the dialog once we are at token exchange state.
  return state_ == State::kTokenExchangePermission;
}

bool WebIdDialogViews::Cancel() {
  std::move(permission_callback_).Run(UserApproval::kDenied);
  // Cancelling always closes the dialog.
  return true;
}

BEGIN_METADATA(WebIdDialogViews, views::BubbleDialogDelegateView)
END_METADATA

// static
WebIdDialog* WebIdDialog::Create(content::WebContents* rp_web_contents) {
  return new WebIdDialogViews(rp_web_contents);
}
