// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_context.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/browser/cast_download_manager_delegate.h"
#include "chromecast/browser/cast_permission_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"

namespace chromecast {
namespace shell {

namespace {
const void* const kDownloadManagerDelegateKey = &kDownloadManagerDelegateKey;
}  // namespace

CastBrowserContext::CastBrowserContext() {
  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kRegular);
  InitWhileIOAllowed();
  simple_factory_key_ =
      std::make_unique<SimpleFactoryKey>(GetPath(), IsOffTheRecord());
  SimpleKeyMap::GetInstance()->Associate(this, simple_factory_key_.get());
}

CastBrowserContext::~CastBrowserContext() {
  SimpleKeyMap::GetInstance()->Dissociate(this);
  NotifyWillBeDestroyed();
  ShutdownStoragePartitions();
}

void CastBrowserContext::InitWhileIOAllowed() {
#if BUILDFLAG(IS_ANDROID)
  CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path_));
  path_ = path_.Append(FILE_PATH_LITERAL("cast_shell"));

  if (!base::PathExists(path_))
    base::CreateDirectory(path_);
#else
  // Chromecast doesn't support user profiles nor does it have
  // incognito mode.  This means that all of the persistent
  // data (currently only cookies and local storage) will be
  // shared in a single location as defined here.
  CHECK(base::PathService::Get(DIR_CAST_HOME, &path_));
#endif  // BUILDFLAG(IS_ANDROID)
}

std::unique_ptr<content::ZoomLevelDelegate>
CastBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

base::FilePath CastBrowserContext::GetPath() {
  return path_;
}

bool CastBrowserContext::IsOffTheRecord() {
  return false;
}

content::DownloadManagerDelegate*
CastBrowserContext::GetDownloadManagerDelegate() {
  if (!GetUserData(kDownloadManagerDelegateKey)) {
    SetUserData(kDownloadManagerDelegateKey,
                std::make_unique<CastDownloadManagerDelegate>());
  }
  return static_cast<CastDownloadManagerDelegate*>(
      GetUserData(kDownloadManagerDelegateKey));
}

content::BrowserPluginGuestManager* CastBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* CastBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
CastBrowserContext::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService* CastBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
CastBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate* CastBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
CastBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_manager_.get())
    permission_manager_.reset(new CastPermissionManager());
  return permission_manager_.get();
}

content::ClientHintsControllerDelegate*
CastBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
CastBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
CastBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
CastBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

content::ReduceAcceptLanguageControllerDelegate*
CastBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  return nullptr;
}

}  // namespace shell
}  // namespace chromecast
