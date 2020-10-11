// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_access_icon_view.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"
#include "chrome/browser/native_file_system/native_file_system_permission_context_factory.h"
#include "chrome/browser/ui/views/native_file_system/native_file_system_usage_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

NativeFileSystemAccessIconView::NativeFileSystemAccessIconView(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate) {
  SetVisible(false);
}

views::BubbleDialogDelegate* NativeFileSystemAccessIconView::GetBubble() const {
  return NativeFileSystemUsageBubbleView::GetBubble();
}

void NativeFileSystemAccessIconView::UpdateImpl() {
  const bool had_write_access = has_write_access_;
  bool show_read_indicator = false;

  if (!GetWebContents()) {
    has_write_access_ = false;
  } else {
    url::Origin origin =
        GetWebContents()->GetMainFrame()->GetLastCommittedOrigin();
    auto* context =
        NativeFileSystemPermissionContextFactory::GetForProfileIfExists(
            GetWebContents()->GetBrowserContext());
    has_write_access_ = context && context->OriginHasWriteAccess(origin);
    show_read_indicator = context && context->OriginHasReadAccess(origin);
  }

  SetVisible(has_write_access_ || show_read_indicator);

  if (has_write_access_ != had_write_access)
    UpdateIconImage();

  // If icon isn't visible, a bubble shouldn't be shown either. Close it if
  // it was still open.
  if (!GetVisible())
    NativeFileSystemUsageBubbleView::CloseCurrentBubble();
}

base::string16
NativeFileSystemAccessIconView::GetTextForTooltipAndAccessibleName() const {
  return has_write_access_
             ? l10n_util::GetStringUTF16(
                   IDS_NATIVE_FILE_SYSTEM_WRITE_USAGE_TOOLTIP)
             : l10n_util::GetStringUTF16(
                   IDS_NATIVE_FILE_SYSTEM_DIRECTORY_USAGE_TOOLTIP);
}

void NativeFileSystemAccessIconView::OnExecuting(ExecuteSource execute_source) {
  auto* web_contents = GetWebContents();
  url::Origin origin = web_contents->GetMainFrame()->GetLastCommittedOrigin();

  auto* context =
      NativeFileSystemPermissionContextFactory::GetForProfileIfExists(
          web_contents->GetBrowserContext());
  if (!context) {
    // If there is no context, there can't be any usage either, so just return.
    return;
  }

  ChromeNativeFileSystemPermissionContext::Grants grants =
      context->GetPermissionGrants(origin);

  NativeFileSystemUsageBubbleView::Usage usage;
  usage.readable_files = std::move(grants.file_read_grants);
  usage.readable_directories = std::move(grants.directory_read_grants);
  usage.writable_files = std::move(grants.file_write_grants);
  usage.writable_directories = std::move(grants.directory_write_grants);

  NativeFileSystemUsageBubbleView::ShowBubble(web_contents, origin,
                                              std::move(usage));
}

const gfx::VectorIcon& NativeFileSystemAccessIconView::GetVectorIcon() const {
  return has_write_access_ ? vector_icons::kSaveOriginalFileIcon
                           : vector_icons::kInsertDriveFileOutlineIcon;
}

const char* NativeFileSystemAccessIconView::GetClassName() const {
  return "NativeFileSystemAccessIconView";
}
