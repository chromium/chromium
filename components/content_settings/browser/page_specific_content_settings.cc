// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/page_specific_content_settings.h"

#include <list>
#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/browsing_data/content/appcache_helper.h"
#include "components/browsing_data/content/cache_storage_helper.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/database_helper.h"
#include "components/browsing_data/content/file_system_helper.h"
#include "components/browsing_data/content/indexed_db_helper.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/browsing_data/content/service_worker_helper.h"
#include "components/browsing_data/content/shared_worker_helper.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_constants.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/origin.h"

using content::BrowserThread;

namespace content_settings {
namespace {

bool ShouldSendUpdatedContentSettingsRulesToRenderer(
    ContentSettingsType content_type) {
  // ContentSettingsType::DEFAULT signals that multiple content settings may
  // have been updated, e.g. by the PolicyProvider. This should always be sent
  // to the renderer in case a relevant setting is updated.
  if (content_type == ContentSettingsType::DEFAULT)
    return true;

  return RendererContentSettingRules::IsRendererContentSetting((content_type));
}

void MaybeSendRendererContentSettingsRules(
    content::RenderFrameHost* rfh,
    const HostContentSettingsMap* map,
    PageSpecificContentSettings::Delegate* delegate) {
  DCHECK_EQ(rfh, rfh->GetMainFrame());
  // Only send a message to the renderer if it is initialised and not dead.
  // Otherwise, the IPC messages will be queued in the browser process,
  // potentially causing large memory leaks. See https://crbug.com/875937.
  content::RenderProcessHost* process = rfh->GetProcess();
  if (!process->IsInitializedAndNotDead())
    return;

  RendererContentSettingRules rules;
  GetRendererContentSettingRules(map, &rules);
  delegate->SetContentSettingRules(process, rules);
}

bool WillNavigationCreateNewPageSpecificContentSettingsOnCommit(
    content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInMainFrame() &&
         !navigation_handle->IsSameDocument() &&
         !navigation_handle->IsServedFromBackForwardCache();
}

}  // namespace

PageSpecificContentSettings::SiteDataObserver::SiteDataObserver(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  // Make sure the handler was attached to the WebContents as some UT might skip
  // this.
  auto* handler =
      PageSpecificContentSettings::WebContentsHandler::FromWebContents(
          web_contents_);
  if (handler)
    handler->AddSiteDataObserver(this);
}

PageSpecificContentSettings::SiteDataObserver::~SiteDataObserver() {
  if (!web_contents_)
    return;
  auto* handler =
      PageSpecificContentSettings::WebContentsHandler::FromWebContents(
          web_contents_);
  if (handler)
    handler->RemoveSiteDataObserver(this);
}

void PageSpecificContentSettings::SiteDataObserver::WebContentsDestroyed() {
  web_contents_ = nullptr;
}

// static
void PageSpecificContentSettings::WebContentsHandler::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<Delegate> delegate) {
  DCHECK(web_contents);
  if (PageSpecificContentSettings::WebContentsHandler::FromWebContents(
          web_contents)) {
    return;
  }

  web_contents->SetUserData(
      PageSpecificContentSettings::WebContentsHandler::UserDataKey(),
      base::WrapUnique(new PageSpecificContentSettings::WebContentsHandler(
          web_contents, std::move(delegate))));
}

PageSpecificContentSettings::WebContentsHandler::WebContentsHandler(
    content::WebContents* web_contents,
    std::unique_ptr<Delegate> delegate)
    : WebContentsObserver(web_contents),
      delegate_(std::move(delegate)),
      map_(delegate_->GetSettingsMap()) {
  DCHECK(!PageSpecificContentSettings::GetForCurrentDocument(
      web_contents->GetMainFrame()));
  content::SetRenderDocumentHostUserData(
      web_contents->GetMainFrame(), PageSpecificContentSettings::UserDataKey(),
      base::WrapUnique(
          new PageSpecificContentSettings(*this, delegate_.get())));
}

PageSpecificContentSettings::WebContentsHandler::~WebContentsHandler() {
  for (SiteDataObserver& observer : observer_list_)
    observer.WebContentsDestroyed();
}

void PageSpecificContentSettings::WebContentsHandler::
    TransferNavigationContentSettingsToCommittedDocument(
        const InflightNavigationContentSettings& navigation_settings,
        content::RenderFrameHost* rfh) {
  for (const auto& cookie_access : navigation_settings.cookie_accesses) {
    OnCookiesAccessed(rfh, cookie_access);
  }
  for (const auto& service_worker_access :
       navigation_settings.service_worker_accesses) {
    OnServiceWorkerAccessed(rfh, service_worker_access.first,
                            service_worker_access.second);
  }
}

