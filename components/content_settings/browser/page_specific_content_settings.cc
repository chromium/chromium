// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/page_specific_content_settings.h"

#include <list>
#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/browsing_data/content/cache_storage_helper.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/database_helper.h"
#include "components/browsing_data/content/file_system_helper.h"
#include "components/browsing_data/content/indexed_db_helper.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/browsing_data/content/service_worker_helper.h"
#include "components/browsing_data/content/shared_worker_helper.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/prefs/pref_service.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
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
  if (pscs)
    pscs->OnCookiesAccessed(details);
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
  if (pscs)
    pscs->OnServiceWorkerAccessed(scope, allowed);
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
                ->GetLastCommittedURL()
          : navigation_handle->GetURL();
  rules.FilterRulesByOutermostMainFrameURL(primary_url);

  mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent> agent;
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&agent);
  // TODO(crbug.com/1187618): We shouldn't be sending the primary patterns here
  // because: a) we have already filtered based on them and they are not needed
  // in the renderer, and b) they could leak the embedder origin to embedded
  // pages like fenced frames.
  agent->SendRendererContentSettingRules(std::move(rules));
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
                             content::RenderFrameHost* render_frame_host)
    : site_data_type(site_data_type),
      access_type(access_type),
      url(url),
      blocked_by_policy(blocked_by_policy),
      render_frame_host(render_frame_host) {}

AccessDetails::~AccessDetails() = default;

PageSpecificContentSettings::SiteDataObserver::SiteDataObserver(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  // Make sure the handler was attached to the WebContents as some UT might skip
  // this.
  auto* handler = WebContentsHandler::FromWebContents(web_contents_);
  if (handler)
    handler->AddSiteDataObserver(this);
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
      allowed_local_shared_objects_(
          GetWebContents()->GetPrimaryMainFrame()->GetStoragePartition(),
#if !BUILDFLAG(IS_ANDROID)
          // TODO(crbug.com/1404234): Remove the async local storage pathway
          // completely when the new dialog has launched.
          /*ignore_empty_localstorage=*/
          !base::FeatureList::IsEnabled(page_info::kPageSpecificSiteDataDialog),
#else
          /*ignore_empty_localstorage=*/true,
#endif
          delegate_->GetAdditionalFileSystemTypes(),
          delegate_->GetIsDeletionDisabledCallback()),
      blocked_local_shared_objects_(
          GetWebContents()->GetPrimaryMainFrame()->GetStoragePartition(),
          /*ignore_empty_localstorage=*/false,
          delegate_->GetAdditionalFileSystemTypes(),
          delegate_->GetIsDeletionDisabledCallback()),
      allowed_browsing_data_model_(BrowsingDataModel::BuildEmpty(
          GetWebContents()->GetPrimaryMainFrame()->GetStoragePartition(),
          delegate_->CreateBrowsingDataModelDelegate())),
      blocked_browsing_data_model_(BrowsingDataModel::BuildEmpty(
          GetWebContents()->GetPrimaryMainFrame()->GetStoragePartition(),
          delegate_->CreateBrowsingDataModelDelegate())),
      microphone_camera_state_(MICROPHONE_CAMERA_NOT_ACCESSED) {
  observation_.Observe(map_.get());
  if (page.GetMainDocument().GetLifecycleState() ==
      LifecycleState::kPrerendering) {
    updates_queued_during_prerender_ = std::make_unique<PendingUpdates>();
  }
}

PageSpecificContentSettings::~PageSpecificContentSettings() = default;

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
void PageSpecificContentSettings::StorageAccessed(StorageType storage_type,
                                                  int render_process_id,
                                                  int render_frame_id,
                                                  const GURL& url,
                                                  bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (DelayUntilCommitIfNecessary(
          rfh, &PageSpecificContentSettings::StorageAccessed, storage_type,
          render_process_id, render_frame_id, url, blocked_by_policy))
    return;
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if (settings)
    settings->OnStorageAccessed(storage_type, url, blocked_by_policy, rfh);
}

// static
void PageSpecificContentSettings::BrowsingDataAccessed(
    content::RenderFrameHost* rfh,
    BrowsingDataModel::DataKey data_key,
    BrowsingDataModel::StorageType storage_type,
    bool blocked) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if(settings)
    settings->OnBrowsingDataAccessed(data_key, storage_type, blocked);
}

