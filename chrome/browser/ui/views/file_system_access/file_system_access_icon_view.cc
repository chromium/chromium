// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_icon_view.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_usage_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

FileSystemAccessIconView::FileSystemAccessIconView(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "FileSystemAccess") {
  SetVisible(false);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_FILE_SYSTEM_ACCESS_DIRECTORY_USAGE_TOOLTIP));
}

views::BubbleDialogDelegate* FileSystemAccessIconView::GetBubble() const {
  return FileSystemAccessUsageBubbleView::GetBubble();
}

void FileSystemAccessIconView::UpdateImpl() {
  const bool had_write_access = has_write_access_;
  bool show_read_indicator = false;

  if (!GetWebContents()) {
    has_write_access_ = false;
  } else {
    url::Origin origin =
        GetWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    auto* context =
        FileSystemAccessPermissionContextFactory::GetForProfileIfExists(
            GetWebContents()->GetBrowserContext());
    has_write_access_ = context && context->OriginHasWriteAccess(origin);
    show_read_indicator = context && context->OriginHasReadAccess(origin);
  }

  SetVisible(has_write_access_ || show_read_indicator);

  if (has_write_access_ != had_write_access)
    UpdateIconImage();

  GetViewAccessibility().SetName(
      has_write_access_ ? l10n_util::GetStringUTF16(
                              IDS_FILE_SYSTEM_ACCESS_WRITE_USAGE_TOOLTIP)
                        : l10n_util::GetStringUTF16(
                              IDS_FILE_SYSTEM_ACCESS_DIRECTORY_USAGE_TOOLTIP));

  // If icon isn't visible, a bubble shouldn't be shown either. Close it if
  // it was still open.
  if (!GetVisible())
    FileSystemAccessUsageBubbleView::CloseCurrentBubble();
}

void FileSystemAccessIconView::OnExecuting(ExecuteSource execute_source) {
  auto* web_contents = GetWebContents();
  url::Origin origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  auto* context =
      FileSystemAccessPermissionContextFactory::GetForProfileIfExists(
          web_contents->GetBrowserContext());
  if (!context) {
    // If there is no context, there can't be any usage either, so just return.
    return;
  }

  ChromeFileSystemAccessPermissionContext::Grants grants =
      context->ConvertObjectsToGrants(context->GetGrantedObjects(origin));

  FileSystemAccessUsageBubbleView::Usage usage;
  usage.readable_files = std::move(grants.file_read_grants);
  usage.readable_directories = std::move(grants.directory_read_grants);
  usage.writable_files = std::move(grants.file_write_grants);
  usage.writable_directories = std::move(grants.directory_write_grants);

  FileSystemAccessUsageBubbleView::ShowBubble(web_contents, origin,
                                              std::move(usage));
}

const gfx::VectorIcon& FileSystemAccessIconView::GetVectorIcon() const {
    return has_write_access_ ? kFileSaveChromeRefreshIcon
                             : vector_icons::kInsertDriveFileOutlineIcon;
}

BEGIN_METADATA(FileSystemAccessIconView)
END_METADATA
