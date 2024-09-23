// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/page_specific_content_settings.h"

#include <list>
#include <vector>

#include "base/command_line.h"
#include "base/functional/overloaded.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern_parser.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/trust_token_access_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_constants.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/navigation/renderer_content_settings.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/page_info/core/features.h"
#endif

using content::BrowserThread;
using StorageType =
    content_settings::mojom::ContentSettingsManager::StorageType;
using LifecycleState = content::RenderFrameHost::LifecycleState;

namespace content_settings {
namespace {

// A delay before the media indicator disappears if they were previously used
// longer than `kMediaIndicatorMinimumHoldDuration`.
constexpr auto kMediaIndicatorHoldAfterUseDuration = base::Seconds(1);
// A minimum delay before media indicator disappears.
constexpr auto kMediaIndicatorMinimumHoldDuration = base::Seconds(5);
constexpr auto kMediaIndicatorMinimumHoldDurationPhase2 = base::Seconds(4);
// A delay before blocked media indicator disappears.
constexpr auto kBlockedMediaIndicatorDismissDelay = base::Minutes(1);
constexpr auto kBlockedMediaIndicatorDismissDelayPhase2 = base::Seconds(4);

// Determines which taxonomy is used to generate sample topics for the Topics
// API.
constexpr int kTopicsAPISampleDataTaxonomy = 1;

bool WillNavigationCreateNewPageSpecificContentSettingsOnCommit(
    content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInMainFrame() &&
         !navigation_handle->IsSameDocument() &&
         !navigation_handle->IsServedFromBackForwardCache() &&
         !navigation_handle->IsPrerenderedPageActivation();
}

// Keeps track of cookie and service worker access during a navigation.
// These types of access can happen for the current page or for a new
// navigation (think cookies sent in the HTTP request or service worker
// being run to serve a fetch request). A navigation might fail to
// commit in which case we have to handle it as if it had never
// occurred. So we cache all cookies and service worker accesses that
// happen during a navigation and only apply the changes if the
// navigation commits.
class InflightNavigationContentSettings
    : public content::NavigationHandleUserData<
          InflightNavigationContentSettings> {
 public:
  ~InflightNavigationContentSettings() override;
  std::vector<content::CookieAccessDetails> cookie_accesses;
  std::vector<content::TrustTokenAccessDetails> trust_token_accesses;
  std::vector<std::pair<GURL, content::AllowServiceWorkerResult>>
      service_worker_accesses;
  std::vector<network::mojom::SharedDictionaryAccessDetailsPtr>
      shared_dictionary_accesses;

 private:
  explicit InflightNavigationContentSettings(
      content::NavigationHandle& navigation_handle);
  friend class content::NavigationHandleUserData<
      InflightNavigationContentSettings>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

// This class attaches to WebContents to listen to events and route them to
// appropriate PageSpecificContentSettings, store navigation related events
// until the navigation finishes and then transferring the
// navigation-associated state to the newly-created page.
class WebContentsHandler
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebContentsHandler> {
 public:
  using Delegate = PageSpecificContentSettings::Delegate;
  using SiteDataObserver = PageSpecificContentSettings::SiteDataObserver;

  explicit WebContentsHandler(content::WebContents* web_contents,
                              std::unique_ptr<Delegate> delegate);
  ~WebContentsHandler() override;
  // Adds the given |SiteDataObserver|. The |observer| is notified when a
  // locale shared object, like for example a cookie, is accessed.
  void AddSiteDataObserver(SiteDataObserver* observer);

  // Removes the given |SiteDataObserver|.
  void RemoveSiteDataObserver(SiteDataObserver* observer);

  // Notifies all registered |SiteDataObserver|s.
  void NotifySiteDataObservers(const AccessDetails& access_details);
  void NotifyStatefulBounceObservers();

  // Queues update sent while the navigation is still in progress. The update
  // is run after the navigation completes (DidFinishNavigation).
  void AddPendingCommitUpdate(content::GlobalRenderFrameHostId id,
                              base::OnceClosure update);

  Delegate* delegate() { return delegate_.get(); }

 private:
  friend class content::WebContentsUserData<WebContentsHandler>;

  // Applies all stored events for the given navigation to the current main
  // document.
  void TransferNavigationContentSettingsToCommittedDocument(
      const InflightNavigationContentSettings& navigation_settings,
      content::RenderFrameHost* rfh);

  // content::WebContentsObserver overrides.
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::RenderFrameHost* rfh,
                         const content::CookieAccessDetails& details) override;
  void OnTrustTokensAccessed(
      content::NavigationHandle* navigation,
      const content::TrustTokenAccessDetails& details) override;
  void OnTrustTokensAccessed(
      content::RenderFrameHost* rfh,
      const content::TrustTokenAccessDetails& details) override;
  // Called when a specific Service Worker scope was accessed.
  // If access was blocked due to the user's content settings,
  // |blocked_by_policy_javascript| or/and |blocked_by_policy_cookie|
  // should be true, and this function should invoke OnContentBlocked for
  // JavaScript or/and cookies respectively.
  void OnServiceWorkerAccessed(
      content::NavigationHandle* navigation,
      const GURL& scope,
      content::AllowServiceWorkerResult allowed) override;
  void OnServiceWorkerAccessed(
      content::RenderFrameHost* frame,
      const GURL& scope,
      content::AllowServiceWorkerResult allowed) override;
  void OnSharedDictionaryAccessed(
      content::NavigationHandle* navigation,
      const network::mojom::SharedDictionaryAccessDetails& details) override;
  void OnSharedDictionaryAccessed(
      content::RenderFrameHost* rfh,
      const network::mojom::SharedDictionaryAccessDetails& details) override;
  void WebContentsDestroyed() override;

  std::unique_ptr<Delegate> delegate_;

  raw_ptr<HostContentSettingsMap> map_;

  // All currently registered |SiteDataObserver|s.
  base::ObserverList<SiteDataObserver>::Unchecked observer_list_;

  std::map<content::GlobalRenderFrameHostId, std::vector<base::OnceClosure>>
      pending_commit_updates_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Certain notifications for content accesses (script/storage) can be
// received from the renderer while the RFH is in the kPendingCommit lifecycle
// state. At this point, no PageSpecificContentSettings object is created (as
// this only happens in DidFinishNavigation), so we queue up the update until
// DidFinishNavigation is called. This method returns true if the update is
// queued, false otherwise.
template <typename Method, typename... Args>
bool DelayUntilCommitIfNecessary(content::RenderFrameHost* rfh,
                                 Method method,
                                 Args... args) {
  if (!rfh)
    return false;
  if (rfh->GetLifecycleState() == LifecycleState::kPendingCommit) {
    auto* handler = WebContentsHandler::FromWebContents(
        content::WebContents::FromRenderFrameHost(rfh));
    if (!handler)
      return false;
    handler->AddPendingCommitUpdate(rfh->GetGlobalId(),
                                    base::BindOnce(method, args...));
    return true;
  }
  return false;
}

bool IsThirdPartyCookieDetails(const content::CookieAccessDetails& details) {
  return net::SchemefulSite(details.url) !=
             net::SchemefulSite(details.first_party_url) ||
         !details.site_for_cookies.IsFirstParty(details.url);
}

}  // namespace

InflightNavigationContentSettings::InflightNavigationContentSettings(
    content::NavigationHandle&) {}

InflightNavigationContentSettings::~InflightNavigationContentSettings() =
    default;

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(InflightNavigationContentSettings);

WebContentsHandler::WebContentsHandler(content::WebContents* web_contents,
                                       std::unique_ptr<Delegate> delegate)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<WebContentsHandler>(*web_contents),
      delegate_(std::move(delegate)),
      map_(delegate_->GetSettingsMap()) {
  DCHECK(
      !PageSpecificContentSettings::GetForPage(web_contents->GetPrimaryPage()));
  content::PageUserData<PageSpecificContentSettings>::CreateForPage(
      web_contents->GetPrimaryPage(), delegate_.get());
}

WebContentsHandler::~WebContentsHandler() = default;

void WebContentsHandler::TransferNavigationContentSettingsToCommittedDocument(
    const InflightNavigationContentSettings& navigation_settings,
    content::RenderFrameHost* rfh) {
  for (const auto& cookie_access : navigation_settings.cookie_accesses) {
    OnCookiesAccessed(rfh, cookie_access);
  }
  for (const auto& trust_token_access :
       navigation_settings.trust_token_accesses) {
    OnTrustTokensAccessed(rfh, trust_token_access);
  }
  for (const auto& service_worker_access :
       navigation_settings.service_worker_accesses) {
    OnServiceWorkerAccessed(rfh, service_worker_access.first,
                            service_worker_access.second);
  }
  for (const auto& shared_dictionary_access :
       navigation_settings.shared_dictionary_accesses) {
    OnSharedDictionaryAccessed(rfh, *shared_dictionary_access);
  }
}

void WebContentsHandler::OnCookiesAccessed(
    content::NavigationHandle* navigation,
    const content::CookieAccessDetails& details) {
  if (WillNavigationCreateNewPageSpecificContentSettingsOnCommit(navigation)) {
    auto* inflight_navigation_settings =
        content::NavigationHandleUserData<InflightNavigationContentSettings>::
            GetOrCreateForNavigationHandle(*navigation);
    inflight_navigation_settings->cookie_accesses.push_back(details);
    return;
  }
  // All accesses during main frame navigations should enter the block above and
  // not reach here. We also don't expect any accesses to be made during page
  // activations or same-document navigations.
  DCHECK(navigation->GetParentFrame());
  OnCookiesAccessed(navigation->GetParentFrame()->GetMainFrame(), details);
}

void WebContentsHandler::OnCookiesAccessed(
    content::RenderFrameHost* rfh,
    const content::CookieAccessDetails& details) {
  auto* pscs = PageSpecificContentSettings::GetForPage(rfh->GetPage());
  if (pscs) {
    pscs->OnCookiesAccessed(details);
  }
}

void WebContentsHandler::OnTrustTokensAccessed(
    content::NavigationHandle* navigation,
    const content::TrustTokenAccessDetails& details) {
  if (WillNavigationCreateNewPageSpecificContentSettingsOnCommit(navigation)) {
    auto* inflight_navigation_settings =
        content::NavigationHandleUserData<InflightNavigationContentSettings>::
            GetOrCreateForNavigationHandle(*navigation);
    inflight_navigation_settings->trust_token_accesses.push_back(details);
    return;
  }
  // All accesses during main frame navigations should enter the block above and
  // not reach here. We also don't expect any accesses to be made during page
  // activations or same-document navigations.
  DCHECK(navigation->GetParentFrame());
  OnTrustTokensAccessed(navigation->GetParentFrame()->GetMainFrame(), details);
}

void WebContentsHandler::OnTrustTokensAccessed(
    content::RenderFrameHost* rfh,
    const content::TrustTokenAccessDetails& details) {
  auto* pscs = PageSpecificContentSettings::GetForPage(rfh->GetPage());
  if (pscs) {
    pscs->OnTrustTokenAccessed(details.origin, details.blocked);
  }
}

void WebContentsHandler::OnServiceWorkerAccessed(
    content::NavigationHandle* navigation,
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  DCHECK(scope.is_valid());

  if (WillNavigationCreateNewPageSpecificContentSettingsOnCommit(navigation)) {
    auto* inflight_navigation_settings =
        content::NavigationHandleUserData<InflightNavigationContentSettings>::
            GetOrCreateForNavigationHandle(*navigation);
    inflight_navigation_settings->service_worker_accesses.emplace_back(scope,
                                                                       allowed);
    return;
  }
  // All accesses during main frame navigations should enter the block above and
  // not reach here. We also don't expect any accesses to be made during page
  // activations or same-document navigations.
  DCHECK(navigation->GetParentFrame());
  OnServiceWorkerAccessed(navigation->GetParentFrame()->GetMainFrame(), scope,
                          allowed);
}

void WebContentsHandler::OnServiceWorkerAccessed(
    content::RenderFrameHost* frame,
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  auto* pscs = PageSpecificContentSettings::GetForPage(frame->GetPage());
  if (pscs) {
    pscs->OnServiceWorkerAccessed(scope, frame->GetStorageKey(), allowed);
  }
}

void WebContentsHandler::OnSharedDictionaryAccessed(
    content::NavigationHandle* navigation,
    const network::mojom::SharedDictionaryAccessDetails& details) {
  if (WillNavigationCreateNewPageSpecificContentSettingsOnCommit(navigation)) {
    auto* inflight_navigation_settings =
        content::NavigationHandleUserData<InflightNavigationContentSettings>::
            GetOrCreateForNavigationHandle(*navigation);
    inflight_navigation_settings->shared_dictionary_accesses.emplace_back(
        details.Clone());
    return;
  }
  // All accesses during main frame navigations should enter the block above and
  // not reach here. We also don't expect any accesses to be made during page
  // activations or same-document navigations.
  DCHECK(navigation->GetParentFrame());
  OnSharedDictionaryAccessed(navigation->GetParentFrame()->GetMainFrame(),
                             details);
}

void WebContentsHandler::OnSharedDictionaryAccessed(
    content::RenderFrameHost* rfh,
    const network::mojom::SharedDictionaryAccessDetails& details) {
  PageSpecificContentSettings::BrowsingDataAccessed(
      rfh, details.isolation_key,
      BrowsingDataModel::StorageType::kSharedDictionary, details.is_blocked);
}
void WebContentsHandler::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  content::RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();

