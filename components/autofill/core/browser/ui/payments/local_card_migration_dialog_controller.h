// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "url/gurl.h"

namespace autofill {

enum class LocalCardMigrationDialogState;
class MigratableCreditCard;

// Interface that exposes controller functionality to local card migration
// dialog views.
class LocalCardMigrationDialogController {
 public:
  LocalCardMigrationDialogController() = default;

  LocalCardMigrationDialogController(
      const LocalCardMigrationDialogController&) = delete;
  LocalCardMigrationDialogController& operator=(
      const LocalCardMigrationDialogController&) = delete;

  virtual ~LocalCardMigrationDialogController() = default;

  virtual LocalCardMigrationDialogState GetViewState() const = 0;
  virtual const std::vector<MigratableCreditCard>& GetCardList() const = 0;
  virtual const LegalMessageLines& GetLegalMessageLines() const = 0;
  virtual const std::u16string& GetTipMessage() const = 0;
  virtual const std::string& GetUserEmail() const = 0;
  virtual void OnSaveButtonClicked(
      const std::vector<std::string>& selected_cards_guids) = 0;
  virtual void OnCancelButtonClicked() = 0;
  virtual void OnDoneButtonClicked() = 0;
  virtual void OnViewCardsButtonClicked() = 0;
  virtual void OnLegalMessageLinkClicked(const GURL& url) = 0;
  virtual void DeleteCard(const std::string& deleted_card_guid) = 0;
  virtual void OnDialogClosed() = 0;
  virtual bool AllCardsInvalid() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_H_
