// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/one_time_tokens/gmail_otp_opt_in_bubble_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/views/autofill/one_time_tokens/gmail_otp_opt_in_bubble_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_result.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace autofill {

namespace {

AutofillBubbleBase* CreateGmailOtpOptInBubbleView(
    content::WebContents* web_contents,
    GmailOtpOptInBubbleController* controller) {
  if (!web_contents) {
    return nullptr;
  }
  ToolbarButtonProvider* toolbar_button_provider = ToolbarButtonProvider::From(
      controller->tab().GetBrowserWindowInterface());
  if (!toolbar_button_provider) {
    return nullptr;
  }
  views::BubbleAnchor anchor =
      toolbar_button_provider->GetBubbleAnchor(std::nullopt);
  auto bubble_view = std::make_unique<GmailOtpOptInBubbleView>(
      anchor, web_contents, controller->account_email());
  GmailOtpOptInBubbleView* const ptr = bubble_view.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  ptr->ShowForReason(LocationBarBubbleDelegateView::AUTOMATIC);
  return ptr;
}

}  // namespace

DEFINE_USER_DATA(GmailOtpOptInBubbleController);

GmailOtpOptInBubbleController::GmailOtpOptInBubbleController(
    tabs::TabInterface& tab)
    : tab_(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this),
      bubble_view_factory_(
          base::BindRepeating(&CreateGmailOtpOptInBubbleView)) {}

GmailOtpOptInBubbleController::~GmailOtpOptInBubbleController() {
  if (IsShowingBubble()) {
    bubble_view_->Hide();
    bubble_view_ = nullptr;
  }
  if (callback_) {
    std::move(callback_).Run(GmailOtpOptInResult::kDiscarded);
  }
}

// static
GmailOtpOptInBubbleController* GmailOtpOptInBubbleController::From(
    tabs::TabInterface& tab) {
  return Get(tab.GetUnownedUserDataHost());
}

void GmailOtpOptInBubbleController::SetUpAndShowBubble(
    const std::u16string& account_email,
    ResultCallback callback) {
  BubbleManager* manager = BubbleManager::GetForTab(&tab_.get());
  if (!manager || IsShowingBubble() ||
      manager->HasPendingBubbleOfSameType(GetBubbleType())) {
    if (callback) {
      std::move(callback).Run(GmailOtpOptInResult::kDiscarded);
    }
    return;
  }
  account_email_ = account_email;
  callback_ = std::move(callback);
  manager->RequestShowController(*this, /*force_show=*/false);
}

void GmailOtpOptInBubbleController::SetBubbleViewFactoryForTesting(
    BubbleViewFactory factory) {
  bubble_view_factory_ = std::move(factory);
}

void GmailOtpOptInBubbleController::ShowBubble() {
  SetBubbleView(bubble_view_factory_.Run(web_contents(), this));
}

void GmailOtpOptInBubbleController::HideBubble(
    bool initiated_by_bubble_manager) {
  if (IsShowingBubble()) {
    bubble_view_->Hide();
    ResetBubbleViewAndInformBubbleManager(initiated_by_bubble_manager);
  }
}

void GmailOtpOptInBubbleController::OnBubbleDiscarded() {
  if (callback_) {
    std::move(callback_).Run(GmailOtpOptInResult::kDiscarded);
  }
}

BubbleType GmailOtpOptInBubbleController::GetBubbleType() const {
  return BubbleType::kGmailOtpOptIn;
}

bool GmailOtpOptInBubbleController::IsShowingBubble() const {
  return bubble_view_ != nullptr;
}

bool GmailOtpOptInBubbleController::IsMouseHovered() const {
  return IsShowingBubble() && bubble_view_->IsMouseHovered();
}

bool GmailOtpOptInBubbleController::CanBeReshown() const {
  return true;
}

bool GmailOtpOptInBubbleController::ShouldReshowOnTabVisible() const {
  // Preserve the opt-in bubble in the queue when the tab is hidden (e.g. if the
  // user switches tabs or clicks the "Learn more" link to open settings in a
  // new tab) so it reappears when returning to the tab.
  return true;
}

base::WeakPtr<BubbleControllerBase>
GmailOtpOptInBubbleController::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GmailOtpOptInBubbleController::SetBubbleView(
    AutofillBubbleBase* bubble_view) {
  bubble_view_ = bubble_view;
}

void GmailOtpOptInBubbleController::ResetBubbleViewAndInformBubbleManager(
    bool initiated_by_bubble_manager) {
  const bool was_showing = IsShowingBubble();
  bubble_view_ = nullptr;
  if (was_showing && !initiated_by_bubble_manager) {
    if (BubbleManager* manager = BubbleManager::GetForTab(&tab_.get())) {
      manager->OnBubbleHiddenByController(*this, /*show_next_bubble=*/true);
    }
  }
}

content::WebContents* GmailOtpOptInBubbleController::web_contents() const {
  return tab_->GetContents();
}

}  // namespace autofill