  RendererContentSettingRules rules;
  content_settings::GetRendererContentSettingRules(map_, &rules);
  delegate()->SetDefaultRendererContentSettingRules(rfh, &rules);
  const GURL& primary_url =
      navigation_handle->GetParentFrameOrOuterDocument()
          ? navigation_handle->GetParentFrameOrOuterDocument()
                ->GetOutermostMainFrame()
                ->GetLastCommittedURL()
          : navigation_handle->GetURL();

  rules.FilterRulesByOutermostMainFrameURL(primary_url);

  mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent> agent;
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&agent);
  // TODO(crbug.com/40172977): We shouldn't be sending the primary patterns here
  // because: a) we have already filtered based on them and they are not needed
  // in the renderer, and b) they could leak the embedder origin to embedded
  // pages like fenced frames.
  agent->SendRendererContentSettingRules(std::move(rules));

  const GURL& secondary_url = navigation_handle->GetURL();

  auto content_settings = blink::CreateDefaultRendererContentSettings();
  // The `Delegate` may have use cases for allowing JavaScript in a frame,
  // regardless of the content setting.
  content_settings->allow_script =
      delegate()->IsFrameAllowlistedForJavaScript(rfh) ||
      map_->GetContentSetting(primary_url, secondary_url,
                              ContentSettingsType::JAVASCRIPT) ==
          CONTENT_SETTING_ALLOW;
  content_settings->allow_popup =
      map_->GetContentSetting(primary_url, secondary_url,
                              ContentSettingsType::POPUPS) ==
      CONTENT_SETTING_ALLOW;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  content_settings->allow_image =
      map_->GetContentSetting(primary_url, secondary_url,
                              ContentSettingsType::IMAGES) ==
      CONTENT_SETTING_ALLOW;
  content_settings->allow_mixed_content =
      map_->GetContentSetting(primary_url, secondary_url,
                              ContentSettingsType::MIXEDSCRIPT) ==
      CONTENT_SETTING_ALLOW;
#endif

  navigation_handle->SetContentSettings(std::move(content_settings));
}

