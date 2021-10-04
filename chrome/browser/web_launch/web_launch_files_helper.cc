// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_launch/web_launch_files_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/web_launch/web_launch.mojom.h"
#include "url/origin.h"

namespace web_launch {

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebLaunchFilesHelper);

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
  EntriesBuilder(content::WebContents* web_contents,
                 const GURL& launch_url,
                 size_t expected_number_of_entries)
      : entry_factory_(web_contents->GetMainFrame()
                           ->GetProcess()
                           ->GetStoragePartition()
                           ->GetFileSystemAccessEntryFactory()),
        context_(blink::StorageKey(url::Origin::Create(launch_url)),
                 launch_url,
                 content::GlobalRenderFrameHostId(
                     web_contents->GetMainFrame()->GetProcess()->GetID(),
                     web_contents->GetMainFrame()->GetRoutingID())) {
    entries_.reserve(expected_number_of_entries);
  }

  void AddFileEntry(const base::FilePath& path) {
    base::FilePath entry_path = path;
    content::FileSystemAccessEntryFactory::PathType path_type =
        MaybeRemapPath(&entry_path);
    entries_.push_back(entry_factory_->CreateFileEntryFromPath(
        context_, path_type, entry_path,
        content::FileSystemAccessEntryFactory::UserAction::kOpen));
  }

  void AddDirectoryEntry(const base::FilePath& path) {
    base::FilePath entry_path = path;
    content::FileSystemAccessEntryFactory::PathType path_type =
        MaybeRemapPath(&entry_path);
    entries_.push_back(entry_factory_->CreateDirectoryEntryFromPath(
        context_, path_type, entry_path,
        content::FileSystemAccessEntryFactory::UserAction::kOpen));
  }

  std::vector<blink::mojom::FileSystemAccessEntryPtr> Build() {
    return std::move(entries_);
  }

 private:
  std::vector<blink::mojom::FileSystemAccessEntryPtr> entries_;
  scoped_refptr<content::FileSystemAccessEntryFactory> entry_factory_;
  content::FileSystemAccessEntryFactory::BindingContext context_;
};

}  // namespace

WebLaunchFilesHelper::~WebLaunchFilesHelper() = default;

// static
WebLaunchFilesHelper* WebLaunchFilesHelper::GetForWebContents(
    content::WebContents* web_contents) {
  return static_cast<WebLaunchFilesHelper*>(
      web_contents->GetUserData(UserDataKey()));
}

// static
void WebLaunchFilesHelper::SetLaunchPaths(
    content::WebContents* web_contents,
    const GURL& launch_url,
    std::vector<base::FilePath> launch_paths) {
  if (launch_paths.empty())
    return;

  SetLaunchPathsIfPermitted(web_contents, launch_url, /*launch_dir=*/{},
                            std::move(launch_paths));
}

// static
void WebLaunchFilesHelper::SetLaunchDirectoryAndLaunchPaths(
    content::WebContents* web_contents,
    const GURL& launch_url,
    base::FilePath launch_dir,
    std::vector<base::FilePath> launch_paths) {
  if (launch_dir.empty() || launch_paths.empty())
    return;

  SetLaunchPathsIfPermitted(web_contents, launch_url, launch_dir,
                            std::move(launch_paths));
}

void WebLaunchFilesHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  // Currently, launch data is only sent for the main frame.
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!handle->IsInPrimaryMainFrame())
    return;

  MaybeSendLaunchEntries();
}

WebLaunchFilesHelper::WebLaunchFilesHelper(
    content::WebContents* web_contents,
    const GURL& launch_url,
    base::FilePath launch_dir,
    std::vector<base::FilePath> launch_paths)
    : content::WebContentsObserver(web_contents),
      launch_paths_(launch_paths),
      launch_dir_(launch_dir),
      launch_url_(launch_url) {
  DCHECK(launch_paths.size());
}

// static
void WebLaunchFilesHelper::SetLaunchPathsIfPermitted(
    content::WebContents* web_contents,
    const GURL& launch_url,
    base::FilePath launch_dir,
    std::vector<base::FilePath> launch_paths) {
  const bool using_settings = base::FeatureList::IsEnabled(
      features::kDesktopPWAsFileHandlingSettingsGated);

  // Don't even bother creating the object if the permission is blocked.
  if (!using_settings &&
      PermissionManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))
              ->GetPermissionStatus(ContentSettingsType::FILE_HANDLING,
                                    launch_url, launch_url)
              .content_setting == CONTENT_SETTING_BLOCK) {
    return;
  }

  auto helper = base::WrapUnique(
      new WebLaunchFilesHelper(web_contents, launch_url, std::move(launch_dir),
                               std::move(launch_paths)));
  // When using settings instead of permissions, the setting should have
  // been checked/prompt shown by this point.
  if (using_settings)
    helper->passed_permission_check_ = true;

  WebLaunchFilesHelper* helper_ptr = helper.get();
  web_contents->SetUserData(UserDataKey(), std::move(helper));
  helper_ptr->MaybeSendLaunchEntries();
}

void WebLaunchFilesHelper::MaybeSendLaunchEntries() {
  const GURL current_url =
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(web_contents());
  if (launch_url_.GetOrigin() == current_url.GetOrigin()) {
    if (!permission_was_checked_ && !passed_permission_check_) {
      permission_was_checked_ = true;
      content::RenderFrameHost* frame = web_contents()->GetMainFrame();
      permissions::PermissionManager* permission_manager =
          PermissionManagerFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
      permission_manager->RequestPermission(
          ContentSettingsType::FILE_HANDLING, frame, current_url,
          /*user_gesture=*/true,
          base::BindOnce(&WebLaunchFilesHelper::OnPermissionRequestResponse,
                         weak_ptr_factory_.GetWeakPtr()));
    } else if (passed_permission_check_) {
      // If the permission was checked and passed, and then a same-site
      // navigation (e.g. redirect) occurred, set the launch queue again.
      SendLaunchEntries();
    }
  } else if (permission_was_checked_) {
    // Delete `this` after a navigation to an ineligible URL.
    web_contents()->RemoveUserData(UserDataKey());
  }
}

void WebLaunchFilesHelper::OnPermissionRequestResponse(ContentSetting setting) {
  passed_permission_check_ = setting == CONTENT_SETTING_ALLOW;

  if (passed_permission_check_) {
    SendLaunchEntries();
  } else {
    // Close the app asynchronously since it deletes the `PermissionManager`.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&WebLaunchFilesHelper::CloseApp,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void WebLaunchFilesHelper::CloseApp() {
  web_contents()->Close();
  // `this` is deleted.
}

void WebLaunchFilesHelper::SendLaunchEntries() {
  EntriesBuilder entries_builder(web_contents(), launch_url_,
                                 launch_paths_.size() + 1);
  if (!launch_dir_.empty())
    entries_builder.AddDirectoryEntry(launch_dir_);

  for (const auto& path : launch_paths_)
    entries_builder.AddFileEntry(path);

  mojo::AssociatedRemote<blink::mojom::WebLaunchService> launch_service;
  web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &launch_service);
  DCHECK(launch_service);
  launch_service->SetLaunchFiles(entries_builder.Build());
}

}  // namespace web_launch