void PageSpecificContentSettings::WebContentsHandler::OnCookiesAccessed(
    content::NavigationHandle* navigation,
    const content::CookieAccessDetails& details) {
  auto it = inflight_navigation_settings_.find(navigation);
  if (it != inflight_navigation_settings_.end()) {
    it->second.cookie_accesses.push_back(details);
    return;
  }
  // TODO(carlscab): We should be able to
  // DHECK(!WillNavigationCreateNewPageSpecificContentSettingsOnCommit) here,
  // but there is still code that starts a navigation before attaching the tab
  // helpers in DevConsole related code. So we miss the DidStartNavigation event
  // for those navigations. (https://crbug.com/1095576)
  OnCookiesAccessed(web_contents()->GetMainFrame(), details);
}

void PageSpecificContentSettings::WebContentsHandler::OnCookiesAccessed(
    content::RenderFrameHost* rfh,
    const content::CookieAccessDetails& details) {
  auto* tscs =
      PageSpecificContentSettings::GetForCurrentDocument(rfh->GetMainFrame());
  if (tscs)
    tscs->OnCookiesAccessed(details);
}

void PageSpecificContentSettings::WebContentsHandler::OnServiceWorkerAccessed(
    content::NavigationHandle* navigation,
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  DCHECK(scope.is_valid());

  auto it = inflight_navigation_settings_.find(navigation);
  if (it != inflight_navigation_settings_.end()) {
    it->second.service_worker_accesses.emplace_back(
        std::make_pair(scope, allowed));
    return;
  }
  // TODO(carlscab): We should be able to
  // DHECK(!WillNavigationCreateNewPageSpecificContentSettingsOnCommit) here,
  // but there is still code that starts a navigation before attaching the tab
  // helpers in DevConsole related code. So we miss the DidStartNavigation event
  // for those navigations.
  OnServiceWorkerAccessed(web_contents()->GetMainFrame(), scope, allowed);
}

void PageSpecificContentSettings::WebContentsHandler::OnServiceWorkerAccessed(
    content::RenderFrameHost* frame,
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  auto* tscs =
      PageSpecificContentSettings::GetForCurrentDocument(frame->GetMainFrame());
  if (tscs)
    tscs->OnServiceWorkerAccessed(scope, allowed);
}

void PageSpecificContentSettings::WebContentsHandler::
    RenderFrameForInterstitialPageCreated(
        content::RenderFrameHost* render_frame_host) {
  // We want to tell the renderer-side code to ignore content settings for this
  // page.
  mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent>
      content_settings_agent;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &content_settings_agent);
  content_settings_agent->SetAsInterstitial();
}

void PageSpecificContentSettings::WebContentsHandler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!WillNavigationCreateNewPageSpecificContentSettingsOnCommit(
          navigation_handle)) {
    return;
  }

  inflight_navigation_settings_.insert(
      std::make_pair(navigation_handle, InflightNavigationContentSettings()));
}

void PageSpecificContentSettings::WebContentsHandler::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!WillNavigationCreateNewPageSpecificContentSettingsOnCommit(
          navigation_handle)) {
    return;
  }

  // There may be content settings that were updated for the navigated URL.
  // These would not have been sent before if we're navigating cross-origin.
  // Ensure up to date rules are sent before navigation commits.
  MaybeSendRendererContentSettingsRules(
      navigation_handle->GetWebContents()->GetMainFrame(), map_,
      delegate_.get());
}

void PageSpecificContentSettings::WebContentsHandler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!WillNavigationCreateNewPageSpecificContentSettingsOnCommit(
          navigation_handle)) {
    return;
  }

  if (!navigation_handle->HasCommitted()) {
    inflight_navigation_settings_.erase(navigation_handle);
    return;
  }

  auto tscs =
      base::WrapUnique(new PageSpecificContentSettings(*this, delegate_.get()));

  // TODO(carlscab): This sort of internal. Maybe add a
  // RenderDocumentHostUserData::Create(RenderFrameHost* rfh, Params...)
  content::SetRenderDocumentHostUserData(
      navigation_handle->GetRenderFrameHost(),
      PageSpecificContentSettings::UserDataKey(), std::move(tscs));

  auto it = inflight_navigation_settings_.find(navigation_handle);
  if (it != inflight_navigation_settings_.end()) {
    TransferNavigationContentSettingsToCommittedDocument(
        it->second, navigation_handle->GetRenderFrameHost());
    inflight_navigation_settings_.erase(it);
  }

  delegate_->UpdateLocationBar();
}