void WebContentsHandler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  if (WillNavigationCreateNewPageSpecificContentSettingsOnCommit(
          navigation_handle)) {
    content::PageUserData<PageSpecificContentSettings>::CreateForPage(
        navigation_handle->GetRenderFrameHost()->GetPage(), delegate_.get());
    InflightNavigationContentSettings* inflight_settings =
        content::NavigationHandleUserData<InflightNavigationContentSettings>::
            GetForNavigationHandle(*navigation_handle);

    if (inflight_settings) {
      TransferNavigationContentSettingsToCommittedDocument(
          *inflight_settings, navigation_handle->GetRenderFrameHost());
    }

    content::GlobalRenderFrameHostId rfh_id =
        navigation_handle->GetRenderFrameHost()->GetGlobalId();
    auto it = pending_commit_updates_.find(rfh_id);
    if (it != pending_commit_updates_.end()) {
      for (auto& update : it->second) {
        std::move(update).Run();
      }
      pending_commit_updates_.erase(it);
    }
  }

  if (navigation_handle->IsPrerenderedPageActivation()) {
    auto* pscs = PageSpecificContentSettings::GetForFrame(
        navigation_handle->GetRenderFrameHost());
    DCHECK(pscs);

    pscs->OnPrerenderingPageActivation();
  }

  if (navigation_handle->IsInPrimaryMainFrame())
    delegate_->UpdateLocationBar();
}

void WebContentsHandler::AddSiteDataObserver(SiteDataObserver* observer) {
  observer_list_.AddObserver(observer);
}

void WebContentsHandler::RemoveSiteDataObserver(SiteDataObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void WebContentsHandler::NotifySiteDataObservers(
    const AccessDetails& access_details) {
  for (SiteDataObserver& observer : observer_list_)
    observer.OnSiteDataAccessed(access_details);
}

void WebContentsHandler::NotifyStatefulBounceObservers() {
  for (SiteDataObserver& observer : observer_list_) {
    observer.OnStatefulBounceDetected();
  }
}

void WebContentsHandler::AddPendingCommitUpdate(
    content::GlobalRenderFrameHostId id,
    base::OnceClosure update) {
#if DCHECK_IS_ON()
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(id);
  DCHECK(rfh);
  DCHECK_EQ(rfh->GetLifecycleState(), LifecycleState::kPendingCommit);
#endif
  pending_commit_updates_[id].push_back(std::move(update));
}

void WebContentsHandler::WebContentsDestroyed() {
  for (SiteDataObserver& observer : observer_list_) {
    observer.WebContentsDestroyed();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsHandler);

AccessDetails::AccessDetails() = default;
AccessDetails::AccessDetails(SiteDataType site_data_type,
                             AccessType access_type,
                             GURL url,
                             bool blocked_by_policy,
                             bool is_from_primary_page)
    : site_data_type(site_data_type),
      access_type(access_type),
      url(url),
      blocked_by_policy(blocked_by_policy),
      is_from_primary_page(is_from_primary_page) {}

AccessDetails::~AccessDetails() = default;

bool AccessDetails::operator<(const AccessDetails& other) const {
  return std::tie(site_data_type, access_type, url, blocked_by_policy,
                  is_from_primary_page) <
         std::tie(other.site_data_type, other.access_type, other.url,
                  other.blocked_by_policy, other.is_from_primary_page);
}

PageSpecificContentSettings::SiteDataObserver::SiteDataObserver(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  // Make sure the handler was attached to the WebContents as some UT might skip
  // this.
  auto* handler = WebContentsHandler::FromWebContents(web_contents_);
  if (handler) {
    handler->AddSiteDataObserver(this);
  } else {
    web_contents_ = nullptr;
  }
}

PageSpecificContentSettings::SiteDataObserver::~SiteDataObserver() {
  if (!web_contents_)
    return;
  auto* handler = WebContentsHandler::FromWebContents(web_contents_);
  if (handler)
    handler->RemoveSiteDataObserver(this);
}

void PageSpecificContentSettings::SiteDataObserver::WebContentsDestroyed() {
  auto* handler = WebContentsHandler::FromWebContents(web_contents_);
  if (handler) {
    handler->RemoveSiteDataObserver(this);
  }
  web_contents_ = nullptr;
}

PageSpecificContentSettings::PendingUpdates::PendingUpdates() = default;

PageSpecificContentSettings::PendingUpdates::~PendingUpdates() = default;

PageSpecificContentSettings::PageSpecificContentSettings(content::Page& page,
                                                         Delegate* delegate)
    : content::PageUserData<PageSpecificContentSettings>(page),
      delegate_(delegate),
      map_(delegate_->GetSettingsMap()),
      allowed_browsing_data_model_(BrowsingDataModel::BuildEmpty(
          GetWebContents()->GetPrimaryMainFrame()->GetStoragePartition(),
          delegate_->CreateBrowsingDataModelDelegate())),
      blocked_browsing_data_model_(BrowsingDataModel::BuildEmpty(
          GetWebContents()->GetPrimaryMainFrame()->GetStoragePartition(),
          delegate_->CreateBrowsingDataModelDelegate())) {
  observation_.Observe(map_.get());
  if (page.GetMainDocument().GetLifecycleState() ==
      LifecycleState::kPrerendering) {
    updates_queued_during_prerender_ = std::make_unique<PendingUpdates>();
  }
}

PageSpecificContentSettings::~PageSpecificContentSettings() {
  for (auto last_used_entry : last_used_time_) {
    switch (last_used_entry.first) {
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::MEDIASTREAM_CAMERA:
        map_->UpdateLastUsedTime(media_stream_access_origin_,
                                 media_stream_access_origin_,
                                 last_used_entry.first, last_used_entry.second);
        break;
      default:
        // Currently, only camera and mic permissions are supported.
        NOTREACHED_IN_MIGRATION();
    }
  }
}

// static
void PageSpecificContentSettings::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<Delegate> delegate) {
  WebContentsHandler::CreateForWebContents(web_contents, std::move(delegate));
}

// static
void PageSpecificContentSettings::DeleteForWebContentsForTest(
    content::WebContents* web_contents) {
  PageSpecificContentSettings::DeleteForPage(web_contents->GetPrimaryPage());

  web_contents->RemoveUserData(WebContentsHandler::UserDataKey());
}

// static
PageSpecificContentSettings* PageSpecificContentSettings::GetForFrame(
    const content::GlobalRenderFrameHostId& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetForFrame(content::RenderFrameHost::FromID(id));
}

// static
PageSpecificContentSettings* PageSpecificContentSettings::GetForFrame(
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return rfh ? PageSpecificContentSettings::GetForPage(rfh->GetPage())
             : nullptr;
}

// static
PageSpecificContentSettings::Delegate*
PageSpecificContentSettings::GetDelegateForWebContents(
    content::WebContents* web_contents) {
  auto* handler = WebContentsHandler::FromWebContents(web_contents);
  return handler ? handler->delegate() : nullptr;
}

// static
void PageSpecificContentSettings::StorageAccessed(
    StorageType storage_type,
    absl::variant<content::GlobalRenderFrameHostToken,
                  content::GlobalRenderFrameHostId> frame_id,
    const blink::StorageKey& storage_key,
    bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderFrameHost* rfh = absl::visit(
      base::Overloaded{
          [](const content::GlobalRenderFrameHostToken& frame_token) {
            return content::RenderFrameHost::FromFrameToken(frame_token);
          },
          [](const content::GlobalRenderFrameHostId& id) {
            return content::RenderFrameHost::FromID(id);
          },
      },
      frame_id);

  if (DelayUntilCommitIfNecessary(
          rfh, &PageSpecificContentSettings::StorageAccessed, storage_type,
          frame_id, storage_key, blocked_by_policy)) {
    return;
  }
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if (settings) {
    auto bdm_storage_type = ([storage_type]() {
      switch (storage_type) {
        case StorageType::LOCAL_STORAGE:
          return BrowsingDataModel::StorageType::kLocalStorage;
        case StorageType::SESSION_STORAGE:
          return BrowsingDataModel::StorageType::kSessionStorage;
        case StorageType::FILE_SYSTEM:
        case StorageType::INDEXED_DB:
        case StorageType::DATABASE:
        case StorageType::CACHE:
        case StorageType::WEB_LOCKS:
          return BrowsingDataModel::StorageType::kQuotaStorage;
      }
    })();
    if (storage_type == StorageType::SESSION_STORAGE) {
      auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
      const auto& session_storage_namespace_map =
          web_contents->GetController().GetSessionStorageNamespaceMap();
      const auto& storage_partition_config =
          web_contents->GetSiteInstance()->GetStoragePartitionConfig();
      const auto& namespace_id =
          session_storage_namespace_map.at(storage_partition_config);

      content::SessionStorageUsageInfo session_storage_usage_info{
          storage_key, namespace_id->id()};
      settings->OnBrowsingDataAccessed(session_storage_usage_info,
                                       bdm_storage_type, blocked_by_policy);
    } else {
      settings->OnBrowsingDataAccessed(storage_key, bdm_storage_type,
                                       blocked_by_policy);
    }
  }
}

// static
void PageSpecificContentSettings::BrowsingDataAccessed(
    content::RenderFrameHost* rfh,
    BrowsingDataModel::DataKey data_key,
    BrowsingDataModel::StorageType storage_type,
    bool blocked) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if (settings) {
    settings->OnBrowsingDataAccessed(data_key, storage_type, blocked);
  }
}

