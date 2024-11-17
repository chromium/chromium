// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/layout/fill_layout.h"

int PasswordAutoSignInView::auto_signin_toast_timeout_ = 3;

PasswordAutoSignInView::~PasswordAutoSignInView() = default;

PasswordAutoSignInView::PasswordAutoSignInView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  const password_manager::PasswordForm& form = controller_.pending_password();

  SetShowCloseButton(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  set_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));

  CredentialsItemView* credential =
      AddChildView(std::make_unique<CredentialsItemView>(
          views::Button::PressedCallback(),
          l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE_MD),
          form.username_value, &form,
          GetURLLoaderForMainFrame(web_contents).get(),
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
          views::style::STYLE_HINT, views::style::STYLE_PRIMARY));
  credential->SetEnabled(false);

  // Setup the observer and maybe start the timer.
  Browser* browser = chrome::FindBrowserWithTab(GetWebContents());
  DCHECK(browser);

  // Sign-in dialogs opened for inactive browser windows do not auto-close on
  // MacOS. This matches existing Cocoa bubble behavior.
  // TODO(varkha): Remove the limitation as part of http://crbug/671916 .
  if (browser->window()->IsActive()) {
    timer_.Start(FROM_HERE, GetTimeout(), this,
                 &PasswordAutoSignInView::OnTimer);
  }
}

PasswordBubbleControllerBase* PasswordAutoSignInView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* PasswordAutoSignInView::GetController()
    const {
  return &controller_;
}

void PasswordAutoSignInView::OnWidgetActivationChanged(views::Widget* widget,
                                                       bool active) {
  if (active && !timer_.IsRunning()) {
    timer_.Start(FROM_HERE, GetTimeout(), this,
                 &PasswordAutoSignInView::OnTimer);
  }
  LocationBarBubbleDelegateView::OnWidgetActivationChanged(widget, active);
}

void PasswordAutoSignInView::OnTimer() {
  controller_.OnAutoSignInToastTimeout();
  CloseBubble();
}

base::TimeDelta PasswordAutoSignInView::GetTimeout() {
  return base::Seconds(PasswordAutoSignInView::auto_signin_toast_timeout_);
}

BEGIN_METADATA(PasswordAutoSignInView)
END_METADATA
