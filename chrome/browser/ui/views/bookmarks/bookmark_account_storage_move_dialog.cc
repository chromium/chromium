// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

const int kAvatarSize = 16;
const int kBookmarkTitleMaxLength = 30;

void RecordDialogMetrics(std::string_view action,
                         BookmarkAccountStorageMoveDialogType type,
                         bool is_local_node) {
  if (is_local_node) {
    base::UmaHistogramEnumeration(
        base::StrCat({"BookmarkAccountStorageMoveDialog.Upload.", action}),
        type);
  } else {
    base::UmaHistogramBoolean(
        base::StrCat({"BookmarkAccountStorageMoveDialog.Download.", action}),
        true);
  }
}

void RecordDialogAccepted(BookmarkAccountStorageMoveDialogType type,
                          bool is_local_node) {
  RecordDialogMetrics("Accepted", type, is_local_node);
}

void RecordDialogDeclined(BookmarkAccountStorageMoveDialogType type,
                          bool is_local_node) {
  RecordDialogMetrics("Declined", type, is_local_node);
}

void RecordDialogExplicitlyClosed(BookmarkAccountStorageMoveDialogType type,
                                  bool is_local_node) {
  RecordDialogMetrics("ExplicitlyClosed", type, is_local_node);
}

void RecordDialogShown(BookmarkAccountStorageMoveDialogType type,
                       bool is_local_node) {
  RecordDialogMetrics("Shown", type, is_local_node);
}

void ShowDialogOnRegularProfile(
    Browser* browser,
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* target_folder,
    size_t index,
    BookmarkAccountStorageMoveDialogType dialog_type,
    base::OnceClosure closed_callback) {
  CHECK(browser);
  Profile* profile = browser->GetProfile();
  CHECK(!profile->IsOffTheRecord());
  CHECK(node);
  CHECK(target_folder);
  CHECK(target_folder->is_folder());
  CHECK(!node->is_permanent_node());
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  bool is_local_node = bookmark_model->IsLocalOnlyNode(*node);
  CHECK_NE(is_local_node, bookmark_model->IsLocalOnlyNode(*target_folder));

  int title_id = IDS_UPLOAD_MOVE_TO_ACCOUNT_DIALOG_TITLE;
  std::u16string body_text;
  switch (dialog_type) {
    case BookmarkAccountStorageMoveDialogType::kDownloadOrUpload: {
      title_id = is_local_node ? IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_TITLE
                               : IDS_BOOKMARKS_MOVE_TO_DEVICE_DIALOG_TITLE;
      body_text = l10n_util::GetStringFUTF16(
          is_local_node
              ? (node->is_folder()
                     ? IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_FOLDER_SUBTITLE
                     : IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_SUBTITLE)
              : (node->is_folder()
                     ? IDS_BOOKMARKS_MOVE_TO_DEVICE_DIALOG_FOLDER_SUBTITLE
                     : IDS_BOOKMARKS_MOVE_TO_DEVICE_DIALOG_SUBTITLE),
          gfx::TruncateString(target_folder->GetTitle(),
                              kBookmarkTitleMaxLength, gfx::CHARACTER_BREAK));
      break;
    }
    case BookmarkAccountStorageMoveDialogType::kUpload: {
      body_text = l10n_util::GetStringFUTF16(
          IDS_BOOKMARK_UPLOAD_MOVE_TO_ACCOUNT_DIALOG_SUBTITLE,
          gfx::TruncateString(node->GetTitle(), kBookmarkTitleMaxLength,
                              gfx::CHARACTER_BREAK),
          target_folder->GetTitle());
    }
  }

  int ok_button_id = is_local_node
                         ? IDS_BOOKMARKS_MOVE_TO_ACCOUNT_DIALOG_OK_BUTTON_LABEL
                         : IDS_BOOKMARKS_MOVE_TO_DEVICE_DIALOG_OK_BUTTON_LABEL;

  auto [ok_callback, cancel_callback] =
      base::SplitOnceCallback(std::move(closed_callback));

  auto delegate = std::make_unique<BookmarkAccountStorageMoveDialogDelegate>(
      browser, node, target_folder);
  ui::DialogModel::Builder builder(std::move(delegate));
  builder.SetInternalName("BookmarkAccountStorageMoveDialog")
      .SetTitle(l10n_util::GetStringUTF16(title_id))
      .AddParagraph(ui::DialogModelLabel(body_text))
      .AddOkButton(base::BindOnce(&bookmarks::BookmarkModel::Move,
                                  bookmark_model->AsWeakPtr(), node,
                                  target_folder, index)
                       .Then(std::move(ok_callback))
                       .Then(base::BindOnce(RecordDialogAccepted, dialog_type,
                                            is_local_node)),
                   ui::DialogModel::Button::Params()
                       .SetLabel(l10n_util::GetStringUTF16(ok_button_id))
                       .SetId(kBookmarkAccountStorageMoveDialogOkButton))
      .AddCancelButton(
          std::move(cancel_callback)
              .Then(base::BindOnce(RecordDialogDeclined, dialog_type,
                                   is_local_node)),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(IDS_CANCEL))
              .SetId(kBookmarkAccountStorageMoveDialogCancelButton))
      .SetCloseActionCallback(base::BindOnce(RecordDialogExplicitlyClosed,
                                             dialog_type, is_local_node));

  if (is_local_node) {
    // If moving to the account, show the avatar and email of the signed-in
    // account as a "custom view" (the part between the dialog's subtitle and
    // buttons).
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
    CHECK(!account_info.IsEmpty());

    auto avatar_and_email_view = std::make_unique<views::View>();
    int horizontal_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_ACCOUNT_INFO_ROW_AVATAR_EMAIL);

    avatar_and_email_view->AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
            account_info.account_image, kAvatarSize, kAvatarSize,
            profiles::SHAPE_CIRCLE))));
    avatar_and_email_view->AddChildView(
        std::make_unique<views::Label>(base::UTF8ToUTF16(account_info.email)));
    avatar_and_email_view
        ->SetLayoutManager(std::make_unique<views::BoxLayout>())
        ->set_between_child_spacing(horizontal_spacing);

    builder.AddCustomField(
        std::make_unique<views::BubbleDialogModelHost::CustomView>(
            std::move(avatar_and_email_view),
            views::BubbleDialogModelHost::FieldType::kControl));
  }

  chrome::ShowBrowserModal(browser, builder.Build());
  RecordDialogShown(dialog_type, is_local_node);
}