// static
void PageSpecificContentSettings::ContentBlocked(
    const content::GlobalRenderFrameHostToken& frame_token,
    ContentSettingsType type) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromFrameToken(frame_token);
  if (DelayUntilCommitIfNecessary(rfh,
                                  &PageSpecificContentSettings::ContentBlocked,
                                  frame_token, type)) {
    return;
  }
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if (settings) {
    settings->OnContentBlocked(type);
  }
}

// static
void PageSpecificContentSettings::SharedWorkerAccessed(
    int render_process_id,
    int render_frame_id,
    const GURL& worker_url,
    const std::string& name,
    const blink::StorageKey& storage_key,
    const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies,
    bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings = GetForFrame(
      content::RenderFrameHost::FromID(render_process_id, render_frame_id));
  if (settings) {
    settings->OnBrowsingDataAccessed(
        browsing_data::SharedWorkerInfo{worker_url, name, storage_key,
                                        same_site_cookies},
        BrowsingDataModel::StorageType::kSharedWorker, blocked_by_policy);
  }
}

// static
void PageSpecificContentSettings::InterestGroupJoined(
    content::RenderFrameHost* rfh,
    const url::Origin& api_origin,
    bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if (settings) {
    settings->OnInterestGroupJoined(api_origin, blocked_by_policy);
  }
}

// static
void PageSpecificContentSettings::TopicAccessed(
    content::RenderFrameHost* rfh,
    const url::Origin& api_origin,
    bool blocked_by_policy,
    privacy_sandbox::CanonicalTopic topic) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if (settings) {
    settings->OnTopicAccessed(api_origin, blocked_by_policy, topic);
  }
}

// static
void PageSpecificContentSettings::NotificationsAccessed(
    content::RenderFrameHost* rfh,
    bool blocked) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if (settings) {
    if (blocked) {
      settings->OnContentBlocked(ContentSettingsType::NOTIFICATIONS);
    } else {
      settings->OnContentAllowed(ContentSettingsType::NOTIFICATIONS);
    }
  }
}

// static
content::WebContentsObserver*
PageSpecificContentSettings::GetWebContentsObserverForTest(
    content::WebContents* web_contents) {
  return WebContentsHandler::FromWebContents(web_contents);
}

bool PageSpecificContentSettings::IsContentBlocked(
    ContentSettingsType content_type) const {
  DCHECK_NE(ContentSettingsType::AUTOMATIC_DOWNLOADS, content_type)
      << "Automatic downloads handled by DownloadRequestLimiter";
  CHECK_NE(ContentSettingsType::STORAGE_ACCESS, content_type)
      << "StorageAccess handled by GetTwoOriginRequests";

  if (content_type == ContentSettingsType::IMAGES ||
      content_type == ContentSettingsType::JAVASCRIPT ||
      content_type == ContentSettingsType::COOKIES ||
      content_type == ContentSettingsType::POPUPS ||
      content_type == ContentSettingsType::MIXEDSCRIPT ||
      content_type == ContentSettingsType::MEDIASTREAM_MIC ||
      content_type == ContentSettingsType::MEDIASTREAM_CAMERA ||
      content_type == ContentSettingsType::MIDI_SYSEX ||
      content_type == ContentSettingsType::ADS ||
      content_type == ContentSettingsType::SOUND ||
      content_type == ContentSettingsType::CLIPBOARD_READ_WRITE ||
      content_type == ContentSettingsType::SENSORS ||
      content_type == ContentSettingsType::GEOLOCATION ||
      content_type == ContentSettingsType::NOTIFICATIONS) {
    const auto& it = content_settings_status_.find(content_type);
    if (it != content_settings_status_.end()) {
      return it->second.blocked;
    }
  }

  return false;
}

bool PageSpecificContentSettings::IsContentAllowed(
    ContentSettingsType content_type) const {
  DCHECK_NE(ContentSettingsType::AUTOMATIC_DOWNLOADS, content_type)
      << "Automatic downloads handled by DownloadRequestLimiter";
  CHECK_NE(ContentSettingsType::STORAGE_ACCESS, content_type)
      << "StorageAccess handled by GetTwoOriginRequests";

  // This method currently only returns meaningful values for the types listed
  // below.
  if (content_type != ContentSettingsType::COOKIES &&
      content_type != ContentSettingsType::MEDIASTREAM_MIC &&
      content_type != ContentSettingsType::MEDIASTREAM_CAMERA &&
      content_type != ContentSettingsType::MIDI_SYSEX &&
      content_type != ContentSettingsType::CLIPBOARD_READ_WRITE &&
      content_type != ContentSettingsType::SENSORS &&
      content_type != ContentSettingsType::GEOLOCATION &&
      content_type != ContentSettingsType::NOTIFICATIONS) {
    return false;
  }

  const auto& it = content_settings_status_.find(content_type);
  if (it != content_settings_status_.end()) {
    return it->second.allowed;
  }
  return false;
}

std::map<net::SchemefulSite, /*is_allowed*/ bool>
PageSpecificContentSettings::GetTwoSiteRequests(
    ContentSettingsType content_type) {
  return content_settings_two_site_requests_[content_type];
}

void PageSpecificContentSettings::
    SetNotificationsWasDeniedBecauseOfSystemPermission() {
  if (notifications_was_denied_because_of_system_permission_) {
    return;
  }
  notifications_was_denied_because_of_system_permission_ = true;
  MaybeUpdateLocationBar();
}

void PageSpecificContentSettings::OnContentBlocked(ContentSettingsType type) {
  DCHECK(type != ContentSettingsType::MEDIASTREAM_MIC &&
         type != ContentSettingsType::MEDIASTREAM_CAMERA)
      << "Media stream settings handled by OnMediaStreamPermissionSet";

  if (freeze_indicators_) {
    return;
  }

  if (!content_settings::ContentSettingsRegistry::GetInstance()->Get(type)) {
    return;
  }

  ContentSettingsStatus& status = content_settings_status_[type];
  if (!status.blocked) {
    status.blocked = true;
    if (!is_updating_synced_pscs_) {
      base::AutoReset<bool> auto_reset(&is_updating_synced_pscs_, true);
      if (auto* synced_pccs = MaybeGetSyncedSettingsForPictureInPicture()) {
        synced_pccs->OnContentBlocked(type);
      }
    }
    MaybeUpdateLocationBar();
    NotifyDelegate(&Delegate::OnContentBlocked, type);
    MaybeUpdateParent(&PageSpecificContentSettings::OnContentBlocked, type);
  }
}

