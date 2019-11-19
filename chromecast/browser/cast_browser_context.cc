// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_context.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/browser/cast_download_manager_delegate.h"
#include "chromecast/browser/cast_permission_manager.h"
#include "chromecast/browser/url_request_context_factory.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cors_origin_pattern_setter.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"

namespace chromecast {
namespace shell {

namespace {
const void* const kDownloadManagerDelegateKey = &kDownloadManagerDelegateKey;
}  // namespace

using content::CorsOriginPatternSetter;

class CastBrowserContext::CastResourceContext
    : public content::ResourceContext {
 public:
  CastResourceContext() {}
  ~CastResourceContext() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CastResourceContext);
};

CastBrowserContext::CastBrowserContext()
    : resource_context_(new CastResourceContext),
      shared_cors_origin_access_list_(
          content::SharedCorsOriginAccessList::Create()) {
  InitWhileIOAllowed();
  simple_factory_key_ =
      std::make_unique<SimpleFactoryKey>(GetPath(), IsOffTheRecord());
  SimpleKeyMap::GetInstance()->Associate(this, simple_factory_key_.get());
}

CastBrowserContext::~CastBrowserContext() {
  SimpleKeyMap::GetInstance()->Dissociate(this);
  BrowserContext::NotifyWillBeDestroyed(this);
  ShutdownStoragePartitions();
  base::DeleteSoon(FROM_HERE, {content::BrowserThread::IO},
                   resource_context_.release());
}

void CastBrowserContext::InitWhileIOAllowed() {
#if defined(OS_ANDROID)
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
#endif  // defined(OS_ANDROID)
  BrowserContext::Initialize(this, path_);
}

#if !defined(OS_ANDROID)
std::unique_ptr<content::ZoomLevelDelegate>
CastBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}
#endif  // !defined(OS_ANDROID)

base::FilePath CastBrowserContext::GetPath() {
  return path_;
}

bool CastBrowserContext::IsOffTheRecord() {
  return false;
}

content::ResourceContext* CastBrowserContext::GetResourceContext() {
  return resource_context_.get();
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

void CastBrowserContext::SetCorsOriginAccessListForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  auto barrier_closure = BarrierClosure(2, std::move(closure));

  // Keep profile storage partitions' NetworkContexts synchronized.
  auto profile_setter = base::MakeRefCounted<CorsOriginPatternSetter>(
      source_origin, CorsOriginPatternSetter::ClonePatterns(allow_patterns),
      CorsOriginPatternSetter::ClonePatterns(block_patterns), barrier_closure);
  ForEachStoragePartition(
      this, base::BindRepeating(&CorsOriginPatternSetter::SetLists,
                                base::RetainedRef(profile_setter.get())));

  // Keep the per-profile access list up to date so that we can use this to
  // restore NetworkContext settings at anytime, e.g. on restarting the
  // network service.
  shared_cors_origin_access_list_->SetForOrigin(
      source_origin, std::move(allow_patterns), std::move(block_patterns),
      barrier_closure);
}

content::SharedCorsOriginAccessList*
CastBrowserContext::GetSharedCorsOriginAccessList() {
  return shared_cors_origin_access_list_.get();
}

}  // namespace shell
}  // namespace chromecast
