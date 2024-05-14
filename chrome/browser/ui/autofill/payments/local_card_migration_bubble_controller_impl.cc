// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble_controller_impl.h"

#include <stddef.h>

#include "base/observer_list.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/local_card_migration_metrics.h"
#include "components/autofill/core/browser/strike_databases/payments/local_card_migration_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

LocalCardMigrationBubbleControllerImpl::LocalCardMigrationBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<LocalCardMigrationBubbleControllerImpl>(
          *web_contents) {}

LocalCardMigrationBubbleControllerImpl::
    ~LocalCardMigrationBubbleControllerImpl() = default;

void LocalCardMigrationBubbleControllerImpl::ShowBubble(
    base::OnceClosure local_card_migration_bubble_closure) {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;

  is_reshow_ = false;
  should_add_strikes_on_bubble_close_ = true;
  local_card_migration_bubble_closure_ =
      std::move(local_card_migration_bubble_closure);

  autofill_metrics::LogLocalCardMigrationBubbleOfferMetric(
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, is_reshow_);

  Show();
}

void LocalCardMigrationBubbleControllerImpl::ReshowBubble() {
  if (bubble_view())
    return;

  is_reshow_ = true;
  autofill_metrics::LogLocalCardMigrationBubbleOfferMetric(
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED, is_reshow_);

  Show();
}

void LocalCardMigrationBubbleControllerImpl::AddObserver(
    LocalCardMigrationControllerObserver* observer) {
  observer_list_.AddObserver(observer);
}

AutofillBubbleBase*
LocalCardMigrationBubbleControllerImpl::local_card_migration_bubble_view()
    const {
  return bubble_view();
}

void LocalCardMigrationBubbleControllerImpl::OnConfirmButtonClicked() {
  DCHECK(local_card_migration_bubble_closure_);
  std::move(local_card_migration_bubble_closure_).Run();
  should_add_strikes_on_bubble_close_ = false;
}

void LocalCardMigrationBubbleControllerImpl::OnCancelButtonClicked() {
  local_card_migration_bubble_closure_.Reset();
}

void LocalCardMigrationBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
  if (should_add_strikes_on_bubble_close_) {
    should_add_strikes_on_bubble_close_ = false;
    AddStrikesForBubbleClose();
  }

  // Log local card migration bubble result according to the closed reason.
  autofill_metrics::LocalCardMigrationBubbleResultMetric metric;
  switch (closed_reason) {
    case PaymentsBubbleClosedReason::kAccepted:
      metric = autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_ACCEPTED;
      break;
    case PaymentsBubbleClosedReason::kClosed:
      metric = autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_CLOSED;
      break;
    case PaymentsBubbleClosedReason::kNotInteracted:
      metric = autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED;
      break;
    case PaymentsBubbleClosedReason::kLostFocus:
      metric = autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_LOST_FOCUS;
      break;
    case PaymentsBubbleClosedReason::kUnknown:
      metric = autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_RESULT_UNKNOWN;
      break;
    case PaymentsBubbleClosedReason::kCancelled:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  autofill_metrics::LogLocalCardMigrationBubbleResultMetric(metric, is_reshow_);
}

PageActionIconType
LocalCardMigrationBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kLocalCardMigration;
}

void LocalCardMigrationBubbleControllerImpl::DoShowBubble() {
  DCHECK(local_card_migration_bubble_closure_);
  DCHECK(!bubble_view());

  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  set_bubble_view(
      browser->window()
          ->GetAutofillBubbleHandler()
          ->ShowLocalCardMigrationBubble(web_contents(), this, is_reshow_));
  DCHECK(bubble_view());

  autofill_metrics::LogLocalCardMigrationBubbleOfferMetric(
      autofill_metrics::LOCAL_CARD_MIGRATION_BUBBLE_SHOWN, is_reshow_);
}

void LocalCardMigrationBubbleControllerImpl::AddStrikesForBubbleClose() {
  LocalCardMigrationStrikeDatabase local_card_migration_strike_database(
      StrikeDatabaseFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext())));
  local_card_migration_strike_database.AddStrikes(
      LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LocalCardMigrationBubbleControllerImpl);

}  // namespace autofill