void PageSpecificContentSettings::OnContentAllowed(ContentSettingsType type) {
  DCHECK(type != ContentSettingsType::MEDIASTREAM_MIC &&
         type != ContentSettingsType::MEDIASTREAM_CAMERA)
      << "Media stream settings handled by OnMediaStreamPermissionSet";
  bool access_changed = false;
  ContentSettingsStatus& status = content_settings_status_[type];

  // Whether to reset status for the |blocked| setting to avoid ending up
  // with both |allowed| and |blocked| set, which can mean multiple things
  // (allowed setting that got disabled, disabled setting that got enabled).
  bool must_reset_blocked_status = false;

  // For sensors, the status with both allowed/blocked flags set means that
  // access was previously allowed but the last decision was to block.
  // Reset the blocked flag so that the UI will properly indicate that the
  // last decision here instead was to allow sensor access.
  if (type == ContentSettingsType::SENSORS) {
    must_reset_blocked_status = true;
  }

  // If this content settings is for the PiP window, must reset |blocked|
  // status since it will not be updated in OnContentSettingChanged() like the
  // normal browser window. We need the status to be precise to display the
  // UI.
  auto* synced_pccs = MaybeGetSyncedSettingsForPictureInPicture();
  if (synced_pccs) {
    must_reset_blocked_status = true;
  }

#if BUILDFLAG(IS_ANDROID)
  // content_settings_status_[type].allowed is always set to true in
  // OnContentBlocked, so we have to use
  // content_settings_status_[type].blocked to detect whether the protected
  // media setting has changed.
  if (type == ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER)
    must_reset_blocked_status = true;
#endif

  if (must_reset_blocked_status && status.blocked) {
    status.blocked = false;
    access_changed = true;
  }
  if (!status.allowed) {
    status.allowed = true;
    access_changed = true;
    NotifyDelegate(&Delegate::OnContentAllowed, type);
  }

  if (access_changed) {
    if (!is_updating_synced_pscs_ && synced_pccs) {
      base::AutoReset<bool> auto_reset(&is_updating_synced_pscs_, true);
      synced_pccs->OnContentAllowed(type);
    }
    MaybeUpdateLocationBar();
    MaybeUpdateParent(&PageSpecificContentSettings::OnContentAllowed, type);
  }
}

void PageSpecificContentSettings::OnTwoSitePermissionChanged(
    ContentSettingsType type,
    net::SchemefulSite requesting_site,
    ContentSetting content_setting) {
  bool access_changed = false;

  auto& site_map = content_settings_two_site_requests_[type];

  switch (content_setting) {
    case CONTENT_SETTING_ASK:
    case CONTENT_SETTING_DEFAULT:
      if (site_map.contains(requesting_site)) {
        site_map.erase(requesting_site);
        access_changed = true;
      }
      break;
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_BLOCK: {
      bool is_allowed = content_setting == CONTENT_SETTING_ALLOW;
      if (!site_map.contains(requesting_site) ||
          site_map[requesting_site] != is_allowed) {
        site_map[requesting_site] = is_allowed;
        access_changed = true;
      }
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION() << content_setting;
  }

  if (access_changed) {
    MaybeUpdateLocationBar();
  }
}

void PageSpecificContentSettings::OnCookiesAccessed(
    const content::CookieAccessDetails& details,
    content::Page* originating_page) {
  originating_page = originating_page ? originating_page : &page();
  if (details.cookie_access_result_list.empty()) {
    return;
  }

  bool blocked = details.blocked_by_policy;
  auto& model =
      blocked ? blocked_browsing_data_model_ : allowed_browsing_data_model_;
  for (const auto& cookie_with_access_result :
       details.cookie_access_result_list) {
    // The size isn't relevant here and won't be displayed in the UI.
    model->AddBrowsingData(cookie_with_access_result.cookie,
                           BrowsingDataModel::StorageType::kCookie,
                           /*storage_size=*/0,
                           /*cookie_count=*/1,
                           /*blocked_third_party=*/
                           (blocked && IsThirdPartyCookieDetails(details)));
  }

  if (details.blocked_by_policy) {
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    OnContentAllowed(ContentSettingsType::COOKIES);
  }

  MaybeUpdateParent(&PageSpecificContentSettings::OnCookiesAccessed, details,
                    originating_page);

  AccessDetails access_details{
      SiteDataType::kCookies,
      details.type == network::mojom::CookieAccessDetails::Type::kChange
          ? AccessType::kWrite
          : AccessType::kRead,
      details.url, details.blocked_by_policy, originating_page->IsPrimary()};

  MaybeNotifySiteDataObservers(access_details);
}

void PageSpecificContentSettings::OnServiceWorkerAccessed(
    const GURL& scope,
    const blink::StorageKey& storage_key,
    content::AllowServiceWorkerResult allowed_result,
    content::Page* originating_page) {
  DCHECK(scope.is_valid());
  originating_page = originating_page ? originating_page : &page();
  auto& model = allowed_result ? allowed_browsing_data_model_
                               : blocked_browsing_data_model_;
  // The size isn't relevant here and won't be displayed in the UI.
  model->AddBrowsingData(storage_key,
                         BrowsingDataModel::StorageType::kQuotaStorage,
                         /*storage_size=*/0);

  if (allowed_result.javascript_blocked_by_policy()) {
    OnContentBlocked(ContentSettingsType::JAVASCRIPT);
  } else {
    OnContentAllowed(ContentSettingsType::JAVASCRIPT);
  }
  if (allowed_result.cookies_blocked_by_policy()) {
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    OnContentAllowed(ContentSettingsType::COOKIES);
  }

  MaybeUpdateParent(&PageSpecificContentSettings::OnServiceWorkerAccessed,
                    scope, storage_key, allowed_result, originating_page);
}

void PageSpecificContentSettings::OnInterestGroupJoined(
    const url::Origin& api_origin,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    // TODO(crbug.com/40066162): Report the COOKIES content setting type as
    // having been blocked when the UI is updated to better reflect site data.
    blocked_interest_group_api_.push_back(api_origin);
  } else {
    allowed_interest_group_api_.push_back(api_origin);
    OnContentAllowed(ContentSettingsType::COOKIES);
  }

  MaybeUpdateParent(&PageSpecificContentSettings::OnInterestGroupJoined,
                    api_origin, blocked_by_policy);

  // Joining an interest is by default modifying data so this is considered an
  // `AccessType::kWrite`.
  AccessDetails access_details{SiteDataType::kInterestGroup, AccessType::kWrite,
                               api_origin.GetURL(), blocked_by_policy, false};
  MaybeNotifySiteDataObservers(access_details);
}

void PageSpecificContentSettings::OnTopicAccessed(
    const url::Origin& api_origin,
    bool blocked_by_policy,
    privacy_sandbox::CanonicalTopic topic) {
  // TODO(crbug.com/40210776): Add URL and Topic to local_shared_objects?
  accessed_topics_.insert(topic);
  MaybeUpdateParent(&PageSpecificContentSettings::OnTopicAccessed, api_origin,
                    blocked_by_policy, topic);
}

void PageSpecificContentSettings::OnTrustTokenAccessed(
    const url::Origin& api_origin,
    bool blocked) {
  OnBrowsingDataAccessed(api_origin,
                         BrowsingDataModel::StorageType::kTrustTokens, blocked);
}

