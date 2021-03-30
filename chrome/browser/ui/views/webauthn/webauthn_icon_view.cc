// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/webauthn_icon_view.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/webauthn/webauthn_bubble_view.h"
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/metadata/metadata_impl_macros.h"

WebAuthnIconView::WebAuthnIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_WEBAUTHN,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate) {
  SetID(VIEW_ID_WEBAUTHN_BUTTON);
}

WebAuthnIconView::~WebAuthnIconView() {
  if (webauthn_bubble_) {
    webauthn_bubble_->GetWidget()->RemoveObserver(this);
  }
}

views::BubbleDialogDelegate* WebAuthnIconView::GetBubble() const {
  return webauthn_bubble_;
}

void WebAuthnIconView::UpdateImpl() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  ChromeAuthenticatorRequestDelegate* request_delegate =
      AuthenticatorRequestScheduler::GetRequestDelegate(GetWebContents());
  if (!request_delegate) {
    // No active WebAuthn request on the current page.
    SetVisible(false);
    return;
  }

  AuthenticatorRequestDialogModel* dialog_model =
      request_delegate->dialog_model();
  if (!dialog_model) {
    // This can happen if the dialog went away but the request has not resolved
    // yet.
    SetVisible(false);
    return;
  }

  SetVisible(dialog_model->current_step() ==
             AuthenticatorRequestDialogModel::Step::kLocationBarBubble);
  if (dialog_models_.find(web_contents) == dialog_models_.end()) {
    dialog_model->AddObserver(this);
    dialog_models_.insert({web_contents, dialog_model});
    if (!dialog_model->users().empty()) {
      ExecuteCommand(EXECUTE_SOURCE_MOUSE);
    }
  }
}

void WebAuthnIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  if (webauthn_bubble_) {
    return;
  }
  content::WebContents* web_contents = GetWebContents();
  AuthenticatorRequestDialogModel* model = dialog_models_.at(web_contents);
  webauthn_bubble_ = WebAuthnBubbleView::Create(
      model->relying_party_id(), model->users(),
      base::BindOnce(&AuthenticatorRequestDialogModel::OnAccountSelected,
                     model->GetWeakPtr()),
      web_contents);
  webauthn_bubble_->GetWidget()->AddObserver(this);
}

const gfx::VectorIcon& WebAuthnIconView::GetVectorIcon() const {
  return kFingerprintIcon;
}

std::u16string WebAuthnIconView::GetTextForTooltipAndAccessibleName() const {
  // TODO(crbug.com/1179014): go through ux review and i18n this string.
  return u"Sign in with your security key";
}

void WebAuthnIconView::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, GetBubble()->GetWidget());
  SetHighlighted(false);
  webauthn_bubble_ = nullptr;
}

void WebAuthnIconView::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  auto pair = base::ranges::find_if(dialog_models_, [model](const auto& pair) {
    return pair.second == model;
  });
  DCHECK(pair != dialog_models_.end());
  if (pair->first == GetWebContents()) {
    SetVisible(false);
  }
  dialog_models_.erase(pair);
}

void WebAuthnIconView::OnStepTransition() {
  UpdateImpl();
}

BEGIN_METADATA(WebAuthnIconView, PageActionIconView)
END_METADATA