// static
void PageSpecificContentSettings::ContentBlocked(int render_process_id,
                                                 int render_frame_id,
                                                 ContentSettingsType type) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (DelayUntilCommitIfNecessary(rfh,
                                  &PageSpecificContentSettings::ContentBlocked,
                                  render_process_id, render_frame_id, type))
    return;
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if (settings)
    settings->OnContentBlocked(type);
}

// static
void PageSpecificContentSettings::SharedWorkerAccessed(
    int render_process_id,
    int render_frame_id,
    const GURL& worker_url,
    const std::string& name,
    const blink::StorageKey& storage_key,
    bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings = GetForFrame(
      content::RenderFrameHost::FromID(render_process_id, render_frame_id));
  if (settings)
    settings->OnSharedWorkerAccessed(worker_url, name, storage_key,
                                     blocked_by_policy);
}

// static
void PageSpecificContentSettings::InterestGroupJoined(
    content::RenderFrameHost* rfh,
    const url::Origin& api_origin,
    bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if (settings)
    settings->OnInterestGroupJoined(api_origin, blocked_by_policy);
}

// static
void PageSpecificContentSettings::TopicAccessed(
    content::RenderFrameHost* rfh,
    const url::Origin& api_origin,
    bool blocked_by_policy,
    privacy_sandbox::CanonicalTopic topic) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings = GetForFrame(rfh);
  if (settings)
    settings->OnTopicAccessed(api_origin, blocked_by_policy, topic);
}

// static
content::WebContentsObserver*
PageSpecificContentSettings::GetWebContentsObserverForTest(
    content::WebContents* web_contents) {
  return WebContentsHandler::FromWebContents(web_contents);
}

bool PageSpecificContentSettings::IsContentBlocked(
    ContentSettingsType content_type) const {
  DCHECK_NE(ContentSettingsType::NOTIFICATIONS, content_type)
      << "Notifications settings handled by "
      << "ContentSettingsNotificationsImageModel";
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
      content_type == ContentSettingsType::GEOLOCATION) {
    const auto& it = content_settings_status_.find(content_type);
    if (it != content_settings_status_.end())
      return it->second.blocked;
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
      content_type != ContentSettingsType::GEOLOCATION) {
    return false;
  }

  const auto& it = content_settings_status_.find(content_type);
  if (it != content_settings_status_.end())
    return it->second.allowed;
  return false;
}

std::map<net::SchemefulSite, /*is_allowed*/ bool>
PageSpecificContentSettings::GetTwoSiteRequests(
    ContentSettingsType content_type) {
  std::map<net::SchemefulSite, /*is_allowed*/ bool> result;
  for (const auto& entry : content_settings_two_site_requests_[content_type]) {
    result[entry.first] = entry.second.allowed;
  }
  return result;
}