void PageSpecificContentSettings::OnBrowsingDataAccessed(
    BrowsingDataModel::DataKey data_key,
    BrowsingDataModel::StorageType storage_type,
    bool blocked,
    content::Page* originating_page) {
  originating_page = originating_page ? originating_page : &page();
  auto& model =
      blocked ? blocked_browsing_data_model_ : allowed_browsing_data_model_;

  // The size isn't relevant here and won't be displayed in the UI.
  model->AddBrowsingData(data_key, storage_type, /*storage_size=*/0);

  if (blocked) {
    // Reduce the set of items reported for block to things that are obviously
    // related to cookies, as that is the icon that is displayed.
    // TODO(crbug.com/40066162): When the COOKIES content setting Omnibox entry
    // correctly reflects site data, reconsider limiting the types.
    if (model->IsStorageTypeCookieLike(storage_type)) {
      OnContentBlocked(ContentSettingsType::COOKIES);
    }
  } else {
    OnContentAllowed(ContentSettingsType::COOKIES);
  }
  MaybeUpdateParent(&PageSpecificContentSettings::OnBrowsingDataAccessed,
                    data_key, storage_type, blocked, originating_page);

  // TODO(njeunje): Look into populating an actual url for this access details.
  // Could be obtained from the `data_key`.
  GURL accessing_url =
      absl::holds_alternative<blink::StorageKey>(data_key)
          ? absl::get<blink::StorageKey>(data_key).origin().GetURL()
          : GURL();

  // Session storage uses a different DataKey than other storage types.
  if (storage_type == BrowsingDataModel::StorageType::kSessionStorage) {
    accessing_url = absl::get<content::SessionStorageUsageInfo>(data_key)
                        .storage_key.origin()
                        .GetURL();
  }

  AccessDetails access_details{SiteDataType::kStorage, AccessType::kUnknown,
                               accessing_url, blocked,
                               originating_page->IsPrimary()};
  MaybeNotifySiteDataObservers(access_details);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
void PageSpecificContentSettings::OnProtectedMediaIdentifierPermissionSet(
    const GURL& requesting_origin,
    bool allowed) {
  if (allowed) {
    OnContentAllowed(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER);
  } else {
    OnContentBlocked(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER);
  }
}
#endif

PageSpecificContentSettings::MicrophoneCameraState
PageSpecificContentSettings::GetMicrophoneCameraState() const {
  return Union(microphone_camera_state_, delegate_->GetMicrophoneCameraState());
}

bool PageSpecificContentSettings::IsMicrophoneCameraStateChanged() const {
  if (microphone_camera_state_.Has(kMicrophoneAccessed) &&
      (microphone_camera_state_.Has(kMicrophoneBlocked)
           ? !IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC)
           : !IsContentAllowed(ContentSettingsType::MEDIASTREAM_MIC))) {
    return true;
  }

  if (microphone_camera_state_.Has(kCameraAccessed) &&
      (microphone_camera_state_.Has(kCameraBlocked)
           ? !IsContentBlocked(ContentSettingsType::MEDIASTREAM_CAMERA)
           : !IsContentAllowed(ContentSettingsType::MEDIASTREAM_CAMERA))) {
    return true;
  }

  return false;
}

void PageSpecificContentSettings::OnMediaStreamPermissionSet(
    const GURL& request_origin,
    MicrophoneCameraState new_microphone_camera_state) {
  DCHECK(!IsEmbeddedPage());

  // Camera and/or Mic permission request could auto-ignore in case of a page
  // refresh. In this case `OnMediaStreamPermissionSet` should not store media
  // stream state and it should not update activity indicators.
  if (freeze_indicators_ && (new_microphone_camera_state.HasAny(
                                {kMicrophoneBlocked, kCameraBlocked}))) {
    return;
  }

  media_stream_access_origin_ = request_origin;

  if (new_microphone_camera_state.Has(kMicrophoneAccessed)) {
    bool mic_blocked = new_microphone_camera_state.Has(kMicrophoneBlocked);
    ContentSettingsStatus& status =
        content_settings_status_[ContentSettingsType::MEDIASTREAM_MIC];
    if (!status.allowed && !mic_blocked) {
      NotifyDelegate(&Delegate::OnContentAllowed,
                     ContentSettingsType::MEDIASTREAM_MIC);
    }
    status.allowed = !mic_blocked;
    status.blocked = mic_blocked;
  }

  if (new_microphone_camera_state.Has(kCameraAccessed)) {
    bool cam_blocked = new_microphone_camera_state.Has(kCameraBlocked);
    ContentSettingsStatus& status =
        content_settings_status_[ContentSettingsType::MEDIASTREAM_CAMERA];
    if (!status.allowed && !cam_blocked) {
      NotifyDelegate(&Delegate::OnContentAllowed,
                     ContentSettingsType::MEDIASTREAM_CAMERA);
    }
    status.allowed = !cam_blocked;
    status.blocked = cam_blocked;
  }

  if (microphone_camera_state_ != new_microphone_camera_state) {
    if (!is_updating_synced_pscs_) {
      base::AutoReset<bool> auto_reset(&is_updating_synced_pscs_, true);
      if (auto* synced_pccs = MaybeGetSyncedSettingsForPictureInPicture()) {
        synced_pccs->OnMediaStreamPermissionSet(request_origin,
                                                new_microphone_camera_state);
      }
    }

    if (base::FeatureList::IsEnabled(
            content_settings::features::kLeftHandSideActivityIndicators)) {
      // Microphone and Camera share an activity indicator view. If a blocked
      // indicator is displayed, there is no need to re-show it and it will be
      // reset automatically. An in-use indicator will be shown/hidden in
      // `OnCapturingStateChanged`.
      if (microphone_camera_state_.HasAny(
              {kCameraAccessed, kMicrophoneAccessed})) {
        return;
      }
    }

    microphone_camera_state_ = new_microphone_camera_state;

    MaybeUpdateLocationBar();
  }

  if (base::FeatureList::IsEnabled(
          content_settings::features::kImprovedSemanticsActivityIndicators)) {
    // Camera and/or Mic is blocked, start a blocked indicator's dismiss timer.
    if (microphone_camera_state_.Has(kMicrophoneBlocked)) {
      StartBlockedIndicatorTimer(ContentSettingsType::MEDIASTREAM_MIC);
    }
    if (microphone_camera_state_.Has(kCameraBlocked)) {
      StartBlockedIndicatorTimer(ContentSettingsType::MEDIASTREAM_CAMERA);
    }
  }
}

void PageSpecificContentSettings::AddPermissionUsageObserver(
    PermissionUsageObserver* observer) {
  permission_usage_observers_.AddObserver(observer);
}
void PageSpecificContentSettings::RemovePermissionUsageObserver(
    PermissionUsageObserver* observer) {
  permission_usage_observers_.RemoveObserver(observer);
}

void PageSpecificContentSettings::ClearPopupsBlocked() {
  ContentSettingsStatus& status =
      content_settings_status_[ContentSettingsType::POPUPS];
  status.blocked = false;
  if (!is_updating_synced_pscs_) {
    base::AutoReset<bool> auto_reset(&is_updating_synced_pscs_, true);
    if (auto* synced_pccs = MaybeGetSyncedSettingsForPictureInPicture()) {
      synced_pccs->ClearPopupsBlocked();
    }
  }
  MaybeUpdateLocationBar();
}

void PageSpecificContentSettings::OnAudioBlocked() {
  OnContentBlocked(ContentSettingsType::SOUND);
}

void PageSpecificContentSettings::IncrementStatefulBounceCount() {
  stateful_bounce_count_++;
  WebContentsHandler::FromWebContents(GetWebContents())
      ->NotifyStatefulBounceObservers();
}

