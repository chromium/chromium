// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/content_settings_manager_impl.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using content_settings::PageSpecificContentSettings;

namespace content_settings {
namespace {
using StorageType = mojom::ContentSettingsManager::StorageType;

void OnStorageAccessed(content::RenderFrameHost* render_frame_host,
                       const GURL& origin_url,
                       const GURL& top_origin_url,
                       bool blocked_by_policy,
                       page_load_metrics::StorageType storage_type) {
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

void NotifyStorageAccess(content::RenderFrameHost* rfh,
                         StorageType storage_type,
                         const url::Origin& top_frame_origin,
                         bool allowed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool should_notify_pscs = ([storage_type]() {
    switch (storage_type) {
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
          case StorageType::WEB_LOCKS:
            return std::nullopt;
        }
      })();

  if (should_notify_pscs) {
    PageSpecificContentSettings::StorageAccessed(
        storage_type, rfh->GetGlobalFrameToken(), rfh->GetStorageKey(),
        !allowed);
  }

  if (metrics_type) {
    OnStorageAccessed(rfh, rfh->GetLastCommittedURL(),
                      top_frame_origin.GetURL(), !allowed,
                      metrics_type.value());
  }
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
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      delegate->GetCookieSettings(render_process_host->GetBrowserContext());
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ContentSettingsManagerImpl(
          render_process_host->GetDeprecatedID(), std::move(delegate),
          std::move(cookie_settings))),
      std::move(receiver));
}

void ContentSettingsManagerImpl::Clone(
    mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ContentSettingsManagerImpl(*this)),
      std::move(receiver));
}

void ContentSettingsManagerImpl::AllowStorageAccess(
    const blink::LocalFrameToken& frame_token,
    StorageType storage_type,
    base::OnceCallback<void(bool)> callback) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(render_process_id_, frame_token));
  if (!render_frame_host) {
    // Ideally this would never happen and we would kill the renderer reporting
    // a mojo bad message here. Unfortunately, this does happen, because the
    // renderer calls this method also from workers with the cached parent
    // document frame_token. If this happens, we just return false here.
    std::move(callback).Run(false);
    return;
  }

  const url::Origin& origin = render_frame_host->GetLastCommittedOrigin();
  const url::Origin& top_frame_origin =
      render_frame_host->GetMainFrame()->GetLastCommittedOrigin();
  const net::SiteForCookies& site_for_cookies =
      render_frame_host->GetIsolationInfoForSubresources().site_for_cookies();
  GURL url = origin.GetURL();

  // TODO(crbug.com/40247160): Consider whether the following check should
  // get CookieSettingOverrides from the frame rather than default to none.

  CookieSettingsBase::CookieSettingWithMetadata cookie_settings;

  net::SchemefulSite top_frame_site(top_frame_origin);
  std::optional<net::CookiePartitionKey> cookie_partition_key =
      net::CookiePartitionKey::FromStorageKeyComponents(
          top_frame_site,
          net::CookiePartitionKey::BoolToAncestorChainBit(
              !site_for_cookies.IsFirstParty(origin.GetURL())),
          /*nonce=*/std::nullopt);

  bool allowed = cookie_settings_->IsFullCookieAccessAllowed(
      url, site_for_cookies, top_frame_origin, net::CookieSettingOverrides(),
      cookie_partition_key, &cookie_settings);

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

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  if (delegate_->AllowStorageAccess(render_frame_host, storage_type, url,
                                    allowed, std::move(split_callback.first))) {
    return;
  }

  NotifyStorageAccess(render_frame_host, storage_type, top_frame_origin,
                      allowed);

  std::move(split_callback.second).Run(allowed);
}

void ContentSettingsManagerImpl::OnContentBlocked(
    const blink::LocalFrameToken& frame_token,
    ContentSettingsType type) {
  PageSpecificContentSettings::ContentBlocked(
      content::GlobalRenderFrameHostToken(render_process_id_, frame_token),
      type);
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

}  // namespace content_settings