void PageSpecificContentSettings::OnContentBlocked(ContentSettingsType type) {
  DCHECK(type != ContentSettingsType::MEDIASTREAM_MIC &&
         type != ContentSettingsType::MEDIASTREAM_CAMERA)
      << "Media stream settings handled by OnMediaStreamPermissionSet";
  if (!content_settings::ContentSettingsRegistry::GetInstance()->Get(type))
    return;

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
  if (type == ContentSettingsType::SENSORS)
    must_reset_blocked_status = true;

  // If this content settings is for the PiP window, must reset |blocked| status
  // since it will not be updated in OnContentSettingChanged() like the normal
  // browser window. We need the status to be precise to display the UI.
  auto* synced_pccs = MaybeGetSyncedSettingsForPictureInPicture();
  if (synced_pccs)
    must_reset_blocked_status = true;

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

void PageSpecificContentSettings::OnTwoSitePermissionRequested(
    ContentSettingsType type,
    net::SchemefulSite requesting_site,
    bool is_allowed) {
  ContentSettingsStatus& status =
      content_settings_two_site_requests_[type][requesting_site];
  if (is_allowed) {
    status.allowed = true;
  } else {
    status.blocked = true;
  }

  MaybeUpdateLocationBar();
}

namespace {
void AddToContainer(browsing_data::LocalSharedObjectsContainer& container,
                    StorageType storage_type,
                    const GURL& url) {
  url::Origin origin = url::Origin::Create(url);
  switch (storage_type) {
    case StorageType::DATABASE:
      container.databases()->Add(origin);
      return;
    case StorageType::LOCAL_STORAGE:
      // TODO(https://crbug.com/1199077): Pass the real StorageKey into this
      // function directly.
      container.local_storages()->Add(
          blink::StorageKey::CreateFirstParty(origin));
      return;
    case StorageType::SESSION_STORAGE:
      // TODO(https://crbug.com/1199077): Pass the real StorageKey into this
      // function directly.
      container.session_storages()->Add(
          blink::StorageKey::CreateFirstParty(origin));
      return;
    case StorageType::INDEXED_DB:
      // TODO(https://crbug.com/1199077): Pass the real StorageKey into this
      // function directly.
      container.indexed_dbs()->Add(blink::StorageKey::CreateFirstParty(origin));
      return;
    case StorageType::CACHE:
      container.cache_storages()->Add(
          blink::StorageKey::CreateFirstParty(origin));
      return;
    case StorageType::FILE_SYSTEM:
      container.file_systems()->Add(origin);
      return;
    case StorageType::WEB_LOCKS:
      NOTREACHED();
      return;
  }
}
}  // namespace

void PageSpecificContentSettings::OnStorageAccessed(
    StorageType storage_type,
    const GURL& url,
    bool blocked_by_policy,
    content::RenderFrameHost* rfh,
    content::Page* originating_page) {
  originating_page = originating_page ? originating_page : &page();
  if (blocked_by_policy) {
    AddToContainer(blocked_local_shared_objects_, storage_type, url);
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    AddToContainer(allowed_local_shared_objects_, storage_type, url);
    NotifyDelegate(&Delegate::OnStorageAccessAllowed, storage_type,
                   url::Origin::Create(url), std::ref(*originating_page));
    OnContentAllowed(ContentSettingsType::COOKIES);
  }

  MaybeUpdateParent(&PageSpecificContentSettings::OnStorageAccessed,
                    storage_type, url, blocked_by_policy, rfh,
                    originating_page);

  AccessDetails access_details{SiteDataType::kStorage, AccessType::kUnknown,
                               url, blocked_by_policy, rfh};

  MaybeNotifySiteDataObservers(access_details);
}

void PageSpecificContentSettings::OnCookiesAccessed(
    const content::CookieAccessDetails& details,
    content::Page* originating_page) {
  originating_page = originating_page ? originating_page : &page();
  if (details.cookie_list.empty())
    return;
  if (details.blocked_by_policy) {
    blocked_local_shared_objects_.cookies()->AddCookies(details);
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    allowed_local_shared_objects_.cookies()->AddCookies(details);
    OnContentAllowed(ContentSettingsType::COOKIES);
    NotifyDelegate(&Delegate::OnCookieAccessAllowed, details.cookie_list,
                   std::ref(*originating_page));
  }

  MaybeUpdateParent(&PageSpecificContentSettings::OnCookiesAccessed, details,
                    originating_page);

  AccessDetails access_details{
      SiteDataType::kCookies,
      details.type == network::mojom::CookieAccessDetails::Type::kChange
          ? AccessType::kWrite
          : AccessType::kRead,
      details.url, details.blocked_by_policy, nullptr};

  MaybeNotifySiteDataObservers(access_details);
}

void PageSpecificContentSettings::OnServiceWorkerAccessed(
    const GURL& scope,
    content::AllowServiceWorkerResult allowed,
    content::Page* originating_page) {
  DCHECK(scope.is_valid());
  originating_page = originating_page ? originating_page : &page();
  if (allowed) {
    allowed_local_shared_objects_.service_workers()->Add(
        url::Origin::Create(scope));
    NotifyDelegate(&Delegate::OnServiceWorkerAccessAllowed,
                   url::Origin::Create(scope), std::ref(*originating_page));
  } else {
    blocked_local_shared_objects_.service_workers()->Add(
        url::Origin::Create(scope));
  }

  if (allowed.javascript_blocked_by_policy()) {
    OnContentBlocked(ContentSettingsType::JAVASCRIPT);
  } else {
    OnContentAllowed(ContentSettingsType::JAVASCRIPT);
  }
  if (allowed.cookies_blocked_by_policy()) {
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    OnContentAllowed(ContentSettingsType::COOKIES);
  }

  MaybeUpdateParent(&PageSpecificContentSettings::OnServiceWorkerAccessed,
                    scope, allowed, originating_page);
}

void PageSpecificContentSettings::OnSharedWorkerAccessed(
    const GURL& worker_url,
    const std::string& name,
    const blink::StorageKey& storage_key,
    bool blocked_by_policy) {
  DCHECK(worker_url.is_valid());
  if (blocked_by_policy) {
    blocked_local_shared_objects_.shared_workers()->AddSharedWorker(
        worker_url, name, storage_key);
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    allowed_local_shared_objects_.shared_workers()->AddSharedWorker(
        worker_url, name, storage_key);
    OnContentAllowed(ContentSettingsType::COOKIES);
  }
  MaybeUpdateParent(&PageSpecificContentSettings::OnSharedWorkerAccessed,
                    worker_url, name, storage_key, blocked_by_policy);
}

void PageSpecificContentSettings::OnInterestGroupJoined(
    const url::Origin& api_origin,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_interest_group_api_.push_back(api_origin);
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    allowed_interest_group_api_.push_back(api_origin);
    OnContentAllowed(ContentSettingsType::COOKIES);
  }
  MaybeUpdateParent(&PageSpecificContentSettings::OnInterestGroupJoined,
                    api_origin, blocked_by_policy);

  // Joining an interest is by default modifying data so this is considered an
  // `AccessType::kWrite`.
  AccessDetails access_details{SiteDataType::kInterestGroup, AccessType::kWrite,
                               api_origin.GetURL(), blocked_by_policy, nullptr};
  MaybeNotifySiteDataObservers(access_details);
}

