// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_restore_permission_bubble_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/file_system_access/file_system_access_ui_helpers.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_scroll_panel.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

FileSystemAccessRestorePermissionBubbleView::
    FileSystemAccessRestorePermissionBubbleView(
        const std::u16string window_title,
        const std::vector<
            FileSystemAccessPermissionRequestManager::FileRequestData>&
            file_data,
        base::OnceCallback<void(permissions::PermissionAction)> callback,
        views::View* anchor_view,
        content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      window_title_(window_title),
      callback_(std::move(callback)) {
  // Initial set up.
  views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));
  set_close_on_deactivate(false);
  set_fixed_width(layout_provider->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/619429, permission prompts should not be accepted
  // as the default action.
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCloseOnMainFrameOriginNavigation(true);
  DialogDelegate::SetCloseCallback(base::BindOnce(
      &FileSystemAccessRestorePermissionBubbleView::OnPromptDismissed,
      base::Unretained(this)));

  // Add subtitle.
  SetSubtitle(l10n_util::GetStringUTF16(
      IDS_FILE_SYSTEM_ACCESS_RESTORE_PERMISSION_DESCRIPTION));

  // Add file/directory list.
  std::vector<base::FilePath> file_paths;
  for (auto file : file_data) {
    file_paths.push_back(file.path_info.path);
  }
  AddChildView(FileSystemAccessScrollPanel::Create(file_paths));

  // Add buttons.
  auto allow_once_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &FileSystemAccessRestorePermissionBubbleView::OnButtonPressed,
          base::Unretained(this), RestorePermissionButton::kAllowOnce),
      l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME));
  allow_once_button->SetID(
      static_cast<int>(RestorePermissionButton::kAllowOnce));

  auto allow_always_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &FileSystemAccessRestorePermissionBubbleView::OnButtonPressed,
          base::Unretained(this), RestorePermissionButton::kAllowAlways),
      l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_EVERY_VISIT));
  allow_always_button->SetID(
      static_cast<int>(RestorePermissionButton::kAllowAlways));

  auto deny_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &FileSystemAccessRestorePermissionBubbleView::OnButtonPressed,
          base::Unretained(this), RestorePermissionButton::kDeny),
      l10n_util::GetStringUTF16(IDS_PERMISSION_DONT_ALLOW));
  deny_button->SetID(static_cast<int>(RestorePermissionButton::kDeny));

  allow_once_button->SetStyle(ui::ButtonStyle::kTonal);
  allow_always_button->SetStyle(ui::ButtonStyle::kTonal);
  deny_button->SetStyle(ui::ButtonStyle::kTonal);

  auto buttons_container = std::make_unique<views::View>();
  buttons_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      DISTANCE_BUTTON_VERTICAL));
  buttons_container->AddChildView(std::move(allow_once_button));
  buttons_container->AddChildView(std::move(allow_always_button));
  buttons_container->AddChildView(std::move(deny_button));
  buttons_container->SetPreferredSize(gfx::Size(
      layout_provider->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
          layout_provider->GetInsetsMetric(views::INSETS_DIALOG_BUTTON_ROW)
              .width(),
      buttons_container->GetPreferredSize().height()));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetExtraView(std::move(buttons_container));
}

FileSystemAccessRestorePermissionBubbleView::
    ~FileSystemAccessRestorePermissionBubbleView() = default;

// static
FileSystemAccessRestorePermissionBubbleView*
FileSystemAccessRestorePermissionBubbleView::CreateAndShow(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction)> callback,
    content::WebContents* web_contents) {
  DCHECK(request.request_type == FileSystemAccessPermissionRequestManager::
                                     RequestType::kRestorePermissions);

  auto* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser || !browser->window()) {
    return nullptr;
  }

  Profile* profile =
      web_contents
          ? Profile::FromBrowserContext(web_contents->GetBrowserContext())
          : nullptr;
  std::u16string window_title = l10n_util::GetStringFUTF16(
      IDS_PERMISSIONS_BUBBLE_PROMPT,
      file_system_access_ui_helper::GetUrlIdentityName(
          profile, request.origin.GetURL()));
  auto* bubble_view = new FileSystemAccessRestorePermissionBubbleView(
      window_title, request.file_request_data, std::move(callback),
      bubble_anchor_util::GetPageInfoAnchorConfiguration(browser).anchor_view,
      web_contents);
  bubble_view->UpdateAnchor(browser);
  views::BubbleDialogDelegateView::CreateBubble(bubble_view);
  bubble_view->ShowForReason(
      FileSystemAccessRestorePermissionBubbleView::AUTOMATIC);
  return bubble_view;
}

void FileSystemAccessRestorePermissionBubbleView::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(CreateTitleOriginLabel(GetWindowTitle()));
}

bool FileSystemAccessRestorePermissionBubbleView::ShouldShowCloseButton()
    const {
  return true;
}

std::u16string FileSystemAccessRestorePermissionBubbleView::GetWindowTitle()
    const {
  return window_title_;
}

void FileSystemAccessRestorePermissionBubbleView::UpdateAnchor(
    Browser* browser) {
  auto configuration =
      bubble_anchor_util::GetPageInfoAnchorConfiguration(browser);
  SetAnchorView(configuration.anchor_view);
  SetHighlightedButton(configuration.highlighted_button);
  if (!configuration.anchor_view) {
    SetAnchorRect(bubble_anchor_util::GetPageInfoAnchorRect(browser));
  }
  SetArrow(configuration.bubble_arrow);
}

void FileSystemAccessRestorePermissionBubbleView::OnButtonPressed(
    RestorePermissionButton button) {
  if (!callback_) {
    return;
  }

  switch (button) {
    case RestorePermissionButton::kAllowOnce:
      AcceptDialog();
      std::move(callback_).Run(permissions::PermissionAction::GRANTED_ONCE);
      return;
    case RestorePermissionButton::kAllowAlways:
      AcceptDialog();
      std::move(callback_).Run(permissions::PermissionAction::GRANTED);
      return;
    case RestorePermissionButton::kDeny:
      CancelDialog();
      std::move(callback_).Run(permissions::PermissionAction::DENIED);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void FileSystemAccessRestorePermissionBubbleView::OnPromptDismissed() {
  if (!callback_) {
    return;
  }

  std::move(callback_).Run(permissions::PermissionAction::DISMISSED);
}

void ShowFileSystemAccessRestorePermissionDialog(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction)> callback,
    content::WebContents* web_contents) {
  FileSystemAccessRestorePermissionBubbleView::CreateAndShow(
      request, std::move(callback), web_contents);
}

FileSystemAccessRestorePermissionBubbleView*
GetFileSystemAccessRestorePermissionDialogForTesting(  // IN-TEST
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction)> callback,
    content::WebContents* web_contents) {
  return FileSystemAccessRestorePermissionBubbleView::CreateAndShow(
      request, std::move(callback), web_contents);
}

BEGIN_METADATA(FileSystemAccessRestorePermissionBubbleView)
END_METADATA
