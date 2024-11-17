// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/sync_passphrase_dialog.h"

#include <functional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model_host.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

// Opens the sync dashboard webpage where the user may clear their sync data.
void OpenSyncDashboardAndCloseDialog(BrowserWindowInterface& browser,
                                     ui::DialogModel* model) {
  GURL sync_dashboard_url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kSyncGoogleDashboardURL),
      g_browser_process->GetApplicationLocale());
  browser.OpenGURL(sync_dashboard_url,
                   WindowOpenDisposition::NEW_FOREGROUND_TAB);
  model->host()->Close();
}

// Returns the content of the password field.
const std::u16string GetSyncPassphraseFieldText(ui::DialogModel* model) {
  return model->GetPasswordFieldByUniqueId(kSyncPassphrasePasswordFieldId)
      ->text();
}

// Callback for the password field. Disables the button when the field is
// empty.
void OnPasswordFieldChanged(ui::DialogModel* model) {
  std::u16string passphrase = GetSyncPassphraseFieldText(model);
  ui::DialogModel::Button* ok_button =
      model->GetButtonByUniqueId(kSyncPassphraseOkButtonFieldId);
  model->SetButtonEnabled(ok_button, !passphrase.empty());
}

// Callback for the password field. Invalidates the field if the passphrase is
// incorrect. `decrypt_data_callback` is piped into this function.
bool HandlePassphraseError(ui::DialogModel* model, bool passphrase_is_valid) {
  if (!passphrase_is_valid) {
    ui::DialogModelPasswordField* password_field =
        model->GetPasswordFieldByUniqueId(kSyncPassphrasePasswordFieldId);
    password_field->Invalidate();
  }
  return passphrase_is_valid;
}

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kSyncPassphrasePasswordFieldId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSyncPassphraseOkButtonFieldId);

void ShowSyncPassphraseDialog(
    Browser& browser,
    base::RepeatingCallback<bool(const std::u16string&)>
        decrypt_data_callback) {
  ui::DialogModel::Builder dialog_builder;

  // Link for the footnote.
  base::RepeatingClosure link_closure =
      base::BindRepeating(&OpenSyncDashboardAndCloseDialog, std::ref(browser),
                          dialog_builder.model());
  ui::DialogModelLabel::TextReplacement link_replacement =
      ui::DialogModelLabel::CreateLink(IDS_SYNC_PASSPHRASE_DIALOG_FOOTER_LINK,
                                       std::move(link_closure));

  // The OK button is initially disabled, as the passphrase must be non-empty.
  ui::DialogModel::Button::Params ok_button_params;
  ok_button_params.SetEnabled(false);
  ok_button_params.SetId(kSyncPassphraseOkButtonFieldId);

  // Callback for the OK button.
  // If the passphrase is correct, the dialog is closed. If it's incorrect, the
  // text field is cleared but remains open for the user to try again.
  base::RepeatingCallback<bool()> ok_callback =
      base::BindRepeating(&GetSyncPassphraseFieldText,
                          base::Unretained(dialog_builder.model()))
          .Then(decrypt_data_callback)
          .Then(base::BindRepeating(&HandlePassphraseError,
                                    base::Unretained(dialog_builder.model())));

  dialog_builder.SetInternalName("SyncPassphraseDialog")
      .SetTitle(l10n_util::GetStringUTF16(IDS_SYNC_PASSPHRASE_DIALOG_TITLE))
      .AddParagraph(ui::DialogModelLabel(IDS_SYNC_PASSPHRASE_DIALOG_BODY))
      .AddPasswordField(
          kSyncPassphrasePasswordFieldId,
          /*label=*/std::u16string(),
          /*accessible_text=*/
          l10n_util::GetStringUTF16(IDS_SYNC_PASSPHRASE_LABEL),
          l10n_util::GetStringUTF16(IDS_SETTINGS_INCORRECT_PASSPHRASE_ERROR))
      .AddOkButton(std::move(ok_callback), ok_button_params)
      .AddCancelButton(base::DoNothing())
      .SetFootnote(ui::DialogModelLabel::CreateWithReplacement(
          IDS_SYNC_PASSPHRASE_DIALOG_FOOTER, std::move(link_replacement)));

  // Listen to password field change events, to disable the OK button when the
  // passphrase is empty.
  ui::DialogModel* model = dialog_builder.model();
  auto subscription =
      model->GetPasswordFieldByUniqueId(kSyncPassphrasePasswordFieldId)
          ->AddOnFieldChangedCallback(base::BindRepeating(
              &OnPasswordFieldChanged, base::Unretained(model)));
  // Dummy callback to own the subscription.
  dialog_builder.SetDialogDestroyingCallback(
      base::BindOnce([](base::CallbackListSubscription subscription) {},
                     std::move(subscription)));

  chrome::ShowBrowserModal(&browser, dialog_builder.Build());
}

bool SyncPassphraseDialogDecryptData(syncer::SyncService* sync_service,
                                     const std::u16string& passphrase) {
  if (!sync_service || !sync_service->IsEngineInitialized()) {
    // Even though this is a failure, return true so that the dialog closes and
    // the user does not need to try again.
    return true;
  }

  syncer::SyncUserSettings* sync_user_settings =
      sync_service->GetUserSettings();

  if (!sync_user_settings->IsPassphraseRequired()) {
    return true;
  }

  if (passphrase.empty()) {
    // Empty passphrases are not allowed.
    return false;
  }

  return sync_user_settings->SetDecryptionPassphrase(
      base::UTF16ToUTF8(passphrase));
}
