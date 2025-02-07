// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_handler.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

int ComputeDialogTitleId(ChromeSignoutConfirmationPromptVariant variant) {
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_NO_UNSYNCED_TITLE;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_TITLE;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_TITLE;
    case ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls:
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_NO_UNSYNCED_TITLE;
    default:
      NOTREACHED();
  }
}

int ComputeDialogSubtitleId(ChromeSignoutConfirmationPromptVariant variant) {
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_NO_UNSYNCED_BODY;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_UNSYNCED_BODY;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_VERIFY_BODY;
    case ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls:
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_KIDS_BODY;
    default:
      NOTREACHED();
  }
}

int ComputeAcceptButtonLabelId(ChromeSignoutConfirmationPromptVariant variant) {
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
      return IDS_SCREEN_LOCK_SIGN_OUT;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_DELETE_AND_SIGNOUT_BUTTON;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      return IDS_PROFILES_VERIFY_ACCOUNT_BUTTON;
    case ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls:
      return IDS_SCREEN_LOCK_SIGN_OUT;
    default:
      NOTREACHED();
  }
}

int ComputeCancelButtonLabelId(ChromeSignoutConfirmationPromptVariant variant) {
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
      return IDS_CANCEL;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      return IDS_CANCEL;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_SIGNOUT_BUTTON;
    case ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls:
      return IDS_CANCEL;
    default:
      NOTREACHED();
  }
}

// Constructs the initial data to be sent over to page. Currently, this only
// consists of strings based on the prompt `variant`.
signout_confirmation::mojom::SignoutConfirmationDataPtr
ConstructSignoutConfirmationData(
    ChromeSignoutConfirmationPromptVariant variant) {
  signout_confirmation::mojom::SignoutConfirmationDataPtr
      signout_confirmation_mojo =
          signout_confirmation::mojom::SignoutConfirmationData::New();
  signout_confirmation_mojo->dialog_title =
      l10n_util::GetStringUTF8(ComputeDialogTitleId(variant));
  signout_confirmation_mojo->dialog_subtitle =
      l10n_util::GetStringUTF8(ComputeDialogSubtitleId(variant));
  signout_confirmation_mojo->accept_button_label =
      l10n_util::GetStringUTF8(ComputeAcceptButtonLabelId(variant));
  signout_confirmation_mojo->cancel_button_label =
      l10n_util::GetStringUTF8(ComputeCancelButtonLabelId(variant));
  return signout_confirmation_mojo;
}

}  //  namespace

SignoutConfirmationHandler::SignoutConfirmationHandler(
    mojo::PendingReceiver<signout_confirmation::mojom::PageHandler> receiver,
    mojo::PendingRemote<signout_confirmation::mojom::Page> page,
    Browser* browser,
    ChromeSignoutConfirmationPromptVariant variant,
    base::OnceCallback<void(ChromeSignoutConfirmationChoice)> callback)
    : browser_(browser ? browser->AsWeakPtr() : nullptr),
      variant_(variant),
      completion_callback_(std::move(callback)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  // Send any necessary data to the page.
  page_->SendSignoutConfirmationData(ConstructSignoutConfirmationData(variant));
}

SignoutConfirmationHandler::~SignoutConfirmationHandler() = default;

void SignoutConfirmationHandler::UpdateViewHeight(uint32_t height) {
  if (browser_) {
    browser_->signin_view_controller()->SetModalSigninHeight(height);
  }
}

void SignoutConfirmationHandler::Accept() {
  ChromeSignoutConfirmationChoice ok_choice =
      (variant_ ==
       ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton)
          ? ChromeSignoutConfirmationChoice::kCancelSignoutAndReauth
          : ChromeSignoutConfirmationChoice::kSignout;

  FinishAndCloseDialog(ok_choice);
}

void SignoutConfirmationHandler::Cancel() {
  ChromeSignoutConfirmationChoice cancel_choice =
      (variant_ ==
       ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton)
          ? ChromeSignoutConfirmationChoice::kSignout
          : ChromeSignoutConfirmationChoice::kCancelSignout;
  FinishAndCloseDialog(cancel_choice);
}

void SignoutConfirmationHandler::Close() {
  FinishAndCloseDialog(ChromeSignoutConfirmationChoice::kCancelSignout);
}

void SignoutConfirmationHandler::FinishAndCloseDialog(
    ChromeSignoutConfirmationChoice choice) {
  RecordChromeSignoutConfirmationPromptMetrics(variant_, choice);
  std::move(completion_callback_).Run(choice);
  if (browser_) {
    browser_->signin_view_controller()->CloseModalSignin();
  }
}