void PageSpecificContentSettings::WebContentsHandler::AppCacheAccessed(
    const GURL& manifest_url,
    bool blocked_by_policy) {
  auto* tscs = PageSpecificContentSettings::GetForCurrentDocument(
      web_contents()->GetMainFrame());
  if (tscs)
    tscs->AppCacheAccessed(manifest_url, blocked_by_policy);
}

void PageSpecificContentSettings::WebContentsHandler::AddSiteDataObserver(
    SiteDataObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PageSpecificContentSettings::WebContentsHandler::RemoveSiteDataObserver(
    SiteDataObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void PageSpecificContentSettings::WebContentsHandler::
    NotifySiteDataObservers() {
  for (SiteDataObserver& observer : observer_list_)
    observer.OnSiteDataAccessed();
}

PageSpecificContentSettings::WebContentsHandler::
    InflightNavigationContentSettings::InflightNavigationContentSettings() =
        default;
PageSpecificContentSettings::WebContentsHandler::
    InflightNavigationContentSettings::InflightNavigationContentSettings(
        const InflightNavigationContentSettings&) = default;
PageSpecificContentSettings::WebContentsHandler::
    InflightNavigationContentSettings::InflightNavigationContentSettings(
        InflightNavigationContentSettings&&) = default;

PageSpecificContentSettings::WebContentsHandler::
    InflightNavigationContentSettings::~InflightNavigationContentSettings() =
        default;

PageSpecificContentSettings::WebContentsHandler::
    InflightNavigationContentSettings&
    PageSpecificContentSettings::WebContentsHandler::
        InflightNavigationContentSettings::operator=(
            InflightNavigationContentSettings&&) = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageSpecificContentSettings::WebContentsHandler)

PageSpecificContentSettings::PageSpecificContentSettings(
    PageSpecificContentSettings::WebContentsHandler& handler,
    Delegate* delegate)
    : handler_(handler),
      main_frame_(handler_.web_contents()->GetMainFrame()),
      delegate_(delegate),
      visible_url_(handler_.web_contents()->GetVisibleURL()),
      map_(delegate_->GetSettingsMap()),
      allowed_local_shared_objects_(
          handler_.web_contents()->GetBrowserContext(),
          delegate_->GetAdditionalFileSystemTypes(),
          delegate_->GetIsDeletionDisabledCallback()),
      blocked_local_shared_objects_(
          handler_.web_contents()->GetBrowserContext(),
          delegate_->GetAdditionalFileSystemTypes(),
          delegate_->GetIsDeletionDisabledCallback()),
      load_plugins_link_enabled_(true),
      microphone_camera_state_(MICROPHONE_CAMERA_NOT_ACCESSED) {
  observer_.Add(map_);
}

PageSpecificContentSettings::~PageSpecificContentSettings() = default;

// static
void PageSpecificContentSettings::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<Delegate> delegate) {
  PageSpecificContentSettings::WebContentsHandler::CreateForWebContents(
      web_contents, std::move(delegate));
}

// static
void PageSpecificContentSettings::DeleteForWebContentsForTest(
    content::WebContents* web_contents) {
  if (web_contents->GetMainFrame()) {
    PageSpecificContentSettings::DeleteForCurrentDocument(
        web_contents->GetMainFrame());
  }

  web_contents->RemoveUserData(
      PageSpecificContentSettings::WebContentsHandler::UserDataKey());
}

// static
PageSpecificContentSettings* PageSpecificContentSettings::GetForFrame(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetForFrame(
      content::RenderFrameHost::FromID(render_process_id, render_frame_id));
}

// static
PageSpecificContentSettings* PageSpecificContentSettings::GetForFrame(
    content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return rfh ? PageSpecificContentSettings::GetForCurrentDocument(
                   rfh->GetMainFrame())
             : nullptr;
}

// static
PageSpecificContentSettings::Delegate*
PageSpecificContentSettings::GetDelegateForWebContents(
    content::WebContents* web_contents) {
  auto* handler =
      PageSpecificContentSettings::WebContentsHandler::FromWebContents(
          web_contents);
  return handler ? handler->delegate() : nullptr;
}

