// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/fill_layout.h"

int PasswordAutoSignInView::auto_signin_toast_timeout_ = 3;

PasswordAutoSignInView::~PasswordAutoSignInView() = default;

PasswordAutoSignInView::PasswordAutoSignInView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             reason,
                             /*easily_dismissable=*/false) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  const autofill::PasswordForm& form = model()->pending_password();

  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);

  set_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));

  CredentialsItemView* credential = new CredentialsItemView(
      this,
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE_MD),
      form.username_value, kButtonHoverColor, &form,
      content::BrowserContext::GetDefaultStoragePartition(model()->GetProfile())
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      STYLE_HINT, views::style::STYLE_PRIMARY);
  credential->SetEnabled(false);
  AddChildView(credential);

  // Setup the observer and maybe start the timer.
  Browser* browser = chrome::FindBrowserWithWebContents(GetWebContents());
  DCHECK(browser);

  // Sign-in dialogs opened for inactive browser windows do not auto-close on
  // MacOS. This matches existing Cocoa bubble behavior.
  // TODO(varkha): Remove the limitation as part of http://crbug/671916 .
  if (browser->window()->IsActive()) {
    timer_.Start(FROM_HERE, GetTimeout(), this,
                 &PasswordAutoSignInView::OnTimer);
  }
}

void PasswordAutoSignInView::OnWidgetActivationChanged(views::Widget* widget,
                                                       bool active) {
  if (active && !timer_.IsRunning()) {
    timer_.Start(FROM_HERE, GetTimeout(), this,
                 &PasswordAutoSignInView::OnTimer);
  }
  LocationBarBubbleDelegateView::OnWidgetActivationChanged(widget, active);
}

gfx::Size PasswordAutoSignInView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void PasswordAutoSignInView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  NOTREACHED();
}

void PasswordAutoSignInView::OnTimer() {
  model()->OnAutoSignInToastTimeout();
  CloseBubble();
}

base::TimeDelta PasswordAutoSignInView::GetTimeout() {
  return base::TimeDelta::FromSeconds(
      PasswordAutoSignInView::auto_signin_toast_timeout_);
}
