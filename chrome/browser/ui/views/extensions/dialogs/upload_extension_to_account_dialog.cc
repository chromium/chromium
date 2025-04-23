// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/layout_provider.h"

namespace extensions {

void ShowUploadExtensionToAccountDialog(Browser* browser,
                                        const Extension& extension,
                                        base::OnceClosure accept_callback,
                                        base::OnceClosure cancel_callback) {
  CHECK(base::FeatureList::IsEnabled(
      switches::kEnableExtensionsExplicitBrowserSignin));
  CHECK(AccountExtensionTracker::Get(browser->profile())
            ->CanUploadAsAccountExtension(extension));

  auto split_cancel_callback =
      base::SplitOnceCallback(std::move(cancel_callback));

  ui::DialogModel::Builder builder;
  builder.SetInternalName("UploadExtensionToAccountDialog")
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_UPLOAD_MOVE_TO_ACCOUNT_DIALOG_TITLE))
      .OverrideShowCloseButton(false)
      .SetSubtitle(l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_MOVE_TO_ACCOUNT_DIALOG_SUBTITLE,
          util::GetFixupExtensionNameForUIDisplay(extension.name())))
      .AddOkButton(
          std::move(accept_callback),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_MOVE_TO_ACCOUNT_DIALOG_OK_BUTTON_LABEL)))
      .AddCancelButton(std::move(split_cancel_callback.first))
      .SetCloseActionCallback(std::move(split_cancel_callback.second));

  // Show the avatar and email of the signed-in account as a custom view
  // between the dialog's subtitle and buttons.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          browser->profile()->GetOriginalProfile());
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  CHECK(!account_info.IsEmpty());

  auto avatar_and_email_view = std::make_unique<views::View>();
  avatar_and_email_view->AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
          account_info.account_image, 16, 16, profiles::SHAPE_CIRCLE))));
  avatar_and_email_view->AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(account_info.email)));
  int horizontal_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACCOUNT_INFO_ROW_AVATAR_EMAIL);
  avatar_and_email_view->SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->set_between_child_spacing(horizontal_spacing);
  builder.AddCustomField(
      std::make_unique<views::BubbleDialogModelHost::CustomView>(
          std::move(avatar_and_email_view),
          views::BubbleDialogModelHost::FieldType::kControl));

  // Show a browser modal instead of one attached to the extensions icon to be
  // consistent with upload dialogs for other syncable types (e.g. bookmarks).
  chrome::ShowBrowserModal(browser, builder.Build());
}

}  // namespace extensions
