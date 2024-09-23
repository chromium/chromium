// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/content_settings_manager_impl.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/features.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using content_settings::PageSpecificContentSettings;

namespace content_settings {
namespace {
using StorageType = mojom::ContentSettingsManager::StorageType;

void OnStorageAccessed(const content::GlobalRenderFrameHostToken& frame_token,
                       const GURL& origin_url,
                       const GURL& top_origin_url,
                       bool blocked_by_policy,
                       page_load_metrics::StorageType storage_type) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromFrameToken(frame_token);
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

void NotifyStorageAccess(const content::GlobalRenderFrameHostToken& frame_token,
                         StorageType storage_type,
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

  auto* rfh = content::RenderFrameHost::FromFrameToken(frame_token);

  if (!rfh) {
    return;
  }

  auto metrics_type =
      ([storage_type]() -> std::optional<page_load_metrics::StorageType> {
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
            return std::nullopt;
        }
      })();

  if (should_notify_pscs) {
    PageSpecificContentSettings::StorageAccessed(
        storage_type, frame_token, rfh->GetStorageKey(), !allowed);
  }

  if (metrics_type) {
    OnStorageAccessed(frame_token, rfh->GetLastCommittedURL(),
                      top_frame_origin.GetURL(), !allowed,
                      metrics_type.value());
  }
}

void OnContentBlockedOnUI(
    const content::GlobalRenderFrameHostToken& frame_token,
    ContentSettingsType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PageSpecificContentSettings::ContentBlocked(frame_token, type);
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
    const blink::LocalFrameToken& frame_token,
    StorageType storage_type,
    const url::Origin& origin,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GURL url = origin.GetURL();

  // TODO(crbug.com/40247160): Consider whether the following check should
  // get CookieSettingOverrides from the frame rather than default to none.

  CookieSettingsBase::CookieSettingWithMetadata cookie_settings;

  bool allowed = cookie_settings_->IsFullCookieAccessAllowed(
      url, site_for_cookies, top_frame_origin,
      cookie_settings_->SettingOverridesForStorage(), &cookie_settings);

  //  If storage partitioning is active, third-party partitioned storage is
  //  allowed by default, and access is only blocked due to general third-party
  //  cookie blocking (and not due to a user specified pattern) then we'll allow
  //  storage access.
  if (base::FeatureList::IsEnabled(
          net::features::kThirdPartyStoragePartitioning) &&
      base::FeatureList::IsEnabled(
          net::features::kThirdPartyPartitionedStorageAllowedByDefault) &&
      !allowed && cookie_settings.BlockedByThirdPartyCookieBlocking()) {
    allowed = true;
  }

  // Allow storage when --test-third-party-cookie-phaseout is used, but ensure
  // that only partitioned storage is available. This developer flag is meant to
  // simulate Chrome's behavior when 3P cookies are turned down to help
  // developers test their site.
  if (!allowed && net::cookie_util::IsForceThirdPartyCookieBlockingEnabled()) {
    allowed = true;
  }

  // Allow unpartitioned storage access when the
  // kNativeUnpartitionedStoragePermittedWhen3PCOff feature is enabled. This
  // developer flag is used to simulate Chrome's unpartitioned storage behavior
  // that is otherwise unreachable through command line flags. (Fixes
  // crbug.com/357784801)
  if (!allowed &&
      base::FeatureList::IsEnabled(
          features::kNativeUnpartitionedStoragePermittedWhen3PCOff)) {
    allowed = true;
  }
  if (delegate_->AllowStorageAccess(
          content::GlobalRenderFrameHostToken(render_process_id_, frame_token),
          storage_type, url, allowed, &callback)) {
    DCHECK(!callback);
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&NotifyStorageAccess,
                                content::GlobalRenderFrameHostToken(
                                    render_process_id_, frame_token),
                                storage_type, top_frame_origin, allowed));

  std::move(callback).Run(allowed);
}

void ContentSettingsManagerImpl::OnContentBlocked(
    const blink::LocalFrameToken& frame_token,
    ContentSettingsType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&OnContentBlockedOnUI,
                                content::GlobalRenderFrameHostToken(
                                    render_process_id_, frame_token),
                                type));
}

ContentSettingsManagerImpl::ContentSettingsManagerImpl(
    int render_process_id,
    std::unique_ptr<Delegate> delegate,
    scoped_refptr<CookieSettings> cookie_settings)
    : delegate_(std::move(delegate)),
      render_process_id_(render_process_id),
      cookie_settings_(cookie_settings) {
  CHECK(cookie_settings_);
}

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
