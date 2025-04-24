// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_page_action_controller.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_usage_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

FileSystemAccessPageActionController::FileSystemAccessPageActionController(
    tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface) {
  CHECK(IsPageActionMigrated(PageActionIconType::kFileSystemAccess));
}

void FileSystemAccessPageActionController::UpdateVisibility() {
  bool has_write_access = false;
  bool show_read_indicator = false;

  content::WebContents* web_contents = tab_interface_->GetContents();
  url::Origin origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  auto* context =
      FileSystemAccessPermissionContextFactory::GetForProfileIfExists(
          web_contents->GetBrowserContext());
  if (context) {
    has_write_access = context->OriginHasWriteAccess(origin);
    show_read_indicator = context->OriginHasReadAccess(origin);
  }
  tabs::TabFeatures* tab_features = tab_interface_->GetTabFeatures();
  CHECK(tab_features);
  page_actions::PageActionController* page_action_controller =
      tab_features->page_action_controller();
  if (has_write_access || show_read_indicator) {
    if (!has_write_access) {
      page_action_controller->OverrideImage(
          kActionShowFileSystemAccess,
          ui::ImageModel::FromVectorIcon(
              vector_icons::kInsertDriveFileOutlineIcon));
      page_action_controller->OverrideText(
          kActionShowFileSystemAccess,
          l10n_util::GetStringUTF16(
              IDS_FILE_SYSTEM_ACCESS_DIRECTORY_USAGE_TOOLTIP));
      page_action_controller->OverrideTooltip(
          kActionShowFileSystemAccess,
          l10n_util::GetStringUTF16(
              IDS_FILE_SYSTEM_ACCESS_DIRECTORY_USAGE_TOOLTIP));
      page_action_controller->Show(kActionShowFileSystemAccess);
    } else {
      page_action_controller->ClearOverrideImage(kActionShowFileSystemAccess);
      page_action_controller->ClearOverrideTooltip(kActionShowFileSystemAccess);
      page_action_controller->ClearOverrideText(kActionShowFileSystemAccess);
      page_action_controller->Show(kActionShowFileSystemAccess);
    }
  } else {
    FileSystemAccessUsageBubbleView::CloseCurrentBubble();
    HideIcon();
  }
}

void FileSystemAccessPageActionController::HideIcon() {
  page_actions::PageActionController* page_action_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(page_action_controller);
  page_action_controller->Hide(kActionShowFileSystemAccess);
}