void PageSpecificContentSettings::OnTopicAccessed(
    const url::Origin& api_origin,
    bool blocked_by_policy,
    privacy_sandbox::CanonicalTopic topic) {
  // TODO(crbug.com/1286276): Add URL and Topic to local_shared_objects?
  accessed_topics_.insert(topic);
  MaybeUpdateParent(&PageSpecificContentSettings::OnTopicAccessed, api_origin,
                    blocked_by_policy, topic);
}

void PageSpecificContentSettings::OnTrustTokenAccessed(
    const url::Origin& api_origin,
    bool blocked) {
  // The size isn't relevant here and won't be displayed in the UI.
  const int kTrustTokenSize = 0;
  auto& model =
      blocked ? blocked_browsing_data_model_ : allowed_browsing_data_model_;
  model->AddBrowsingData(api_origin,
                         BrowsingDataModel::StorageType::kTrustTokens,
                         kTrustTokenSize);
  if (blocked) {
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    OnContentAllowed(ContentSettingsType::COOKIES);
  }
  MaybeUpdateParent(&PageSpecificContentSettings::OnTrustTokenAccessed,
                    api_origin, blocked);

  AccessDetails access_details{SiteDataType::kTrustToken, AccessType::kUnknown,
                               api_origin.GetURL(), blocked, nullptr};

  MaybeNotifySiteDataObservers(access_details);
}

void PageSpecificContentSettings::OnBrowsingDataAccessed(
    BrowsingDataModel::DataKey data_key,
    BrowsingDataModel::StorageType storage_type,
    bool blocked) {
  auto& model =
      blocked ? blocked_browsing_data_model_ : allowed_browsing_data_model_;

  // The size isn't relevant here and won't be displayed in the UI.
  model->AddBrowsingData(data_key, storage_type, /*storage_size=*/0);
  if (blocked) {
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    OnContentAllowed(ContentSettingsType::COOKIES);
  }
  MaybeUpdateParent(&PageSpecificContentSettings::OnBrowsingDataAccessed,
                    data_key, storage_type, blocked);

  // TODO(njeunje): Look into populating an actual url for this access details.
  // Could be obtained from the `data_key`.
  AccessDetails access_details{SiteDataType::kUnknown, AccessType::kUnknown,
                               GURL(), blocked, nullptr};
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
  return microphone_camera_state_ | delegate_->GetMicrophoneCameraState();
}