void PageSpecificContentSettings::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  if (IsEmbeddedPage())
    return;

  const GURL current_url = page().GetMainDocument().GetLastCommittedURL();
  if (content_type == ContentSettingsType::STORAGE_ACCESS) {
    if (!secondary_pattern.Matches(current_url)) {
      return;
    }
  } else {
    if (!primary_pattern.Matches(current_url)) {
      return;
    }
  }

  ContentSettingsStatus& status = content_settings_status_[content_type];
  switch (content_type) {
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA: {
      const GURL media_origin = media_stream_access_origin();
      ContentSetting setting =
          map_->GetContentSetting(media_origin, media_origin, content_type);

      if (content_type == ContentSettingsType::MEDIASTREAM_MIC &&
          setting == CONTENT_SETTING_ALLOW) {
        mic_was_just_granted_on_site_level_ = true;
      }

      if (content_type == ContentSettingsType::MEDIASTREAM_CAMERA &&
          setting == CONTENT_SETTING_ALLOW) {
        camera_was_just_granted_on_site_level_ = true;
      }

      status.allowed = setting == CONTENT_SETTING_ALLOW;
      status.blocked = setting == CONTENT_SETTING_BLOCK;
      break;
    }
    case ContentSettingsType::GEOLOCATION: {
      ContentSetting geolocation_setting =
          map_->GetContentSetting(current_url, current_url, content_type);
      if (geolocation_setting == CONTENT_SETTING_ALLOW) {
        geolocation_was_just_granted_on_site_level_ = true;
      } else if (geolocation_setting == CONTENT_SETTING_ASK) {
        // On manual permission revocation as well as automatic permission
        // revocation (e.g. due to content setting expiry), the content setting
        // icon for the permission needs to be hidden, hence a location bar
        // update may be required.
        MaybeUpdateLocationBar();
      }

      [[fallthrough]];
    }
    case ContentSettingsType::NOTIFICATIONS:
      MaybeUpdateLocationBar();
      [[fallthrough]];
    case ContentSettingsType::IMAGES:
    case ContentSettingsType::JAVASCRIPT:
    case ContentSettingsType::COOKIES:
    case ContentSettingsType::POPUPS:
    case ContentSettingsType::MIXEDSCRIPT:
    case ContentSettingsType::MIDI_SYSEX:
    case ContentSettingsType::ADS:
    case ContentSettingsType::SOUND:
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
    case ContentSettingsType::SENSORS: {
      ContentSetting setting =
          map_->GetContentSetting(current_url, current_url, content_type);
      // If an indicator is shown and the content settings has changed, swap the
      // indicator for the one with the opposite meaning (allowed <=> blocked).
      if (setting == CONTENT_SETTING_BLOCK && status.allowed) {
        status.blocked = false;
        status.allowed = false;
        OnContentBlocked(content_type);
      } else if (setting == CONTENT_SETTING_ALLOW && status.blocked) {
        status.blocked = false;
        status.allowed = false;
        OnContentAllowed(content_type);
      }
      break;
    }
    case ContentSettingsType::STORAGE_ACCESS: {
      GURL requesting_url = primary_pattern.ToRepresentativeUrl();
      if (!requesting_url.is_valid()) {
        return;
      }
      // Only forward updates for sites which we are already tracking.
      net::SchemefulSite requesting_site(requesting_url);
      if (!content_settings_two_site_requests_[content_type].contains(
              requesting_site)) {
        return;
      }

      ContentSetting setting =
          map_->GetContentSetting(requesting_url, current_url, content_type);
      OnTwoSitePermissionChanged(content_type, requesting_site, setting);
      break;
    }
    default:
      break;
  }
}

void PageSpecificContentSettings::ClearContentSettingsChangedViaPageInfo() {
  content_settings_changed_via_page_info_.clear();
}

void PageSpecificContentSettings::BlockAllContentForTesting() {
  content_settings::ContentSettingsRegistry* registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();
    if (type != ContentSettingsType::MEDIASTREAM_MIC &&
        type != ContentSettingsType::MEDIASTREAM_CAMERA) {
      OnContentBlocked(type);
    }
  }

  // Media must be blocked separately, as the generic
  // PageSpecificContentSettings::OnContentBlocked does not apply to them.
  MicrophoneCameraState media_blocked{kMicrophoneAccessed, kMicrophoneBlocked,
                                      kCameraAccessed, kCameraBlocked};
  OnMediaStreamPermissionSet(page().GetMainDocument().GetLastCommittedURL(),
                             media_blocked);
}

void PageSpecificContentSettings::ContentSettingChangedViaPageInfo(
    ContentSettingsType type) {
  content_settings_changed_via_page_info_.insert(type);
}

bool PageSpecificContentSettings::HasContentSettingChangedViaPageInfo(
    ContentSettingsType type) const {
  return content_settings_changed_via_page_info_.find(type) !=
         content_settings_changed_via_page_info_.end();
}

bool PageSpecificContentSettings::HasAccessedTopics() const {
  return !GetAccessedTopics().empty();
}

std::vector<privacy_sandbox::CanonicalTopic>
PageSpecificContentSettings::GetAccessedTopics() const {
  if (accessed_topics_.empty() &&
      privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.Get() &&
      page().GetMainDocument().GetLastCommittedURL().host() == "example.com") {
    // TODO(crbug.com/40210776): Remove sample topic when API is ready.
    return {privacy_sandbox::CanonicalTopic(browsing_topics::Topic(3),
                                            kTopicsAPISampleDataTaxonomy),
            privacy_sandbox::CanonicalTopic(browsing_topics::Topic(4),
                                            kTopicsAPISampleDataTaxonomy)};
  }
  return {accessed_topics_.begin(), accessed_topics_.end()};
}

bool PageSpecificContentSettings::HasJoinedUserToInterestGroup() const {
  return !allowed_interest_group_api_.empty();
}

bool PageSpecificContentSettings::IsPagePrerendering() const {
  // We consider the Page to be prerendering iff
  // |updates_queued_during_prerender_| is non null. Note, the page already may
  // already have exited prerendering as |updates_queued_during_prerender_| is
  // flushed in DidFinishNavigation for the prerender activation but other
  // observers may come before this and call into here. In that case we'll still
  // queue their updates.
  return !!updates_queued_during_prerender_;
}

bool PageSpecificContentSettings::IsEmbeddedPage() const {
  return page().GetMainDocument().GetParentOrOuterDocument();
}

void PageSpecificContentSettings::OnPrerenderingPageActivation() {
  DCHECK(updates_queued_during_prerender_);
  for (auto& delegate_update :
       updates_queued_during_prerender_->delegate_updates) {
    std::move(delegate_update).Run();
  }

  if (updates_queued_during_prerender_->site_data_accessed) {
    // TODO(crbug.com/40269100): Re-attribute the
    // `access_details.is_from_primary_page`.
    WebContentsHandler::FromWebContents(GetWebContents())
        ->NotifySiteDataObservers(
            updates_queued_during_prerender_->access_details);
  }

  updates_queued_during_prerender_.reset();
}

