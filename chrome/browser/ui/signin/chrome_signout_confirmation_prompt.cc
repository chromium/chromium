// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome_signout_confirmation_prompt.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"

namespace {

constexpr char kChromeSignoutPromptHistogramBaseName[] =
    "Signin.ChromeSignoutConfirmationPrompt.";
constexpr char kChromeSignoutPromptHistogramUnsyncedReauthVariant[] =
    "UnsyncedReauth";
constexpr char kChromeSignoutPromptHistogramUnsyncedVariant[] = "Unsynced";
constexpr char kChromeSignoutPromptHistogramNoUnsyncedVariant[] = "NoUnsynced";

ChromeSignoutConfirmationChoice RecordChromeSignoutConfirmationPromptMetrics(
    ChromeSignoutConfirmationPromptVariant variant,
    ChromeSignoutConfirmationChoice choice) {
  const char* histogram_variant_name =
      kChromeSignoutPromptHistogramNoUnsyncedVariant;
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      histogram_variant_name = kChromeSignoutPromptHistogramUnsyncedVariant;
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      histogram_variant_name =
          kChromeSignoutPromptHistogramUnsyncedReauthVariant;
      break;
  }

  base::UmaHistogramEnumeration(
      base::StrCat(
          {kChromeSignoutPromptHistogramBaseName, histogram_variant_name}),
      choice);
  return choice;
}

std::unique_ptr<ui::DialogModel>
CreateChromeSignoutConfirmationPromptDialogModel(
    ChromeSignoutConfirmationPromptVariant variant,
    base::OnceCallback<void(ChromeSignoutConfirmationChoice)> callback) {
  auto callback_with_metrics =
      base::BindOnce(&RecordChromeSignoutConfirmationPromptMetrics, variant)
          .Then(std::move(callback));

  // Split the callback in 3: Ok, Cancel, Close.
  auto [ok_callback, temp_callback] =
      base::SplitOnceCallback(std::move(callback_with_metrics));
  auto [cancel_callback, close_callback] =
      base::SplitOnceCallback(std::move(temp_callback));

  // Strings and choices.
  int title_string_id = IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_TITLE;
  int body_string_id = IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_UNSYNCED_BODY;
  int ok_string_id =
      IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_DELETE_AND_SIGNOUT_BUTTON;
  int cancel_string_id = IDS_CANCEL;
  ChromeSignoutConfirmationChoice ok_choice =
      ChromeSignoutConfirmationChoice::kSignout;
  ChromeSignoutConfirmationChoice cancel_choice =
      ChromeSignoutConfirmationChoice::kCancelSignout;
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
      title_string_id =
          IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_NO_UNSYNCED_TITLE;
      body_string_id = IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_NO_UNSYNCED_BODY;
      ok_string_id = IDS_SCREEN_LOCK_SIGN_OUT;
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      body_string_id = IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_VERIFY_BODY;
      ok_string_id = IDS_PROFILES_VERIFY_ACCOUNT_BUTTON;
      cancel_string_id = IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_SIGNOUT_BUTTON;
      ok_choice = ChromeSignoutConfirmationChoice::kCancelSignoutAndReauth;
      cancel_choice = ChromeSignoutConfirmationChoice::kSignout;
      break;
  }
  std::u16string ok_label = l10n_util::GetStringUTF16(ok_string_id);
  std::u16string cancel_label = l10n_util::GetStringUTF16(cancel_string_id);

  // Build the dialog.
  ui::DialogModel::Builder dialog_builder;
  return dialog_builder.SetInternalName("ChromeSignoutConfirmationChoicePrompt")
      .SetTitle(l10n_util::GetStringUTF16(title_string_id))
      .AddParagraph(ui::DialogModelLabel(body_string_id))
      .AddOkButton(
          base::BindOnce(std::move(ok_callback), ok_choice),
          ui::DialogModel::Button::Params().SetLabel(std::move(ok_label)))
      .AddCancelButton(
          base::BindOnce(std::move(cancel_callback), cancel_choice),
          ui::DialogModel::Button::Params().SetLabel(std::move(cancel_label)))
      .SetCloseActionCallback(
          base::BindOnce(std::move(close_callback),
                         ChromeSignoutConfirmationChoice::kCancelSignout))
      .Build();
}

}  // namespace

void ShowChromeSignoutConfirmationPrompt(
    Browser& browser,
    ChromeSignoutConfirmationPromptVariant variant,
    base::OnceCallback<void(ChromeSignoutConfirmationChoice)> callback) {
  if (!switches::IsImprovedSigninUIOnDesktopEnabled() &&
      variant == ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData) {
    // This variant is not enabled. Skip the UI and sign out immediately.
    std::move(callback).Run(ChromeSignoutConfirmationChoice::kSignout);
    return;
  }
  chrome::ShowBrowserModal(&browser,
                           CreateChromeSignoutConfirmationPromptDialogModel(
                               variant, std::move(callback)));
}
