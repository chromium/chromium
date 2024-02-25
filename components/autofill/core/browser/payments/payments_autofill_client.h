// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"

namespace autofill {

class MigratableCreditCard;

namespace payments {

// A payments-specific client interface that handles dependency injection, and
// its implementations serve as the integration for platform-specific code. One
// per WebContents, owned by the AutofillClient. Created lazily in the
// AutofillClient when it is needed.
class PaymentsAutofillClient : public RiskDataLoader {
 public:
  ~PaymentsAutofillClient() override;

  // Callback to run if user presses the Save button in the migration dialog.
  // Will pass a vector of GUIDs of cards that the user selected to upload to
  // LocalCardMigrationManager.
  using LocalCardMigrationCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;

  // Callback to run if the user presses the trash can button in the
  // action-required dialog. Will pass to LocalCardMigrationManager a
  // string of GUID of the card that the user selected to delete from local
  // storage.
  using MigrationDeleteCardCallback =
      base::RepeatingCallback<void(const std::string&)>;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Runs `show_migration_dialog_closure` if the user accepts the card migration
  // offer. This causes the card migration dialog to be shown.
  virtual void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure);

  // Shows a dialog with the given `legal_message_lines` and the `user_email`.
  // Runs `start_migrating_cards_callback` if the user would like the selected
  // cards in the `migratable_credit_cards` to be uploaded to cloud.
  virtual void ConfirmMigrateLocalCardToCloud(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback);

  // Will show a dialog containing a error message if `has_server_error`
  // is true, or the migration results for cards in
  // `migratable_credit_cards` otherwise. If migration succeeds the dialog will
  // contain a `tip_message`. `migratable_credit_cards` will be used when
  // constructing the dialog. The dialog is invoked when the migration process
  // is finished. Runs `delete_local_card_callback` if the user chose to delete
  // one invalid card from local storage.
  virtual void ShowLocalCardMigrationResults(
      bool has_server_error,
      const std::u16string& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrationDeleteCardCallback delete_local_card_callback);

  // Called after virtual card enrollment is finished. Shows enrollment
  // result to users. `is_vcn_enrolled` indicates if the card was successfully
  // enrolled as a virtual card.
  virtual void VirtualCardEnrollCompleted(bool is_vcn_enrolled);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Called after credit card upload is finished. Will show upload result to
  // users. `card_saved` indicates if the card is successfully saved.
  // TODO(crbug.com/932818): This function is overridden in iOS codebase and in
  // the desktop codebase. If iOS is not using it to do anything, please keep
  // this function for desktop.
  virtual void CreditCardUploadCompleted(bool card_saved);
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