// static
void PageSpecificContentSettings::WebDatabaseAccessed(int render_process_id,
                                                      int render_frame_id,
                                                      const GURL& url,
                                                      bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings =
      GetForFrame(render_process_id, render_frame_id);
  if (settings)
    settings->OnWebDatabaseAccessed(url, blocked_by_policy);
}

// static
void PageSpecificContentSettings::IndexedDBAccessed(int render_process_id,
                                                    int render_frame_id,
                                                    const GURL& url,
                                                    bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings =
      GetForFrame(render_process_id, render_frame_id);
  if (settings)
    settings->OnIndexedDBAccessed(url, blocked_by_policy);
}

// static
void PageSpecificContentSettings::CacheStorageAccessed(int render_process_id,
                                                       int render_frame_id,
                                                       const GURL& url,
                                                       bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings =
      GetForFrame(render_process_id, render_frame_id);
  if (settings)
    settings->OnCacheStorageAccessed(url, blocked_by_policy);
}

// static
void PageSpecificContentSettings::FileSystemAccessed(int render_process_id,
                                                     int render_frame_id,
                                                     const GURL& url,
                                                     bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings =
      GetForFrame(render_process_id, render_frame_id);
  if (settings)
    settings->OnFileSystemAccessed(url, blocked_by_policy);
}

// static
void PageSpecificContentSettings::SharedWorkerAccessed(
    int render_process_id,
    int render_frame_id,
    const GURL& worker_url,
    const std::string& name,
    const url::Origin& constructor_origin,
    bool blocked_by_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PageSpecificContentSettings* settings =
      GetForFrame(render_process_id, render_frame_id);
  if (settings)
    settings->OnSharedWorkerAccessed(worker_url, name, constructor_origin,
                                     blocked_by_policy);
}

// static
content::WebContentsObserver*
PageSpecificContentSettings::GetWebContentsObserverForTest(
    content::WebContents* web_contents) {
  return PageSpecificContentSettings::WebContentsHandler::FromWebContents(
      web_contents);
}