void PageSpecificContentSettings::OnCapturingStateChanged(
    ContentSettingsType type,
    bool is_capturing) {
  DCHECK(type == ContentSettingsType::MEDIASTREAM_MIC ||
         type == ContentSettingsType::MEDIASTREAM_CAMERA);

  // If `is_capturing` is true, we should not hide an indicator. Erasing an
  // entry from `indicators_hiding_delay_timer_` will stop a dedicated timer.
  if (indicators_hiding_delay_timer_.contains(type) && is_capturing) {
    indicators_hiding_delay_timer_.erase(type);
  }

  // Check if media indicators should be hidden.
  if (is_capturing) {
    if (media_indicator_time_ == base::TimeTicks()) {
      media_indicator_time_ = base::TimeTicks::Now();
    }
    OnCapturingStateChangedInternal(type, is_capturing);
  } else {
    // When audio/video capturing is over, save a time so it can be stored in
    // HCSM.
    last_used_time_[type] = base::Time::Now();

    // Add a delay before the media indicator disappears.
    if ((type == ContentSettingsType::MEDIASTREAM_CAMERA &&
         !microphone_camera_state_.Has(kMicrophoneAccessed)) ||
        (type == ContentSettingsType::MEDIASTREAM_MIC &&
         !microphone_camera_state_.Has(kCameraAccessed))) {
      base::TimeDelta indicator_display_time =
          base::TimeTicks::Now() - media_indicator_time_;
      base::TimeDelta delay;
      base::TimeDelta min_delay;

      // A total duration of an indicator should never be less than
      // `kMediaIndicatorMinimumHoldDuration`.
      if (base::FeatureList::IsEnabled(
              content_settings::features::kLeftHandSideActivityIndicators)) {
        min_delay = kMediaIndicatorMinimumHoldDurationPhase2;
      } else {
        min_delay = kMediaIndicatorMinimumHoldDuration;
      }

      if (indicator_display_time < min_delay) {
        delay = min_delay - indicator_display_time;
        // `delay` should not be smaller than
        // `kMediaIndicatorHoldAfterUseDuration`.
        delay = std::max(min_delay - indicator_display_time,
                         kMediaIndicatorHoldAfterUseDuration);
      } else {
        delay = kMediaIndicatorHoldAfterUseDuration;
      }

      indicators_hiding_delay_timer_[type].Start(
          FROM_HERE, delay,
          base::BindOnce(
              &PageSpecificContentSettings::OnCapturingStateChangedInternal,
              weak_factory_.GetWeakPtr(), type, /*is_capturing=*/false));
    } else {
      OnCapturingStateChangedInternal(type, /*is_capturing=*/false);
    }
  }
}

void PageSpecificContentSettings::OnCapturingStateChangedInternal(
    ContentSettingsType type,
    bool is_capturing) {
  MicrophoneCameraStateFlags state =
      type == ContentSettingsType::MEDIASTREAM_MIC ? kMicrophoneAccessed
                                                   : kCameraAccessed;

  if (is_capturing) {
    microphone_camera_state_.Put(state);
    in_use_.insert(type);

    // Camera and Microphone share the same activity indicator view. If one of
    // them is in use, reset a blocked state for another as we cannot display
    // in-use and blocked indicator at once.
    // If permission is blocked on the system level, it should be reset as well
    // as the in use indicator has higher priority.
    auto t = type == ContentSettingsType::MEDIASTREAM_CAMERA
                 ? ContentSettingsType::MEDIASTREAM_MIC
                 : ContentSettingsType::MEDIASTREAM_CAMERA;
    if (media_blocked_indicator_timer_.contains(t) ||
        delegate_->IsBlockedOnSystemLevel(t)) {
      ResetMediaBlockedState(t, /*update_indicators=*/false);
    }
  } else {
    microphone_camera_state_.Remove(state);
    in_use_.erase(type);
  }

  // If `kMicrophoneAccessed` and `kCameraAccessed` not set, reset
  // `microphone_camera_state_`.
  if (!microphone_camera_state_.HasAny(
          {kMicrophoneAccessed, kCameraAccessed})) {
    microphone_camera_state_.Clear();
    media_indicator_time_ = base::TimeTicks();
  }

  MaybeUpdateLocationBar();
}

const base::Time PageSpecificContentSettings::GetLastUsedTime(
    ContentSettingsType type) {
  auto it = last_used_time_.find(type);
  if (it != last_used_time_.end()) {
    // After a recent usage HCSM will not have an updated last used time. HCSM
    // will be update in PSCS dtor.
    return it->second;
  }

  content_settings::SettingInfo info;
  map_->GetContentSetting(GetWebContents()->GetLastCommittedURL(),
                          GetWebContents()->GetLastCommittedURL(), type, &info);

  return info.metadata.last_used();
}

void PageSpecificContentSettings::OnActivityIndicatorBubbleOpened(
    ContentSettingsType type) {
  if (indicators_hiding_delay_timer_.contains(type) &&
      indicators_hiding_delay_timer_[type].IsRunning()) {
    indicators_hiding_delay_timer_[type].Stop();
  } else if (media_blocked_indicator_timer_.contains(type) &&
             media_blocked_indicator_timer_[type].IsRunning()) {
    media_blocked_indicator_timer_[type].Stop();
  }
}

void PageSpecificContentSettings::OnActivityIndicatorBubbleClosed(
    ContentSettingsType type) {
  if (indicators_hiding_delay_timer_.contains(type)) {
    // In use indicator timer was stopped, relaunch.
    indicators_hiding_delay_timer_[type].Start(
        FROM_HERE, kMediaIndicatorHoldAfterUseDuration,
        base::BindOnce(
            &PageSpecificContentSettings::OnCapturingStateChangedInternal,
            weak_factory_.GetWeakPtr(), type, /*is_capturing=*/false));
  } else if (media_blocked_indicator_timer_.contains(type)) {
    // Blocked indicator timer was stopped, relaunch.
    StartBlockedIndicatorTimer(type);
  }
}

bool PageSpecificContentSettings::IsIndicatorVisible(
    ContentSettingsType type) const {
  return visible_indicators_.contains(type);
}

void PageSpecificContentSettings::OnPermissionIndicatorShown(
    ContentSettingsType type) {
  visible_indicators_.insert(type);
}

void PageSpecificContentSettings::OnPermissionIndicatorHidden(
    ContentSettingsType type) {
  visible_indicators_.erase(type);
}

void PageSpecificContentSettings::StartBlockedIndicatorTimer(
    ContentSettingsType type) {
  base::TimeDelta blocked_indicator_delay;
  if (base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideActivityIndicators)) {
    blocked_indicator_delay = kBlockedMediaIndicatorDismissDelayPhase2;
  } else {
    blocked_indicator_delay = kBlockedMediaIndicatorDismissDelay;
  }
  media_blocked_indicator_timer_[type].Start(
      FROM_HERE, blocked_indicator_delay,
      base::BindOnce(&PageSpecificContentSettings::ResetMediaBlockedState,
                     weak_factory_.GetWeakPtr(), type,
                     /*update_indicators=*/true));
}

void PageSpecificContentSettings::ResetMediaBlockedState(
    ContentSettingsType type,
    bool update_indicators) {
  media_blocked_indicator_timer_.erase(type);

  if (type == ContentSettingsType::MEDIASTREAM_MIC) {
    microphone_camera_state_.Remove(kMicrophoneBlocked);
    microphone_camera_state_.Remove(kMicrophoneAccessed);

  } else {
    microphone_camera_state_.Remove(kCameraBlocked);
    microphone_camera_state_.Remove(kCameraAccessed);
  }

  if (update_indicators) {
    MaybeUpdateLocationBar();
  }
}

void PageSpecificContentSettings::MaybeNotifySiteDataObservers(
    const AccessDetails& access_details) {
  if (IsEmbeddedPage())
    return;
  if (IsPagePrerendering()) {
    updates_queued_during_prerender_->site_data_accessed = true;
    updates_queued_during_prerender_->access_details = access_details;
    return;
  }
  WebContentsHandler::FromWebContents(GetWebContents())
      ->NotifySiteDataObservers(access_details);
}

void PageSpecificContentSettings::MaybeUpdateLocationBar() {
  if (IsEmbeddedPage())
    return;
  if (IsPagePrerendering())
    return;
  delegate_->UpdateLocationBar();

  for (PermissionUsageObserver& observer : permission_usage_observers_) {
    observer.OnPermissionUsageChange();
  }
}

content::WebContents* PageSpecificContentSettings::GetWebContents() const {
  return content::WebContents::FromRenderFrameHost(&page().GetMainDocument());
}

PageSpecificContentSettings*
PageSpecificContentSettings::MaybeGetSyncedSettingsForPictureInPicture() {
  content::WebContents* web_contents =
      delegate_->MaybeGetSyncedWebContentsForPictureInPicture(GetWebContents());
  if (web_contents)
    return GetForFrame(web_contents->GetPrimaryMainFrame());
  return nullptr;
}

PAGE_USER_DATA_KEY_IMPL(PageSpecificContentSettings);

}  // namespace content_settings