// Redirect Incognito to the Original Profile, and show the dialog over the
// Bookmarks Manager page.
void OpenDialogInOriginalProfileBookmarksManager(
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* target_folder,
    size_t index,
    BookmarkAccountStorageMoveDialogType dialog_type,
    base::OnceClosure closed_callback,
    Browser* browser) {
  // `browser` may be null in case of failure to instantiate a window.
  if (!browser) {
    return;
  }

  CHECK(!browser->GetProfile()->IsOffTheRecord());
  // Open BookmarksManager page.
  browser->OpenURL(content::OpenURLParams(
                       GURL(chrome::kChromeUIBookmarksURL), content::Referrer(),
                       WindowOpenDisposition::NEW_FOREGROUND_TAB,
                       ui::PAGE_TRANSITION_LINK, false),
                   /*navigation_handle_callback=*/{});

  ShowDialogOnRegularProfile(browser, node, target_folder, index, dialog_type,
                             std::move(closed_callback));
}

void ShowDialog(Browser* browser,
                const bookmarks::BookmarkNode* node,
                const bookmarks::BookmarkNode* target_folder,
                size_t index,
                BookmarkAccountStorageMoveDialogType dialog_type,
                base::OnceClosure closed_callback) {
  CHECK(browser);
  if (browser->GetProfile()->IsOffTheRecord()) {
    // If we cannot open an original profile window, just ignore the request. If
    // we do not do this, a new empty incognito browser opens as result of
    // `profiles::OpenBrowserWindowForProfile()`, which is confusing, so it is
    // better to exit early.
    if (IncognitoModePrefs::GetAvailability(
            browser->GetProfile()->GetPrefs()) ==
        policy::IncognitoModeAvailability::kForced) {
      return;
    }
    base::OnceCallback<void(Browser*)> on_browser_ready = base::BindOnce(
        &OpenDialogInOriginalProfileBookmarksManager, node, target_folder,
        index, dialog_type, std::move(closed_callback));
    profiles::OpenBrowserWindowForProfile(
        std::move(on_browser_ready), /*always_create=*/false,
        /*is_new_profile=*/false, /*open_command_line_urls=*/false,
        browser->GetProfile()->GetOriginalProfile());
    return;
  }

  ShowDialogOnRegularProfile(browser, node, target_folder, index, dialog_type,
                             std::move(closed_callback));
}

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kBookmarkAccountStorageMoveDialogOkButton);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kBookmarkAccountStorageMoveDialogCancelButton);

void ShowBookmarkAccountStorageMoveDialog(
    Browser* browser,
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* target_folder,
    size_t index,
    base::OnceClosure closed_callback) {
  ShowDialog(browser, node, target_folder, index,
             BookmarkAccountStorageMoveDialogType::kDownloadOrUpload,
             std::move(closed_callback));
}

void ShowBookmarkAccountStorageUploadDialog(Browser* browser,
                                            const bookmarks::BookmarkNode* node,
                                            base::OnceClosure closed_callback) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  const bookmarks::BookmarkPermanentNode* target = nullptr;
  if (node->HasAncestor(model->other_node())) {
    target = model->account_other_node();
  } else if (node->HasAncestor(model->bookmark_bar_node())) {
    target = model->account_bookmark_bar_node();
  } else if (node->HasAncestor(model->mobile_node())) {
    target = model->account_mobile_node();
  }
  CHECK(target);
  ShowDialog(browser, node, target, target->children().size(),
             BookmarkAccountStorageMoveDialogType::kUpload,
             std::move(closed_callback));
}
