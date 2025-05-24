// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_launch_queue_delegate_impl.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace web_app {

namespace {

// TODO(crbug.com/40169582): Consider adding an {extension, pwa} enum to
// `launch_params` instead of checking the scheme specifically for extensions?
bool IsExtensionURL(const GURL& gurl) {
  return gurl.SchemeIs(extensions::kExtensionScheme);
}

}  // namespace

LaunchQueueDelegateImpl::LaunchQueueDelegateImpl(
    const WebAppRegistrar& registrar)
    : registrar_(registrar) {}

bool LaunchQueueDelegateImpl::IsValidLaunchParams(
    const webapps::LaunchParams& launch_params) const {
  return launch_params.dir.empty() ||
         registrar_->IsSystemApp(launch_params.app_id);
}

bool LaunchQueueDelegateImpl::IsInScope(
    const webapps::LaunchParams& launch_params,
    const GURL& current_url) const {
  // webapps::LaunchQueue is used by extensions with file handlers, extensions
  // don't have a concept of scope.
  // App scope is a web app concept that is not applicable for extensions.
  // Therefore this check will be skipped when launching an extension URL.
  return IsExtensionURL(current_url) ||
         registrar_->IsUrlInAppExtendedScope(current_url, launch_params.app_id);
}

// On Chrome OS paths that exist on an external mount point need to be treated
// differently to make sure the File System Access code accesses these paths via
// the correct file system backend. This method checks if this is the case, and
// updates `entry_path` to the path that should be used by the File System
// Access implementation.
content::PathInfo LaunchQueueDelegateImpl::GetPathInfo(
    const base::FilePath& entry_path) const {
#if BUILDFLAG(IS_CHROMEOS)
  base::FilePath virtual_path;
  auto* external_mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  if (external_mount_points->GetVirtualPath(entry_path, &virtual_path)) {
    return content::PathInfo(content::PathType::kExternal,
                             std::move(virtual_path));
  }
#endif
  return content::PathInfo(entry_path);
}

}  // namespace web_app
