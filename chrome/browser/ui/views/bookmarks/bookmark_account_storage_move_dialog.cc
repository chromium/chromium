// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
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

DEFINE_ELEMENT_IDENTIFIER_VALUE(kBookmarkAccountStorageMoveDialogOkButton);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kBookmarkAccountStorageMoveDialogCancelButton);

void ShowBookmarkAccountStorageMoveDialog(
    Browser* browser,
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* target_folder,
    size_t index,
    base::OnceClosure closed_callback) {
  // Note: All keyed services are retrieved for GetOriginalProfile() because the
  // dialog can be shown in incognito.
  CHECK(browser);
  CHECK(node);
  CHECK(target_folder);
  CHECK(target_folder->is_folder());
  CHECK(!node->is_permanent_node());
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(
          browser->GetProfile()->GetOriginalProfile());
  bool is_local_node = bookmark_model->IsLocalOnlyNode(*node);
  CHECK_NE(is_local_node, bookmark_model->IsLocalOnlyNode(*target_folder));

  int title_id = is_local_node ? IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_TITLE
                               : IDS_BOOKMARKS_MOVE_TO_DEVICE_DIALOG_TITLE;
  int subtitle_id =
      is_local_node
          ? (node->is_folder()
                 ? IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_FOLDER_SUBTITLE
                 : IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_SUBTITLE)
          : (node->is_folder()
                 ? IDS_BOOKMARKS_MOVE_TO_DEVICE_DIALOG_FOLDER_SUBTITLE
                 : IDS_BOOKMARKS_MOVE_TO_DEVICE_DIALOG_SUBTITLE);
  int ok_button_id = is_local_node
                         ? IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_OK_BUTTON_LABEL
                         : IDS_BOOKMARKS_MOVE_TO_DEVICE_DIALOG_OK_BUTTON_LABEL;
  auto [ok_callback, cancel_callback] =
      base::SplitOnceCallback(std::move(closed_callback));
  ui::DialogModel::Builder builder;
  builder.SetInternalName("BookmarkAccountStorageMoveDialog")
      .SetTitle(l10n_util::GetStringUTF16(title_id))
      .SetSubtitle(
          l10n_util::GetStringFUTF16(subtitle_id, target_folder->GetTitle()))
      .AddOkButton(base::BindOnce(&bookmarks::BookmarkModel::Move,
                                  bookmark_model->AsWeakPtr(), node,
                                  target_folder, index)
                       .Then(std::move(ok_callback)),
                   ui::DialogModel::Button::Params()
                       .SetLabel(l10n_util::GetStringUTF16(ok_button_id))
                       .SetId(kBookmarkAccountStorageMoveDialogOkButton))
      .AddCancelButton(
          std::move(cancel_callback),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(IDS_CANCEL))
              .SetId(kBookmarkAccountStorageMoveDialogCancelButton));

  if (is_local_node) {
    // If moving to the account, show the avatar and email of the signed-in
    // account as a "custom view" (the part between the dialog's subtitle and
    // buttons).
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(
            browser->profile()->GetOriginalProfile());
    AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
    CHECK(!account_info.IsEmpty());
    auto avatar_and_email_view = std::make_unique<views::View>();
    avatar_and_email_view->AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
            account_info.account_image, kAvatarSize, kAvatarSize,
            profiles::SHAPE_CIRCLE))));
    avatar_and_email_view->AddChildView(
        std::make_unique<views::Label>(base::UTF8ToUTF16(account_info.email)));
    int horizontal_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_RELATED_LABEL_HORIZONTAL);
    avatar_and_email_view
        ->SetLayoutManager(std::make_unique<views::BoxLayout>())
        ->set_between_child_spacing(horizontal_spacing);
    builder.AddCustomField(
        std::make_unique<views::BubbleDialogModelHost::CustomView>(
            std::move(avatar_and_email_view),
            views::BubbleDialogModelHost::FieldType::kControl));
  }

  chrome::ShowBrowserModal(browser, builder.Build());
}
