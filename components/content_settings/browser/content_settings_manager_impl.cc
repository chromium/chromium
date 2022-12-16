// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/content_settings_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/cookies/site_for_cookies.h"

using content_settings::PageSpecificContentSettings;

namespace content_settings {
namespace {
using StorageType = mojom::ContentSettingsManager::StorageType;

void OnStorageAccessed(int process_id,
                       int frame_id,
                       const GURL& origin_url,
                       const GURL& top_origin_url,
                       bool blocked_by_policy,
                       page_load_metrics::StorageType storage_type) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(process_id, frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  page_load_metrics::MetricsWebContentsObserver* metrics_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (metrics_observer) {
    metrics_observer->OnStorageAccessed(render_frame_host, origin_url,
                                        top_origin_url, blocked_by_policy,
                                        storage_type);
  }
}

void NotifyStorageAccess(int render_process_id,
                         int32_t render_frame_id,
                         StorageType storage_type,
                         const GURL& url,
                         const url::Origin& top_frame_origin,
                         bool allowed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool should_notify_pscs = ([storage_type]() {
    switch (storage_type) {
      case StorageType::DATABASE:
      case StorageType::LOCAL_STORAGE:
      case StorageType::SESSION_STORAGE:
      case StorageType::FILE_SYSTEM:
      case StorageType::INDEXED_DB:
      case StorageType::CACHE:
        return true;
      case StorageType::WEB_LOCKS:
        // State not persisted, no need to record anything;
        return false;
    }
  })();

  auto metrics_type =
      ([storage_type]() -> absl::optional<page_load_metrics::StorageType> {
        switch (storage_type) {
          case StorageType::LOCAL_STORAGE:
            return page_load_metrics::StorageType::kLocalStorage;
          case StorageType::SESSION_STORAGE:
            return page_load_metrics::StorageType::kSessionStorage;
          case StorageType::FILE_SYSTEM:
            return page_load_metrics::StorageType::kFileSystem;
          case StorageType::INDEXED_DB:
            return page_load_metrics::StorageType::kIndexedDb;
          case StorageType::CACHE:
            return page_load_metrics::StorageType::kCacheStorage;
          case StorageType::DATABASE:
          case StorageType::WEB_LOCKS:
            return absl::nullopt;
        }
      })();

  if (should_notify_pscs) {
    PageSpecificContentSettings::StorageAccessed(
        storage_type, render_process_id, render_frame_id, url, !allowed);
  }

  if (metrics_type) {
    OnStorageAccessed(render_process_id, render_frame_id, url,
                      top_frame_origin.GetURL(), !allowed,
                      metrics_type.value());
  }
}

void OnContentBlockedOnUI(int render_process_id,
                          int32_t render_frame_id,
                          ContentSettingsType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PageSpecificContentSettings::ContentBlocked(render_process_id,
                                              render_frame_id, type);
}

}  // namespace

ContentSettingsManagerImpl::~ContentSettingsManagerImpl() = default;

// static
void ContentSettingsManagerImpl::Create(
    content::RenderProcessHost* render_process_host,
    mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
        receiver,
    std::unique_ptr<Delegate> delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&ContentSettingsManagerImpl::CreateOnThread,
                         render_process_host->GetID(), std::move(receiver),
                         delegate->GetCookieSettings(
                             render_process_host->GetBrowserContext()),
                         std::move(delegate)));
}

void ContentSettingsManagerImpl::Clone(
    mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ContentSettingsManagerImpl(*this)),
      std::move(receiver));
}

void ContentSettingsManagerImpl::AllowStorageAccess(
    int32_t render_frame_id,
    StorageType storage_type,
    const url::Origin& origin,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GURL url = origin.GetURL();

  bool allowed = cookie_settings_->IsFullCookieAccessAllowed(
      url, site_for_cookies, top_frame_origin, net::CookieSettingOverrides(),
      CookieSettings::QueryReason::kSiteStorage);
  if (delegate_->AllowStorageAccess(render_process_id_, render_frame_id,
                                    storage_type, url, allowed, &callback)) {
    DCHECK(!callback);
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NotifyStorageAccess, render_process_id_, render_frame_id,
                     storage_type, url, top_frame_origin, allowed));

  std::move(callback).Run(allowed);
}

void ContentSettingsManagerImpl::OnContentBlocked(int32_t render_frame_id,
                                                  ContentSettingsType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&OnContentBlockedOnUI, render_process_id_,
                                render_frame_id, type));
}

ContentSettingsManagerImpl::ContentSettingsManagerImpl(
    int render_process_id,
    std::unique_ptr<Delegate> delegate,
    scoped_refptr<CookieSettings> cookie_settings)
    : delegate_(std::move(delegate)),
      render_process_id_(render_process_id),
      cookie_settings_(cookie_settings) {}

ContentSettingsManagerImpl::ContentSettingsManagerImpl(
    const ContentSettingsManagerImpl& other)
    : delegate_(other.delegate_->Clone()),
      render_process_id_(other.render_process_id_),
      cookie_settings_(other.cookie_settings_) {}

// static
void ContentSettingsManagerImpl::CreateOnThread(
    int render_process_id,
    mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
        receiver,
    scoped_refptr<CookieSettings> cookie_settings,
    std::unique_ptr<Delegate> delegate) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ContentSettingsManagerImpl(
          render_process_id, std::move(delegate), cookie_settings)),
      std::move(receiver));
}

}  // namespace content_settings