bool PageSpecificContentSettings::IsMicrophoneCameraStateChanged() const {
  if ((microphone_camera_state_ & MICROPHONE_ACCESSED) &&
      ((microphone_camera_state_ & MICROPHONE_BLOCKED)
           ? !IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC)
           : !IsContentAllowed(ContentSettingsType::MEDIASTREAM_MIC)))
    return true;

  if ((microphone_camera_state_ & CAMERA_ACCESSED) &&
      ((microphone_camera_state_ & CAMERA_BLOCKED)
           ? !IsContentBlocked(ContentSettingsType::MEDIASTREAM_CAMERA)
           : !IsContentAllowed(ContentSettingsType::MEDIASTREAM_CAMERA)))
    return true;

  return delegate_->IsMicrophoneCameraStateChanged(
      microphone_camera_state_, media_stream_selected_audio_device(),
      media_stream_selected_video_device());
}

void PageSpecificContentSettings::OnMediaStreamPermissionSet(
    const GURL& request_origin,
    MicrophoneCameraState new_microphone_camera_state,
    const std::string& media_stream_selected_audio_device,
    const std::string& media_stream_selected_video_device,
    const std::string& media_stream_requested_audio_device,
    const std::string& media_stream_requested_video_device) {
  DCHECK(!IsEmbeddedPage());
  media_stream_access_origin_ = request_origin;

  if (new_microphone_camera_state & MICROPHONE_ACCESSED) {
    media_stream_requested_audio_device_ = media_stream_requested_audio_device;
    media_stream_selected_audio_device_ = media_stream_selected_audio_device;
    bool mic_blocked = (new_microphone_camera_state & MICROPHONE_BLOCKED) != 0;
    ContentSettingsStatus& status =
        content_settings_status_[ContentSettingsType::MEDIASTREAM_MIC];
    if (!status.allowed && !mic_blocked) {
      NotifyDelegate(&Delegate::OnContentAllowed,
                     ContentSettingsType::MEDIASTREAM_MIC);
    }
    status.allowed = !mic_blocked;
    status.blocked = mic_blocked;
  }

  if (new_microphone_camera_state & CAMERA_ACCESSED) {
    media_stream_requested_video_device_ = media_stream_requested_video_device;
    media_stream_selected_video_device_ = media_stream_selected_video_device;
    bool cam_blocked = (new_microphone_camera_state & CAMERA_BLOCKED) != 0;
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
    microphone_camera_state_ = new_microphone_camera_state;
    if (!is_updating_synced_pscs_) {
      base::AutoReset<bool> auto_reset(&is_updating_synced_pscs_, true);
      if (auto* synced_pccs = MaybeGetSyncedSettingsForPictureInPicture()) {
        synced_pccs->OnMediaStreamPermissionSet(
            request_origin, new_microphone_camera_state,
            media_stream_selected_audio_device,
            media_stream_selected_video_device,
            media_stream_requested_audio_device,
            media_stream_requested_video_device);
      }
    }
    MaybeUpdateLocationBar();
  }
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
  if (!primary_pattern.Matches(current_url)) {
    return;
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
      if (geolocation_setting == CONTENT_SETTING_ALLOW)
        geolocation_was_just_granted_on_site_level_ = true;
      [[fallthrough]];
    }
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
  MicrophoneCameraStateFlags media_blocked =
      static_cast<MicrophoneCameraStateFlags>(
          PageSpecificContentSettings::MICROPHONE_ACCESSED |
          PageSpecificContentSettings::MICROPHONE_BLOCKED |
          PageSpecificContentSettings::CAMERA_ACCESSED |
          PageSpecificContentSettings::CAMERA_BLOCKED);
  OnMediaStreamPermissionSet(page().GetMainDocument().GetLastCommittedURL(),
                             media_blocked, std::string(), std::string(),
                             std::string(), std::string());
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
      (privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting
           .Get() ||
       privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting
           .Get()) &&
      page().GetMainDocument().GetLastCommittedURL().host() == "example.com") {
    // TODO(crbug.com/1286276): Remove sample topic when API is ready.
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
    WebContentsHandler::FromWebContents(GetWebContents())
        ->NotifySiteDataObservers(
            updates_queued_during_prerender_->access_details);
  }

  updates_queued_during_prerender_.reset();
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