bool PageSpecificContentSettings::IsContentBlocked(
    ContentSettingsType content_type) const {
  DCHECK_NE(ContentSettingsType::NOTIFICATIONS, content_type)
      << "Notifications settings handled by "
      << "ContentSettingsNotificationsImageModel";
  DCHECK_NE(ContentSettingsType::AUTOMATIC_DOWNLOADS, content_type)
      << "Automatic downloads handled by DownloadRequestLimiter";

  if (content_type == ContentSettingsType::IMAGES ||
      content_type == ContentSettingsType::JAVASCRIPT ||
      content_type == ContentSettingsType::PLUGINS ||
      content_type == ContentSettingsType::COOKIES ||
      content_type == ContentSettingsType::POPUPS ||
      content_type == ContentSettingsType::MIXEDSCRIPT ||
      content_type == ContentSettingsType::MEDIASTREAM_MIC ||
      content_type == ContentSettingsType::MEDIASTREAM_CAMERA ||
      content_type == ContentSettingsType::PPAPI_BROKER ||
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

  // This method currently only returns meaningful values for the types listed
  // below.
  if (content_type != ContentSettingsType::COOKIES &&
      content_type != ContentSettingsType::MEDIASTREAM_MIC &&
      content_type != ContentSettingsType::MEDIASTREAM_CAMERA &&
      content_type != ContentSettingsType::PPAPI_BROKER &&
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

void PageSpecificContentSettings::OnContentBlocked(ContentSettingsType type) {
  DCHECK(type != ContentSettingsType::MEDIASTREAM_MIC &&
         type != ContentSettingsType::MEDIASTREAM_CAMERA)
      << "Media stream settings handled by OnMediaStreamPermissionSet";
  if (!content_settings::ContentSettingsRegistry::GetInstance()->Get(type))
    return;
  ContentSettingsStatus& status = content_settings_status_[type];

  if (!status.blocked) {
    status.blocked = true;
    delegate_->UpdateLocationBar();
    delegate_->OnContentBlocked(type);
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

#if defined(OS_ANDROID)
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
    delegate_->OnContentAllowed(type);
  }

  if (access_changed)
    delegate_->UpdateLocationBar();
}

void PageSpecificContentSettings::OnDomStorageAccessed(const GURL& url,
                                                       bool local,
                                                       bool blocked_by_policy) {
  browsing_data::LocalSharedObjectsContainer& container =
      blocked_by_policy ? blocked_local_shared_objects_
                        : allowed_local_shared_objects_;
  browsing_data::CannedLocalStorageHelper* helper =
      local ? container.local_storages() : container.session_storages();
  helper->Add(url::Origin::Create(url));

  if (blocked_by_policy) {
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    delegate_->OnDomStorageAccessAllowed(url::Origin::Create(url));
    OnContentAllowed(ContentSettingsType::COOKIES);
  }

  handler_.NotifySiteDataObservers();
}

void PageSpecificContentSettings::OnCookiesAccessed(
    const content::CookieAccessDetails& details) {
  if (details.cookie_list.empty())
    return;
  if (details.blocked_by_policy) {
    blocked_local_shared_objects_.cookies()->AddCookies(details);
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    allowed_local_shared_objects_.cookies()->AddCookies(details);
    OnContentAllowed(ContentSettingsType::COOKIES);
    delegate_->OnCookieAccessAllowed(details.cookie_list);
  }

  handler_.NotifySiteDataObservers();
}

void PageSpecificContentSettings::OnIndexedDBAccessed(const GURL& url,
                                                      bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.indexed_dbs()->Add(url::Origin::Create(url));
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    allowed_local_shared_objects_.indexed_dbs()->Add(url::Origin::Create(url));
    delegate_->OnIndexedDBAccessAllowed(url::Origin::Create(url));
    OnContentAllowed(ContentSettingsType::COOKIES);
  }

  handler_.NotifySiteDataObservers();
}

void PageSpecificContentSettings::OnCacheStorageAccessed(
    const GURL& url,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.cache_storages()->Add(
        url::Origin::Create(url));
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    allowed_local_shared_objects_.cache_storages()->Add(
        url::Origin::Create(url));
    delegate_->OnCacheStorageAccessAllowed(url::Origin::Create(url));
    OnContentAllowed(ContentSettingsType::COOKIES);
  }

  handler_.NotifySiteDataObservers();
}

void PageSpecificContentSettings::OnServiceWorkerAccessed(
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  DCHECK(scope.is_valid());
  if (allowed) {
    allowed_local_shared_objects_.service_workers()->Add(
        url::Origin::Create(scope));
    delegate_->OnServiceWorkerAccessAllowed(url::Origin::Create(scope));
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
}

void PageSpecificContentSettings::OnSharedWorkerAccessed(
    const GURL& worker_url,
    const std::string& name,
    const url::Origin& constructor_origin,
    bool blocked_by_policy) {
  DCHECK(worker_url.is_valid());
  if (blocked_by_policy) {
    blocked_local_shared_objects_.shared_workers()->AddSharedWorker(
        worker_url, name, constructor_origin);
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    allowed_local_shared_objects_.shared_workers()->AddSharedWorker(
        worker_url, name, constructor_origin);
    OnContentAllowed(ContentSettingsType::COOKIES);
  }
}

void PageSpecificContentSettings::OnWebDatabaseAccessed(
    const GURL& url,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.databases()->Add(url::Origin::Create(url));
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    allowed_local_shared_objects_.databases()->Add(url::Origin::Create(url));
    delegate_->OnWebDatabaseAccessAllowed(url::Origin::Create(url));
    OnContentAllowed(ContentSettingsType::COOKIES);
  }

  handler_.NotifySiteDataObservers();
}

void PageSpecificContentSettings::OnFileSystemAccessed(const GURL& url,
                                                       bool blocked_by_policy) {
  // Note that all sandboxed file system access is recorded here as
  // kTemporary; the distinction between temporary (default) and persistent
  // storage is not made in the UI that presents this data.
  if (blocked_by_policy) {
    blocked_local_shared_objects_.file_systems()->Add(url::Origin::Create(url));
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    allowed_local_shared_objects_.file_systems()->Add(url::Origin::Create(url));
    delegate_->OnFileSystemAccessAllowed(url::Origin::Create(url));
    OnContentAllowed(ContentSettingsType::COOKIES);
  }

  handler_.NotifySiteDataObservers();
}

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
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
  media_stream_access_origin_ = request_origin;

  if (new_microphone_camera_state & MICROPHONE_ACCESSED) {
    media_stream_requested_audio_device_ = media_stream_requested_audio_device;
    media_stream_selected_audio_device_ = media_stream_selected_audio_device;
    bool mic_blocked = (new_microphone_camera_state & MICROPHONE_BLOCKED) != 0;
    ContentSettingsStatus& status =
        content_settings_status_[ContentSettingsType::MEDIASTREAM_MIC];
    if (!status.allowed && !mic_blocked)
      delegate_->OnContentAllowed(ContentSettingsType::MEDIASTREAM_MIC);
    status.allowed = !mic_blocked;
    status.blocked = mic_blocked;
  }

  if (new_microphone_camera_state & CAMERA_ACCESSED) {
    media_stream_requested_video_device_ = media_stream_requested_video_device;
    media_stream_selected_video_device_ = media_stream_selected_video_device;
    bool cam_blocked = (new_microphone_camera_state & CAMERA_BLOCKED) != 0;
    ContentSettingsStatus& status =
        content_settings_status_[ContentSettingsType::MEDIASTREAM_CAMERA];
    if (!status.allowed && !cam_blocked)
      delegate_->OnContentAllowed(ContentSettingsType::MEDIASTREAM_CAMERA);
    status.allowed = !cam_blocked;
    status.blocked = cam_blocked;
  }

  if (microphone_camera_state_ != new_microphone_camera_state) {
    microphone_camera_state_ = new_microphone_camera_state;
    delegate_->UpdateLocationBar();
  }
}

