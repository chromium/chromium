// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/move_bookmark_to_account_dialog.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

const int kAvatarSize = 16;

}  // namespace

void ShowMoveBookmarkToAccountDialog(Browser* browser) {
  // Retrieve info about the signed-in account. Use GetOriginalProfile() because
  // the dialog can be shown in incognito.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          browser->profile()->GetOriginalProfile());
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  CHECK(!account_info.IsEmpty());

  // Create "custom view", the part between the dialog's subtitle and buttons.
  // It contains the avatar and email of the signed-in account.
  // TODO(crbug.com/354896249): Include target folder in subtitle string.
  auto avatar_and_email_view = std::make_unique<views::View>();
  avatar_and_email_view->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromImage(
          profiles::GetSizedAvatarIcon(account_info.account_image, kAvatarSize,
                                       kAvatarSize, profiles::SHAPE_CIRCLE))));
  avatar_and_email_view->AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(account_info.email)));
  int horizontal_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  avatar_and_email_view->SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->set_between_child_spacing(horizontal_spacing);

  // Create the dialog and hook the "custom view" to it.
  // TODO(crbug.com/354896249): Register button callback to move bookmark(s).
  std::unique_ptr<ui::DialogModel> model =
      ui::DialogModel::Builder()
          .SetInternalName("MoveBookmarkToAccountDialog")
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_TITLE))
          .SetSubtitle(l10n_util::GetStringUTF16(
              IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_SUBTITLE))
          .AddOkButton(
              base::DoNothing(),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_OK_BUTTON_LABEL)))
          .AddCancelButton(base::DoNothing())
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(avatar_and_email_view),
                  views::BubbleDialogModelHost::FieldType::kControl))
          .Build();
  chrome::ShowBrowserModal(browser, std::move(model));
}
