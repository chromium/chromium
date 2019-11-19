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

const base::Feature kNativeFileSystemReadOnlyUsageIndicatorFeature{
    "NativeFileSystemReadOnlyUsageIndicator",
    base::FEATURE_DISABLED_BY_DEFAULT};

NativeFileSystemAccessIconView::NativeFileSystemAccessIconView(
    Delegate* delegate)
    : PageActionIconView(nullptr, 0, delegate) {
  SetVisible(false);
}

views::BubbleDialogDelegateView* NativeFileSystemAccessIconView::GetBubble()
    const {
  return NativeFileSystemUsageBubbleView::GetBubble();
}

bool NativeFileSystemAccessIconView::Update() {
  const bool was_visible = GetVisible();
  const bool had_write_access = has_write_access_;

  has_write_access_ = GetWebContents() &&
                      GetWebContents()->HasWritableNativeFileSystemHandles();

  // TODO(https://crbug.com/992158): Also take read-only files into account
  // once inconsistencies in old APIs are fixed.
  bool show_read_indicator =
      base::FeatureList::IsEnabled(
          kNativeFileSystemReadOnlyUsageIndicatorFeature) &&
      GetWebContents() &&
      GetWebContents()->HasNativeFileSystemDirectoryHandles();

  SetVisible(has_write_access_ || show_read_indicator);

  if (has_write_access_ != had_write_access)
    UpdateIconImage();

  // If icon isn't visible, a bubble shouldn't be shown either. Close it if
  // it was still open.
  if (!GetVisible())
    NativeFileSystemUsageBubbleView::CloseCurrentBubble();

  return GetVisible() != was_visible || had_write_access != has_write_access_;
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
  url::Origin origin =
      url::Origin::Create(GetWebContents()->GetLastCommittedURL());

  auto* web_contents = GetWebContents();
  auto* context =
      NativeFileSystemPermissionContextFactory::GetForProfileIfExists(
          web_contents->GetBrowserContext());
  if (!context) {
    // If there is no context, there can't be any usage either, so just return.
    return;
  }

  ChromeNativeFileSystemPermissionContext::Grants grants =
      context->GetPermissionGrants(
          origin, web_contents->GetMainFrame()->GetProcess()->GetID(),
          web_contents->GetMainFrame()->GetRoutingID());

  NativeFileSystemUsageBubbleView::Usage usage;
  if (base::FeatureList::IsEnabled(
          kNativeFileSystemReadOnlyUsageIndicatorFeature)) {
    usage.readable_directories =
        web_contents->GetNativeFileSystemDirectoryHandles();
  }
  usage.writable_files = std::move(grants.file_write_grants);
  usage.writable_directories = std::move(grants.directory_write_grants);

  NativeFileSystemUsageBubbleView::ShowBubble(web_contents, origin,
                                              std::move(usage));
}

const gfx::VectorIcon& NativeFileSystemAccessIconView::GetVectorIcon() const {
  return has_write_access_ ? kSaveOriginalFileIcon
                           : vector_icons::kInsertDriveFileOutlineIcon;
}
