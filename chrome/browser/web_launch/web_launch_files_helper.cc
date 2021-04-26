// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_launch/web_launch_files_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/origin.h"

namespace web_launch {

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebLaunchFilesHelper)

// static
void WebLaunchFilesHelper::SetLaunchPaths(
    content::WebContents* web_contents,
    const GURL& launch_url,
    std::vector<base::FilePath> launch_paths) {
  if (launch_paths.size() == 0)
    return;

  web_contents->SetUserData(
      UserDataKey(), std::make_unique<WebLaunchFilesHelper>(
                         web_contents, launch_url, std::move(launch_paths)));
}

// static
void WebLaunchFilesHelper::SetLaunchDirectoryAndLaunchPaths(
    content::WebContents* web_contents,
    const GURL& launch_url,
    base::FilePath launch_dir,
    std::vector<base::FilePath> launch_paths) {
  if (launch_dir.empty())
    return;

  if (launch_paths.size() == 0)
    return;

  web_contents->SetUserData(UserDataKey(),
                            std::make_unique<WebLaunchFilesHelper>(
                                web_contents, launch_url, std::move(launch_dir),
                                std::move(launch_paths)));
}

void WebLaunchFilesHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Currently, launch data is only sent for the main frame.
  if (!handle->IsInMainFrame())
    return;

  MaybeSendLaunchEntries();
}

namespace {

// On Chrome OS paths that exist on an external mount point need to be treated
// differently to make sure the File System Access code accesses these paths via
// the correct file system backend. This method checks if this is the case, and
// updates `entry_path` to the path that should be used by the File System
// Access implementation.
content::FileSystemAccessEntryFactory::PathType MaybeRemapPath(
    base::FilePath* entry_path) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath virtual_path;
  auto* external_mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  if (external_mount_points->GetVirtualPath(*entry_path, &virtual_path)) {
    *entry_path = std::move(virtual_path);
    return content::FileSystemAccessEntryFactory::PathType::kExternal;
  }
#endif
  return content::FileSystemAccessEntryFactory::PathType::kLocal;
}

class EntriesBuilder {
 public:
  EntriesBuilder(
      std::vector<blink::mojom::FileSystemAccessEntryPtr>* entries_ref,
      content::WebContents* web_contents,
      const GURL& launch_url)
      : entries_ref_(entries_ref),
        entry_factory_(web_contents->GetMainFrame()
                           ->GetProcess()
                           ->GetStoragePartition()
                           ->GetFileSystemAccessEntryFactory()),
        context_(url::Origin::Create(launch_url),
                 launch_url,
                 content::GlobalFrameRoutingId(
                     web_contents->GetMainFrame()->GetProcess()->GetID(),
                     web_contents->GetMainFrame()->GetRoutingID())) {}

  void AddFileEntry(const base::FilePath& path) {
    base::FilePath entry_path = path;
    content::FileSystemAccessEntryFactory::PathType path_type =
        MaybeRemapPath(&entry_path);
    entries_ref_->push_back(entry_factory_->CreateFileEntryFromPath(
        context_, path_type, entry_path,
        content::FileSystemAccessEntryFactory::UserAction::kOpen));
  }
  void AddDirectoryEntry(const base::FilePath& path) {
    base::FilePath entry_path = path;
    content::FileSystemAccessEntryFactory::PathType path_type =
        MaybeRemapPath(&entry_path);
    entries_ref_->push_back(entry_factory_->CreateDirectoryEntryFromPath(
        context_, path_type, entry_path,
        content::FileSystemAccessEntryFactory::UserAction::kOpen));
  }

 private:
  std::vector<blink::mojom::FileSystemAccessEntryPtr>* entries_ref_;
  scoped_refptr<content::FileSystemAccessEntryFactory> entry_factory_;
  content::FileSystemAccessEntryFactory::BindingContext context_;
};

}  // namespace

WebLaunchFilesHelper::WebLaunchFilesHelper(
    content::WebContents* web_contents,
    const GURL& launch_url,
    std::vector<base::FilePath> launch_paths)
    : content::WebContentsObserver(web_contents), launch_url_(launch_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(launch_paths.size());

  launch_entries_.reserve(launch_paths.size());

  EntriesBuilder entries_builder(&launch_entries_, web_contents, launch_url);
  for (const auto& path : launch_paths)
    entries_builder.AddFileEntry(path);

  // Asynchronously call MaybeSendLaunchEntries, since it may destroy |this|.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&WebLaunchFilesHelper::MaybeSendLaunchEntries,
                                weak_ptr_factory_.GetWeakPtr()));
}

WebLaunchFilesHelper::WebLaunchFilesHelper(
    content::WebContents* web_contents,
    const GURL& launch_url,
    base::FilePath launch_dir,
    std::vector<base::FilePath> launch_paths)
    : content::WebContentsObserver(web_contents), launch_url_(launch_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!launch_dir.empty());
  DCHECK(launch_paths.size());

  launch_entries_.reserve(launch_paths.size() + 1);

  EntriesBuilder entries_builder(&launch_entries_, web_contents, launch_url);
  entries_builder.AddDirectoryEntry(launch_dir);
  for (const auto& path : launch_paths)
    entries_builder.AddFileEntry(path);

  // Asynchronously call MaybeSendLaunchEntries, since it may destroy |this|.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&WebLaunchFilesHelper::MaybeSendLaunchEntries,
                                weak_ptr_factory_.GetWeakPtr()));
}

WebLaunchFilesHelper::~WebLaunchFilesHelper() = default;

void WebLaunchFilesHelper::MaybeSendLaunchEntries() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (launch_entries_.size() == 0)
    return;
  if (launch_url_ != web_contents()->GetLastCommittedURL())
    return;

  content::RenderFrameHost* frame = web_contents()->GetMainFrame();
  permissions::PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  permission_manager->RequestPermission(
      ContentSettingsType::FILE_HANDLING, frame, launch_url_,
      /*user_gesture=*/true,
      base::BindOnce(
          &WebLaunchFilesHelper::MaybeSendLaunchEntriesWithPermission,
          weak_ptr_factory_.GetWeakPtr()));
}

void WebLaunchFilesHelper::MaybeSendLaunchEntriesWithPermission(
    ContentSetting content_setting) {
  if (content_setting == CONTENT_SETTING_ALLOW) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    mojo::AssociatedRemote<blink::mojom::WebLaunchService> launch_service;
    web_contents()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&launch_service);
    DCHECK(launch_service);
    launch_service->SetLaunchFiles(std::move(launch_entries_));
  }
  // LaunchParams are sent, clean up.
  web_contents()->RemoveUserData(UserDataKey());
}

}  // namespace web_launch
