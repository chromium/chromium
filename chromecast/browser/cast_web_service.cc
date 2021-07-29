// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/cxx20_erase.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_web_view_default.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/browser/lru_renderer_cache.h"
#include "chromecast/browser/webui/cast_webui_controller_factory.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace chromecast {

namespace {

uint32_t remove_data_mask =
    content::StoragePartition::REMOVE_DATA_MASK_APPCACHE |
    content::StoragePartition::REMOVE_DATA_MASK_COOKIES |
    content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
    content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
    content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE |
    content::StoragePartition::REMOVE_DATA_MASK_WEBSQL;

}  // namespace

CastWebService::CastWebService(content::BrowserContext* browser_context,
                               CastWebViewFactory* web_view_factory,
                               CastWindowManager* window_manager)
    : browser_context_(browser_context),
      web_view_factory_(web_view_factory),
      window_manager_(window_manager),
      overlay_renderer_cache_(
          std::make_unique<LRURendererCache>(browser_context_, 1)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DCHECK(browser_context_);
  DCHECK(web_view_factory_);
  DCHECK(task_runner_);
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

CastWebService::~CastWebService() = default;

CastWebView::Scoped CastWebService::CreateWebViewInternal(
    const CastWebView::CreateParams& create_params,
    mojom::CastWebViewParamsPtr params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto web_view =
      web_view_factory_->CreateWebView(create_params, std::move(params), this);
  CastWebView::Scoped scoped(web_view.release(), [this](CastWebView* web_view) {
    OwnerDestroyed(web_view);
  });
  return scoped;
}

void CastWebService::CreateWebView(
    mojom::CastWebViewParamsPtr params,
    mojo::PendingReceiver<mojom::CastWebContents> web_contents,
    mojo::PendingReceiver<mojom::CastContentWindow> window) {
  // TODO(b/149041392): Implement this.
  NOTIMPLEMENTED_LOG_ONCE();
}

void CastWebService::FlushDomLocalStorage() {
  browser_context_->ForEachStoragePartition(
      base::BindRepeating([](content::StoragePartition* storage_partition) {
        DVLOG(1) << "Starting DOM localStorage flush.";
        storage_partition->Flush();
      }));
}

void CastWebService::ClearLocalStorage(base::OnceClosure callback) {
  browser_context_->ForEachStoragePartition(
      base::BindRepeating(
          [](base::OnceClosure cb, content::StoragePartition* partition) {
            auto cookie_delete_filter =
                network::mojom::CookieDeletionFilter::New();
            cookie_delete_filter->session_control =
                network::mojom::CookieDeletionSessionControl::IGNORE_CONTROL;
            partition->ClearData(
                remove_data_mask,
                content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                content::StoragePartition::OriginMatcherFunction(),
                std::move(cookie_delete_filter), true /*perform_cleanup*/,
                base::Time::Min(), base::Time::Max(), std::move(cb));
          },
          base::Passed(std::move(callback))));
}

void CastWebService::RegisterWebUiClient(
    mojo::PendingRemote<mojom::WebUiClient> client,
    const std::vector<std::string>& hosts) {
  content::WebUIControllerFactory::RegisterFactory(
      new CastWebUiControllerFactory(std::move(client), hosts));
}

void CastWebService::DeleteExpiringWebViews() {
  DCHECK(!immediately_delete_webviews_);
  // We don't want to delay webview deletion after this point.
  immediately_delete_webviews_ = true;
  expiring_web_views_.clear();
}

void CastWebService::OwnerDestroyed(CastWebView* web_view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::WebContents* web_contents = web_view->web_contents();
  GURL url;
  if (web_contents) {
    url = web_contents->GetVisibleURL();
    // Suspend the MediaSession to free up media resources for the next content
    // window.
    content::MediaSession::Get(web_contents)
        ->Suspend(content::MediaSession::SuspendType::kSystem);
  }
  auto delay = web_view->shutdown_delay();
  if (delay <= base::TimeDelta() || immediately_delete_webviews_) {
    LOG(INFO) << "Immediately deleting CastWebView for " << url;
    delete web_view;
    return;
  }
  LOG(INFO) << "Deleting CastWebView for " << url << " in "
            << delay.InMilliseconds() << " milliseconds.";
  expiring_web_views_.emplace(web_view);
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastWebService::DeleteWebView, weak_ptr_, web_view),
      delay);
}

void CastWebService::DeleteWebView(CastWebView* web_view) {
  LOG(INFO) << "Deleting CastWebView.";
  base::EraseIf(expiring_web_views_,
                [web_view](const std::unique_ptr<CastWebView>& ptr) {
                  return ptr.get() == web_view;
                });
}

void CastWebService::CreateSessionWithSubstitutions(
    const std::string& session_id,
    std::vector<mojom::SubstitutableParameterPtr> params) {
  DCHECK(settings_managers_.find(session_id) == settings_managers_.end());
  auto settings_manager_it = settings_managers_.insert_or_assign(
      session_id, base::MakeRefCounted<IdentificationSettingsManager>());
  settings_manager_it.first->second->SetSubstitutableParameters(
      std::move(params));
  LOG(INFO) << "Added session: " << session_id;
}

void CastWebService::SetClientAuthForSession(
    const std::string& session_id,
    mojo::PendingRemote<mojom::ClientAuthDelegate> client_auth_delegate) {
  GetSessionManager(session_id)->SetClientAuth(std::move(client_auth_delegate));
}

void CastWebService::UpdateAppSettingsForSession(
    const std::string& session_id,
    mojom::AppSettingsPtr app_settings) {
  GetSessionManager(session_id)->UpdateAppSettings(std::move(app_settings));
}

void CastWebService::UpdateDeviceSettingsForSession(
    const std::string& session_id,
    mojom::DeviceSettingsPtr device_settings) {
  GetSessionManager(session_id)
      ->UpdateDeviceSettings(std::move(device_settings));
}

void CastWebService::UpdateSubstitutableParamValuesForSession(
    const std::string& session_id,
    std::vector<mojom::IndexValuePairPtr> updated_values) {
  GetSessionManager(session_id)
      ->UpdateSubstitutableParamValues(std::move(updated_values));
}

void CastWebService::UpdateBackgroundModeForSession(
    const std::string& session_id,
    bool background_mode) {
  GetSessionManager(session_id)->UpdateBackgroundMode(background_mode);
}

void CastWebService::OnSessionDestroyed(const std::string& session_id) {
  size_t num_erased = settings_managers_.erase(session_id);
  if (num_erased == 0U) {
    LOG(INFO) << "Successfully erased session: " << session_id;
    return;
  }
  LOG(ERROR) << "Failed to erase session: " << session_id;
}

scoped_refptr<CastURLLoaderThrottle::Delegate>
CastWebService::GetURLLoaderThrottleDelegateForSession(
    const std::string& session_id) {
  auto delegate_it = settings_managers_.find(session_id);
  if (delegate_it == settings_managers_.end()) {
    return nullptr;
  }
  return delegate_it->second;
}

IdentificationSettingsManager* CastWebService::GetSessionManager(
    const std::string& session_id) {
  auto it = settings_managers_.find(session_id);
  CHECK(it != settings_managers_.end());
  return it->second.get();
}

}  // namespace chromecast
