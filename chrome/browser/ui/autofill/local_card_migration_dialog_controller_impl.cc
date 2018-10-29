// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/local_card_migration_dialog_controller_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/browser.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace autofill {

LocalCardMigrationDialogControllerImpl::LocalCardMigrationDialogControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      local_card_migration_dialog_(nullptr),
      pref_service_(
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())) {}

LocalCardMigrationDialogControllerImpl::
    ~LocalCardMigrationDialogControllerImpl() {
  if (local_card_migration_dialog_)
    local_card_migration_dialog_->CloseDialog();
}

void LocalCardMigrationDialogControllerImpl::ShowDialog(
    std::unique_ptr<base::DictionaryValue> legal_message,
    LocalCardMigrationDialog* local_card_migration_dialog,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    AutofillClient::LocalCardMigrationCallback start_migrating_cards_callback) {
  if (local_card_migration_dialog_)
    local_card_migration_dialog_->CloseDialog();

  if (!LegalMessageLine::Parse(*legal_message, &legal_message_lines_,
                               /*escape_apostrophes=*/true)) {
    AutofillMetrics::LogLocalCardMigrationDialogOfferMetric(
        AutofillMetrics::
            LOCAL_CARD_MIGRATION_DIALOG_NOT_SHOWN_INVALID_LEGAL_MESSAGE);
    return;
  }

  local_card_migration_dialog_ = local_card_migration_dialog;
  start_migrating_cards_callback_ = std::move(start_migrating_cards_callback);
  migratable_credit_cards_ = migratable_credit_cards;
  view_state_ = LocalCardMigrationDialogState::kOffered;
  local_card_migration_dialog_->ShowDialog();
  dialog_is_visible_duration_timer_ = base::ElapsedTimer();

  AutofillMetrics::LogLocalCardMigrationDialogOfferMetric(
      AutofillMetrics::LOCAL_CARD_MIGRATION_DIALOG_SHOWN);
}

LocalCardMigrationDialogState
LocalCardMigrationDialogControllerImpl::GetViewState() const {
  return view_state_;
}

const std::vector<MigratableCreditCard>&
LocalCardMigrationDialogControllerImpl::GetCardList() const {
  return migratable_credit_cards_;
}

const LegalMessageLines&
LocalCardMigrationDialogControllerImpl::GetLegalMessageLines() const {
  return legal_message_lines_;
}

void LocalCardMigrationDialogControllerImpl::OnSaveButtonClicked(
    const std::vector<std::string>& selected_cards_guids) {
  AutofillMetrics::LogLocalCardMigrationDialogUserInteractionMetric(
      dialog_is_visible_duration_timer_.Elapsed(), selected_cards_guids.size(),
      migratable_credit_cards_.size(),
      AutofillMetrics::LOCAL_CARD_MIGRATION_DIALOG_CLOSED_SAVE_BUTTON_CLICKED);

  std::move(start_migrating_cards_callback_).Run(selected_cards_guids);
}

void LocalCardMigrationDialogControllerImpl::OnCancelButtonClicked() {
  AutofillMetrics::LogLocalCardMigrationDialogUserInteractionMetric(
      dialog_is_visible_duration_timer_.Elapsed(), 0,
      migratable_credit_cards_.size(),
      AutofillMetrics::
          LOCAL_CARD_MIGRATION_DIALOG_CLOSED_CANCEL_BUTTON_CLICKED);

  prefs::SetLocalCardMigrationPromptPreviouslyCancelled(pref_service_, true);

  start_migrating_cards_callback_.Reset();
}

void LocalCardMigrationDialogControllerImpl::OnViewCardsButtonClicked() {
  // TODO(crbug.com/867194): Add metrics.
  constexpr int kPaymentsProfileUserIndex = 0;
  OpenUrl(payments::GetManageInstrumentsUrl(kPaymentsProfileUserIndex));
}

void LocalCardMigrationDialogControllerImpl::OnLegalMessageLinkClicked(
    const GURL& url) {
  OpenUrl(url);
  AutofillMetrics::LogLocalCardMigrationDialogUserInteractionMetric(
      dialog_is_visible_duration_timer_.Elapsed(), 0,
      migratable_credit_cards_.size(),
      AutofillMetrics::LOCAL_CARD_MIGRATION_DIALOG_LEGAL_MESSAGE_CLICKED);
}

void LocalCardMigrationDialogControllerImpl::OnDialogClosed() {
  if (local_card_migration_dialog_)
    local_card_migration_dialog_ = nullptr;
}

void LocalCardMigrationDialogControllerImpl::OpenUrl(const GURL& url) {
  web_contents_->OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

}  // namespace autofill