void PageSpecificContentSettings::FlashDownloadBlocked() {
  OnContentBlocked(ContentSettingsType::PLUGINS);
}

void PageSpecificContentSettings::ClearPopupsBlocked() {
  ContentSettingsStatus& status =
      content_settings_status_[ContentSettingsType::POPUPS];
  status.blocked = false;
  delegate_->UpdateLocationBar();
}

void PageSpecificContentSettings::OnAudioBlocked() {
  OnContentBlocked(ContentSettingsType::SOUND);
}

void PageSpecificContentSettings::SetPepperBrokerAllowed(bool allowed) {
  if (allowed) {
    OnContentAllowed(ContentSettingsType::PPAPI_BROKER);
  } else {
    OnContentBlocked(ContentSettingsType::PPAPI_BROKER);
  }
}

void PageSpecificContentSettings::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  const ContentSettingsDetails details(primary_pattern, secondary_pattern,
                                       content_type, resource_identifier);
  if (!details.update_all() &&
      // The visible URL is the URL in the URL field of a tab.
      // Currently this should be matched by the |primary_pattern|.
      !details.primary_pattern().Matches(visible_url_)) {
    return;
  }

  ContentSettingsStatus& status = content_settings_status_[content_type];
  switch (content_type) {
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA: {
      const GURL media_origin = media_stream_access_origin();
      ContentSetting setting = map_->GetContentSetting(
          media_origin, media_origin, content_type, std::string());

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
      ContentSetting geolocation_setting = map_->GetContentSetting(
          visible_url_, visible_url_, content_type, std::string());
      if (geolocation_setting == CONTENT_SETTING_ALLOW)
        geolocation_was_just_granted_on_site_level_ = true;
      FALLTHROUGH;
    }
    case ContentSettingsType::IMAGES:
    case ContentSettingsType::JAVASCRIPT:
    case ContentSettingsType::PLUGINS:
    case ContentSettingsType::COOKIES:
    case ContentSettingsType::POPUPS:
    case ContentSettingsType::MIXEDSCRIPT:
    case ContentSettingsType::PPAPI_BROKER:
    case ContentSettingsType::MIDI_SYSEX:
    case ContentSettingsType::ADS:
    case ContentSettingsType::SOUND:
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
    case ContentSettingsType::SENSORS: {
      ContentSetting setting = map_->GetContentSetting(
          visible_url_, visible_url_, content_type, std::string());
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

  if (!ShouldSendUpdatedContentSettingsRulesToRenderer(content_type))
    return;

  MaybeSendRendererContentSettingsRules(main_frame_, map_, delegate_);
}

void PageSpecificContentSettings::AppCacheAccessed(const GURL& manifest_url,
                                                   bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_local_shared_objects_.appcaches()->Add(
        url::Origin::Create(manifest_url));
    OnContentBlocked(ContentSettingsType::COOKIES);
  } else {
    allowed_local_shared_objects_.appcaches()->Add(
        url::Origin::Create(manifest_url));
    OnContentAllowed(ContentSettingsType::COOKIES);
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
  OnMediaStreamPermissionSet(main_frame_->GetLastCommittedURL(), media_blocked,
                             std::string(), std::string(), std::string(),
                             std::string());
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

RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(PageSpecificContentSettings)

}  // namespace content_settings
