// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"

namespace {

std::unique_ptr<ui::DialogModel>
CreateChromeSignoutConfirmationPromptDialogModel(
    ChromeSignoutConfirmationPromptVariant variant,
    base::OnceCallback<void(ChromeSignoutConfirmationChoice)> callback) {
  // Split the callback in 3: Ok, Cancel, Close.
  auto [ok_callback, temp_callback] =
      base::SplitOnceCallback(std::move(callback));
  auto [cancel_callback, close_callback] =
      base::SplitOnceCallback(std::move(temp_callback));

  // Strings and choices.
  int body_string_id = IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_UNSYNCED_BODY;
  int ok_string_id = IDS_CANCEL;
  ChromeSignoutConfirmationChoice ok_choice =
      ChromeSignoutConfirmationChoice::kDismissed;
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      body_string_id = IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_VERIFY_BODY;
      ok_string_id = IDS_PROFILES_VERIFY_ACCOUNT_BUTTON;
      ok_choice = ChromeSignoutConfirmationChoice::kReauth;
      break;
  }
  std::u16string ok_label = l10n_util::GetStringUTF16(ok_string_id);
  std::u16string cancel_label = l10n_util::GetStringUTF16(
      IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_SIGNOUT_BUTTON);
  const ChromeSignoutConfirmationChoice cancel_choice =
      ChromeSignoutConfirmationChoice::kSignout;

  // Build the dialog.
  ui::DialogModel::Builder dialog_builder;
  return dialog_builder.SetInternalName("ChromeSignoutConfirmationChoicePrompt")
      .SetTitle(l10n_util::GetStringUTF16(
          IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_TITLE))
      .AddParagraph(ui::DialogModelLabel(body_string_id))
      .AddOkButton(
          base::BindOnce(std::move(ok_callback), ok_choice),
          ui::DialogModel::Button::Params().SetLabel(std::move(ok_label)))
      .AddCancelButton(
          base::BindOnce(std::move(cancel_callback), cancel_choice),
          ui::DialogModel::Button::Params().SetLabel(std::move(cancel_label)))
      .SetCloseActionCallback(
          base::BindOnce(std::move(close_callback),
                         ChromeSignoutConfirmationChoice::kDismissed))
      .Build();
}

}  // namespace

void ShowChromeSignoutConfirmationPrompt(
    Browser& browser,
    ChromeSignoutConfirmationPromptVariant variant,
    base::OnceCallback<void(ChromeSignoutConfirmationChoice)> callback) {
  chrome::ShowBrowserModal(&browser,
                           CreateChromeSignoutConfirmationPromptDialogModel(
                               variant, std::move(callback)));
}
