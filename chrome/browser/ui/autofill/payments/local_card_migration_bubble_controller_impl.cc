// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble_controller_impl.h"

#include <stddef.h>

#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/payments/local_card_migration_strike_database.h"
#include "components/autofill/core/browser/payments/strike_database.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// TODO(crbug.com/862405): Build a base class for this
// and SaveCardBubbleControllerImpl.
LocalCardMigrationBubbleControllerImpl::LocalCardMigrationBubbleControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      local_card_migration_bubble_(nullptr) {}

LocalCardMigrationBubbleControllerImpl::
    ~LocalCardMigrationBubbleControllerImpl() {
  if (local_card_migration_bubble_)
    HideBubble();
}

void LocalCardMigrationBubbleControllerImpl::ShowBubble(
    base::OnceClosure local_card_migration_bubble_closure) {
  // Don't show the bubble if it's already visible.
  if (local_card_migration_bubble_)
    return;

  is_reshow_ = false;
  should_add_strikes_on_bubble_close_ = true;
  local_card_migration_bubble_closure_ =
      std::move(local_card_migration_bubble_closure);

  AutofillMetrics::LogLocalCardMigrationBubbleOfferMetric(
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, is_reshow_);

  ShowBubbleImplementation();
}

void LocalCardMigrationBubbleControllerImpl::HideBubble() {
  if (local_card_migration_bubble_) {
    local_card_migration_bubble_->Hide();
    local_card_migration_bubble_ = nullptr;
  }
}

void LocalCardMigrationBubbleControllerImpl::ReshowBubble() {
  if (local_card_migration_bubble_)
    return;

  is_reshow_ = true;
  AutofillMetrics::LogLocalCardMigrationBubbleOfferMetric(
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, is_reshow_);

  ShowBubbleImplementation();
}

void LocalCardMigrationBubbleControllerImpl::AddObserver(
    LocalCardMigrationControllerObserver* observer) {
  observer_list_.AddObserver(observer);
}

LocalCardMigrationBubble*
LocalCardMigrationBubbleControllerImpl::local_card_migration_bubble_view()
    const {
  return local_card_migration_bubble_;
}

void LocalCardMigrationBubbleControllerImpl::OnConfirmButtonClicked() {
  DCHECK(local_card_migration_bubble_closure_);
  std::move(local_card_migration_bubble_closure_).Run();
  should_add_strikes_on_bubble_close_ = false;

  AutofillMetrics::LogLocalCardMigrationBubbleUserInteractionMetric(
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_ACCEPTED, is_reshow_);
}

void LocalCardMigrationBubbleControllerImpl::OnCancelButtonClicked() {
  local_card_migration_bubble_closure_.Reset();

  AutofillMetrics::LogLocalCardMigrationBubbleUserInteractionMetric(
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_DENIED, is_reshow_);
}

void LocalCardMigrationBubbleControllerImpl::OnBubbleClosed() {
  local_card_migration_bubble_ = nullptr;
  UpdateLocalCardMigrationIcon();
  if (should_add_strikes_on_bubble_close_) {
    should_add_strikes_on_bubble_close_ = false;
    AddStrikesForBubbleClose();
  }
}

base::TimeDelta LocalCardMigrationBubbleControllerImpl::Elapsed() const {
  return timer_->Elapsed();
}

void LocalCardMigrationBubbleControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // Nothing to do if there's no bubble available.
  if (!local_card_migration_bubble_closure_)
    return;

  // Don't react to same-document (fragment) navigations.
  if (navigation_handle->IsSameDocument())
    return;

  // Don't do anything if a navigation occurs before a user could reasonably
  // interact with the bubble.
  if (Elapsed() < kCardBubbleSurviveNavigationTime)
    return;

  // Otherwise, get rid of the bubble and icon.
  local_card_migration_bubble_closure_.Reset();
  bool bubble_was_visible = local_card_migration_bubble_;
  for (LocalCardMigrationControllerObserver& observer : observer_list_) {
    observer.OnMigrationNoLongerAvailable();
  }
  if (bubble_was_visible) {
    local_card_migration_bubble_->Hide();
    OnBubbleClosed();
  } else {
    UpdateLocalCardMigrationIcon();
  }

  AutofillMetrics::LogLocalCardMigrationBubbleUserInteractionMetric(
      bubble_was_visible
          ? AutofillMetrics::
                LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_NAVIGATED_WHILE_SHOWING
          : AutofillMetrics::
                LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_NAVIGATED_WHILE_HIDDEN,
      is_reshow_);
}

void LocalCardMigrationBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    HideBubble();
}

void LocalCardMigrationBubbleControllerImpl::WebContentsDestroyed() {
  HideBubble();
}

void LocalCardMigrationBubbleControllerImpl::ShowBubbleImplementation() {
  DCHECK(local_card_migration_bubble_closure_);
  DCHECK(!local_card_migration_bubble_);

  // Update the visibility and toggled state of the credit card icon in either
  // Location bar or in Status Chip.
  UpdateLocalCardMigrationIcon();

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  local_card_migration_bubble_ =
      browser->window()
          ->GetAutofillBubbleHandler()
          ->ShowLocalCardMigrationBubble(web_contents(), this, is_reshow_);
  DCHECK(local_card_migration_bubble_);
  UpdateLocalCardMigrationIcon();
  timer_ = std::make_unique<base::ElapsedTimer>();

  AutofillMetrics::LogLocalCardMigrationBubbleOfferMetric(
      AutofillMetrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, is_reshow_);
}

void LocalCardMigrationBubbleControllerImpl::UpdateLocalCardMigrationIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser) {
    browser->window()->UpdatePageActionIcon(
        PageActionIconType::kLocalCardMigration);
  }
}

void LocalCardMigrationBubbleControllerImpl::AddStrikesForBubbleClose() {
  LocalCardMigrationStrikeDatabase local_card_migration_strike_database(
      StrikeDatabaseFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext())));
  local_card_migration_strike_database.AddStrikes(
      LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LocalCardMigrationBubbleControllerImpl)

}  // namespace autofill
