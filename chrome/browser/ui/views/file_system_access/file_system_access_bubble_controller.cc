// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_bubble_controller.h"

#include <iterator>

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_usage_bubble_view.h"
namespace {
std::vector<base::FilePath> GetPaths(
    const std::vector<content::PathInfo>& path_infos) {
  std::vector<base::FilePath> result;
  std::ranges::transform(path_infos, std::back_inserter(result),
                         &content::PathInfo::path);
  return result;
}

}  // namespace

// static
void FileSystemAccessBubbleController::Show(Browser* browser) {
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  CHECK(web_contents);

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
  usage.readable_files = GetPaths(grants.file_read_grants);
  usage.readable_directories = GetPaths(grants.directory_read_grants);
  usage.writable_files = GetPaths(grants.file_write_grants);
  usage.writable_directories = GetPaths(grants.directory_write_grants);

  FileSystemAccessUsageBubbleView::ShowBubble(web_contents, origin,
                                              std::move(usage));
}
