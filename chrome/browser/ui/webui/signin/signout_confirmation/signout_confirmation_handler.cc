// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_handler.h"

#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/barrier_callback.h"
#include "base/base64.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/extension_sync_util.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/gfx/image/image.h"
#endif

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
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_NO_UNSYNCED_DATA_BODY;
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
      return IDS_CHROME_SIGNOUT_CONFIRMATION_PROMPT_DELETE_AND_SIGNOUT_BUTTON;
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
      return IDS_PROFILES_VERIFY_ACCOUNT_BUTTON;
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
    ChromeSignoutConfirmationPromptVariant variant,
    std::vector<signout_confirmation::mojom::ExtensionInfoPtr>
        extension_infos_mojo) {
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

  signout_confirmation_mojo->has_unsynced_data =
      variant == ChromeSignoutConfirmationPromptVariant::kUnsyncedData ||
      variant ==
          ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton;
  signout_confirmation_mojo->account_extensions =
      std::move(extension_infos_mojo);
  return signout_confirmation_mojo;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
std::string GetIconUrlFromImage(const gfx::Image& image) {
  std::string base_64 = base::Base64Encode(*image.As1xPNGBytes());
  const char kDataUrlPrefix[] = "data:image/png;base64,";
  return GURL(kDataUrlPrefix + base_64).spec();
}

// Creates a placeholder icon image based on the extension's `name` and returns
// a base64 representation of it.
std::string GetPlaceholderIconUrl(const std::string& name) {
  return GetIconUrlFromImage(extensions::ExtensionIconPlaceholder::CreateImage(
      extension_misc::EXTENSION_ICON_SMALLISH, name));
}

// Called when the icon for `extension_name` is loaded. Must be called for all
// account extensions before the `SignoutConfirmationData` is sent.
signout_confirmation::mojom::ExtensionInfoPtr OnExtensionIconLoaded(
    const std::string& extension_name,
    const gfx::Image& icon) {
  auto extension_info_mojo = signout_confirmation::mojom::ExtensionInfo::New();
  extension_info_mojo->name = extension_name;
  extension_info_mojo->icon_url = icon.IsEmpty()
                                      ? GetPlaceholderIconUrl(extension_name)
                                      : GetIconUrlFromImage(icon);

  return extension_info_mojo;
}

bool HasAccountExtensions(Profile* profile) {
  if (!extensions::sync_util::IsSyncingExtensionsInTransportMode(profile)) {
    return false;
  }

  extensions::AccountExtensionTracker* tracker =
      extensions::AccountExtensionTracker::Get(profile);
  std::vector<const extensions::Extension*> account_extensions =
      tracker->GetSignedInAccountExtensions();
  return !account_extensions.empty();
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  //  namespace

SignoutConfirmationHandler::SignoutConfirmationHandler(
    mojo::PendingReceiver<signout_confirmation::mojom::PageHandler> receiver,
    mojo::PendingRemote<signout_confirmation::mojom::Page> page,
    Browser* browser,
    ChromeSignoutConfirmationPromptVariant variant,
    SignoutConfirmationCallback callback)
    : browser_(browser ? browser->AsWeakPtr() : nullptr),
      variant_(variant),
      completion_callback_(std::move(callback)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ComputeAccountExtensions();
#else
  ComputeAndSendSignoutConfirmationDataWithoutExtensions();
#endif
}

SignoutConfirmationHandler::~SignoutConfirmationHandler() = default;

void SignoutConfirmationHandler::UpdateViewHeight(uint32_t height) {
  if (browser_) {
    browser_->signin_view_controller()->SetModalSigninHeight(height);
  }
}

void SignoutConfirmationHandler::Accept(bool uninstall_account_extensions) {
  FinishAndCloseDialog(ChromeSignoutConfirmationChoice::kSignout,
                       uninstall_account_extensions);
}

void SignoutConfirmationHandler::Cancel(bool uninstall_account_extensions) {
  ChromeSignoutConfirmationChoice cancel_choice =
      (variant_ ==
       ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton)
          ? ChromeSignoutConfirmationChoice::kCancelSignoutAndReauth
          : ChromeSignoutConfirmationChoice::kCancelSignout;
  FinishAndCloseDialog(cancel_choice, uninstall_account_extensions);
}

void SignoutConfirmationHandler::Close() {
  FinishAndCloseDialog(ChromeSignoutConfirmationChoice::kCancelSignout,
                       /*uninstall_account_extensions=*/false);
}

void SignoutConfirmationHandler::FinishAndCloseDialog(
    ChromeSignoutConfirmationChoice choice,
    bool uninstall_account_extensions) {
  RecordChromeSignoutConfirmationPromptMetrics(variant_, choice);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (browser_ && HasAccountExtensions(browser_->profile())) {
    RecordAccountExtensionsSignoutChoice(choice, !uninstall_account_extensions);
  }
#endif

  std::move(completion_callback_).Run(choice, uninstall_account_extensions);
  if (browser_) {
    browser_->signin_view_controller()->CloseModalSignin();
  }
}

void SignoutConfirmationHandler::
    ComputeAndSendSignoutConfirmationDataWithoutExtensions() {
  ComputeAndSendSignoutConfirmationData({});
}

void SignoutConfirmationHandler::ComputeAndSendSignoutConfirmationData(
    std::vector<signout_confirmation::mojom::ExtensionInfoPtr>
        account_extensions_info) {
  page_->SendSignoutConfirmationData(ConstructSignoutConfirmationData(
      variant_, std::move(account_extensions_info)));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void SignoutConfirmationHandler::ComputeAccountExtensions() {
  if (!browser_ || !extensions::sync_util::IsSyncingExtensionsInTransportMode(
                       browser_->profile())) {
    ComputeAndSendSignoutConfirmationDataWithoutExtensions();
    return;
  }

  extensions::AccountExtensionTracker* tracker =
      extensions::AccountExtensionTracker::Get(browser_->profile());
  std::vector<const extensions::Extension*> account_extensions =
      tracker->GetSignedInAccountExtensions();
  if (account_extensions.empty()) {
    ComputeAndSendSignoutConfirmationDataWithoutExtensions();
    return;
  }

  auto barrier_callback = base::BarrierCallback<
      signout_confirmation::mojom::ExtensionInfoPtr>(
      account_extensions.size(),
      base::BindOnce(
          &SignoutConfirmationHandler::ComputeAndSendSignoutConfirmationData,
          weak_ptr_factory_.GetWeakPtr()));

  const int icon_size = extension_misc::EXTENSION_ICON_SMALLISH;
  auto* image_loader = extensions::ImageLoader::Get(browser_->profile());

  for (const extensions::Extension* extension : account_extensions) {
    extensions::ExtensionResource icon = extensions::IconsInfo::GetIconResource(
        extension, icon_size, ExtensionIconSet::Match::kBigger);
    if (icon.empty()) {
      barrier_callback.Run(
          ::OnExtensionIconLoaded(extension->name(), gfx::Image()));
    } else {
      gfx::Size max_size(icon_size, icon_size);
      image_loader->LoadImageAsync(
          extension, icon, max_size,
          base::BindOnce(&::OnExtensionIconLoaded, extension->name())
              .Then(barrier_callback));
    }
  }
}
#endif
