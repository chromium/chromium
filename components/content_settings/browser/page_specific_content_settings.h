// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}

namespace url {
class Origin;
}  // namespace url

namespace content_settings {

// TODO(msramek): Media is storing their state in PageSpecificContentSettings:
// |microphone_camera_state_| without being tied to a single content setting.
// This state is not ideal, potential solution is to save this information via
// content::WebContentsUserData

// This class manages state about permissions, content settings, cookies and
// site data for a specific page (main document and all of its child frames). It
// tracks which content was accessed and which content was blocked. Based on
// this it provides information about which types of content were accessed and
// blocked.
//
// Tracking is done per main document so instances of this class will be deleted
// when the main document is deleted. This can happen after the tab navigates
// away to a new document or when the tab itself is deleted, so you should not
// keep references to objects of this class.
//
// When a page enters the back-forward cache its associated
// PageSpecificContentSettings are not cleared and will be restored along with
// the document when navigating back. These stored instances still listen to
// content settings updates and keep their internal state up to date.
//
// Events tied to a main frame navigation will be associated with the newly
// loaded page once the navigation commits or discarded if it does not.
class PageSpecificContentSettings
    : public content_settings::Observer,
      public content::RenderDocumentHostUserData<PageSpecificContentSettings> {
 public:
  // Fields describing the current mic/camera state. If a page has attempted to
  // access a device, the XXX_ACCESSED bit will be set. If access was blocked,
  // XXX_BLOCKED will be set.
  enum MicrophoneCameraStateFlags {
    MICROPHONE_CAMERA_NOT_ACCESSED = 0,
    MICROPHONE_ACCESSED = 1 << 0,
    MICROPHONE_BLOCKED = 1 << 1,
    CAMERA_ACCESSED = 1 << 2,
    CAMERA_BLOCKED = 1 << 3,
  };
  // Use signed int, that's what the enum flags implicitly convert to.
  typedef int32_t MicrophoneCameraState;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when content settings state changes that might require updating
    // the location bar.
    virtual void UpdateLocationBar() = 0;

    // Notifies the delegate content settings rules have changed that need to be
    // sent to the renderer.
    virtual void SetContentSettingRules(
        content::RenderProcessHost* process,
        const RendererContentSettingRules& rules) = 0;

    // Gets the pref service for the current web contents.
    virtual PrefService* GetPrefs() = 0;

    // Gets the settings map for the current web contents.
    virtual HostContentSettingsMap* GetSettingsMap() = 0;

    virtual ContentSetting GetEmbargoSetting(
        const GURL& request_origin,
        ContentSettingsType permission) = 0;

    // Gets any additional file system types which should be used when
    // constructing a browsing_data::FileSystemHelper.
    virtual std::vector<storage::FileSystemType>
    GetAdditionalFileSystemTypes() = 0;

    virtual browsing_data::CookieHelper::IsDeletionDisabledCallback
    GetIsDeletionDisabledCallback() = 0;

    // Allows the delegate to provide additional logic for detecting state
    // changes on top of the camera/microphone permission state.
    virtual bool IsMicrophoneCameraStateChanged(
        MicrophoneCameraState microphone_camera_state,
        const std::string& media_stream_selected_audio_device,
        const std::string& media_stream_selected_video_device) = 0;

    // Allows the delegate to provide additional logic for getting microphone
    // and camera state on top of the microphone and camera state at the last
    // media stream request.
    virtual MicrophoneCameraState GetMicrophoneCameraState() = 0;

    // Notifies the delegate a particular content settings type was allowed for
    // the first time on this page.
    virtual void OnContentAllowed(ContentSettingsType type) = 0;

    // Notifies the delegate a particular content settings type was blocked.
    virtual void OnContentBlocked(ContentSettingsType type) = 0;

    // Notifies the delegate that access was granted to cache storage for
    // |origin|.
    virtual void OnCacheStorageAccessAllowed(const url::Origin& origin) = 0;

    // Notifies the delegate that access was granted to |accessed_cookies|.
    virtual void OnCookieAccessAllowed(
        const net::CookieList& accessed_cookies) = 0;

    // Notifies the delegate that access was granted to DOM storage for
    // |origin|.
    virtual void OnDomStorageAccessAllowed(const url::Origin& origin) = 0;

    // Notifies the delegate that access was granted to file system storage for
    // |origin|.
    virtual void OnFileSystemAccessAllowed(const url::Origin& origin) = 0;

    // Notifies the delegate that access was granted to Indexed DB storage for
    // |origin|.
    virtual void OnIndexedDBAccessAllowed(const url::Origin& origin) = 0;

    // Notifies the delegate that access was granted to service workers for
    // |origin|.
    virtual void OnServiceWorkerAccessAllowed(const url::Origin& origin) = 0;

    // Notifies the delegate that access was granted to web database storage for
    // |origin|.
    virtual void OnWebDatabaseAccessAllowed(const url::Origin& origin) = 0;
  };

  // Classes that want to be notified about site data events must implement
  // this abstract class and add themselves as observer to the
  // |PageSpecificContentSettings|.
  class SiteDataObserver {
   public:
    explicit SiteDataObserver(content::WebContents* web_contents);
    virtual ~SiteDataObserver();

    // Called whenever site data is accessed.
    virtual void OnSiteDataAccessed() = 0;

    content::WebContents* web_contents() { return web_contents_; }

    // Called when the WebContents is destroyed; nulls out
    // the local reference.
    void WebContentsDestroyed();

   private:
    content::WebContents* web_contents_;

    DISALLOW_COPY_AND_ASSIGN(SiteDataObserver);
  };

  ~PageSpecificContentSettings() override;

  static void CreateForWebContents(content::WebContents* web_contents,
                                   std::unique_ptr<Delegate> delegate);
  static void DeleteForWebContentsForTest(content::WebContents* web_contents);

  // Returns the object given a RenderFrameHost ids. Returns nullptr if the
  // frame no longer exist or there are no PageSpecificContentSettings attached
  // to the document.
  static PageSpecificContentSettings* GetForFrame(int render_process_id,
                                                  int render_frame_id);
  // Returns the object given a RenderFrameHost. Returns nullptr if the frame
  // is nullptr or there are no PageSpecificContentSettings attached to the
  // document.
  static PageSpecificContentSettings* GetForFrame(
      content::RenderFrameHost* rfh);

  // Returns the Delegate that was associated to |web_contents| in
  // CreateForWebContents. Null if CreateForWebContents was not called for
  // |web_contents|.
  static PageSpecificContentSettings::Delegate* GetDelegateForWebContents(
      content::WebContents* web_contents);

  // Called when a specific Web database in the current page was accessed. If
  // access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void WebDatabaseAccessed(int render_process_id,
                                  int render_frame_id,
                                  const GURL& url,
                                  bool blocked_by_policy);

  // Called when a specific indexed db factory in the current page was
  // accessed. If access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void IndexedDBAccessed(int render_process_id,
                                int render_frame_id,
                                const GURL& url,
                                bool blocked_by_policy);

  // Called when CacheStorage::Open() is called in the current page.
  // If access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void CacheStorageAccessed(int render_process_id,
                                   int render_frame_id,
                                   const GURL& url,
                                   bool blocked_by_policy);

  // Called when a specific file system in the current page was accessed.
  // If access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void FileSystemAccessed(int render_process_id,
                                 int render_frame_id,
                                 const GURL& url,
                                 bool blocked_by_policy);

  // Called when a specific Shared Worker was accessed.
  static void SharedWorkerAccessed(int render_process_id,
                                   int render_frame_id,
                                   const GURL& worker_url,
                                   const std::string& name,
                                   const url::Origin& constructor_origin,
                                   bool blocked_by_policy);

  static content::WebContentsObserver* GetWebContentsObserverForTest(
      content::WebContents* web_contents);

  // Returns a WeakPtr to this instance. Given that PageSpecificContentSettings
  // instances are tied to a page it is generally unsafe to store these
  // references, instead a WeakPtr should be used instead.
  base::WeakPtr<PageSpecificContentSettings> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Changes the |content_blocked_| entry for popups.
  void ClearPopupsBlocked();

  // Called when audio has been blocked on the page.
  void OnAudioBlocked();

  // Returns whether a particular kind of content has been blocked for this
  // page.
  bool IsContentBlocked(ContentSettingsType content_type) const;

  // Returns whether a particular kind of content has been allowed. Currently
  // only tracks cookies.
  bool IsContentAllowed(ContentSettingsType content_type) const;

  const GURL& media_stream_access_origin() const {
    return media_stream_access_origin_;
  }

  const std::string& media_stream_requested_audio_device() const {
    return media_stream_requested_audio_device_;
  }

  const std::string& media_stream_requested_video_device() const {
    return media_stream_requested_video_device_;
  }

  // Only public for tests.
  const std::string& media_stream_selected_audio_device() const {
    return media_stream_selected_audio_device_;
  }

  // Only public for tests.
  const std::string& media_stream_selected_video_device() const {
    return media_stream_selected_video_device_;
  }

  bool camera_was_just_granted_on_site_level() {
    return camera_was_just_granted_on_site_level_;
  }

  bool mic_was_just_granted_on_site_level() {
    return mic_was_just_granted_on_site_level_;
  }

  bool geolocation_was_just_granted_on_site_level() {
    return geolocation_was_just_granted_on_site_level_;
  }

  // Returns the state of the camera and microphone usage.
  // The return value always includes all active media capture devices, on top
  // of the devices from the last request.
  MicrophoneCameraState GetMicrophoneCameraState() const;

  // Returns whether the camera or microphone permission or media device setting
  // has changed since the last permission request.
  bool IsMicrophoneCameraStateChanged() const;

  // Returns the |LocalSharedObjectsContainer| instances corresponding to all
  // allowed, and blocked, respectively, local shared objects like cookies,
  // local storage, ... .
  const browsing_data::LocalSharedObjectsContainer&
  allowed_local_shared_objects() const {
    return allowed_local_shared_objects_;
  }

  const browsing_data::LocalSharedObjectsContainer&
  blocked_local_shared_objects() const {
    return blocked_local_shared_objects_;
  }

  // Called to indicate whether access to the Pepper broker was allowed or
  // blocked.
  void SetPepperBrokerAllowed(bool allowed);

  void OnContentBlocked(ContentSettingsType type);
  void OnContentAllowed(ContentSettingsType type);

  // These methods are invoked on the UI thread forwarded from the
  // ContentSettingsManagerImpl.
  void OnDomStorageAccessed(const GURL& url,
                            bool local,
                            bool blocked_by_policy);

  // These methods are invoked on the UI thread by the static functions above.
  // Only public for tests.
  void OnFileSystemAccessed(const GURL& url, bool blocked_by_policy);
  void OnIndexedDBAccessed(const GURL& url, bool blocked_by_policy);
  void OnCacheStorageAccessed(const GURL& url, bool blocked_by_policy);
  void OnSharedWorkerAccessed(const GURL& worker_url,
                              const std::string& name,
                              const url::Origin& constructor_origin,
                              bool blocked_by_policy);
  void OnWebDatabaseAccessed(const GURL& url, bool blocked_by_policy);
#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  void OnProtectedMediaIdentifierPermissionSet(const GURL& requesting_frame,
                                               bool allowed);
#endif

  // This method is called to update the status about the microphone and
  // camera stream access.
  void OnMediaStreamPermissionSet(
      const GURL& request_origin,
      MicrophoneCameraState new_microphone_camera_state,
      const std::string& media_stream_selected_audio_device,
      const std::string& media_stream_selected_video_device,
      const std::string& media_stream_requested_audio_device,
      const std::string& media_stream_requested_video_device);

  void OnCookiesAccessed(const content::CookieAccessDetails& details);
  void OnServiceWorkerAccessed(const GURL& scope,
                               content::AllowServiceWorkerResult allowed);

  // Block all content. Used for testing content setting bubbles.
  void BlockAllContentForTesting();

  // Stores content settings changed by the user via PageInfo.
  void ContentSettingChangedViaPageInfo(ContentSettingsType type);

  // Returns true if the user changed the given ContentSettingsType via PageInfo
  // since the last navigation.
  bool HasContentSettingChangedViaPageInfo(ContentSettingsType type) const;

 private:
  friend class content::RenderDocumentHostUserData<PageSpecificContentSettings>;

  // This class attaches to WebContents to listen to events and route them to
  // appropriate PageSpecificContentSettings, store navigation related events
  // until the navigation finishes and then transferring the
  // navigation-associated state to the newly-created page.
  class WebContentsHandler
      : public content::WebContentsObserver,
        public content::WebContentsUserData<WebContentsHandler> {
   public:
    explicit WebContentsHandler(content::WebContents* web_contents,
                                std::unique_ptr<Delegate> delegate);
    ~WebContentsHandler() override;
    // Adds the given |SiteDataObserver|. The |observer| is notified when a
    // locale shared object, like for example a cookie, is accessed.
    void AddSiteDataObserver(SiteDataObserver* observer);

    // Removes the given |SiteDataObserver|.
    void RemoveSiteDataObserver(SiteDataObserver* observer);

    // Notifies all registered |SiteDataObserver|s.
    void NotifySiteDataObservers();

    Delegate* delegate() { return delegate_.get(); }

   private:
    friend class content::WebContentsUserData<WebContentsHandler>;

    // Keeps track of cookie and service worker access during a navigation.
    // These types of access can happen for the current page or for a new
    // navigation (think cookies sent in the HTTP request or service worker
    // being run to serve a fetch request). A navigation might fail to
    // commit in which case we have to handle it as if it had never
    // occurred. So we cache all cookies and service worker accesses that
    // happen during a navigation and only apply the changes if the
    // navigation commits.
    struct InflightNavigationContentSettings {
      InflightNavigationContentSettings();
      InflightNavigationContentSettings(
          const InflightNavigationContentSettings&);
      InflightNavigationContentSettings(InflightNavigationContentSettings&&);

      ~InflightNavigationContentSettings();

      InflightNavigationContentSettings& operator=(
          InflightNavigationContentSettings&&);

      std::vector<content::CookieAccessDetails> cookie_accesses;
      std::vector<std::pair<GURL, content::AllowServiceWorkerResult>>
          service_worker_accesses;
    };

    // Applies all stored events for the given navigation to the current main
    // document.
    void TransferNavigationContentSettingsToCommittedDocument(
        const InflightNavigationContentSettings& navigation_settings,
        content::RenderFrameHost* rfh);

    // content::WebContentsObserver overrides.
    void DidStartNavigation(
        content::NavigationHandle* navigation_handle) override;
    void ReadyToCommitNavigation(
        content::NavigationHandle* navigation_handle) override;
    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;
    // TODO(carlscab): Change interface to pass target RenderFrameHost
    void AppCacheAccessed(const GURL& manifest_url,
                          bool blocked_by_policy) override;
    void OnCookiesAccessed(
        content::NavigationHandle* navigation,
        const content::CookieAccessDetails& details) override;
    void OnCookiesAccessed(
        content::RenderFrameHost* rfh,
        const content::CookieAccessDetails& details) override;
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

    std::unique_ptr<Delegate> delegate_;

    HostContentSettingsMap* map_;

    // All currently registered |SiteDataObserver|s.
    base::ObserverList<SiteDataObserver>::Unchecked observer_list_;

    // Keeps track of currently inflight navigations. Updates for those are
    // kept aside until the navigation commits.
    std::unordered_map<content::NavigationHandle*,
                       InflightNavigationContentSettings>
        inflight_navigation_settings_;

    WEB_CONTENTS_USER_DATA_KEY_DECL();
  };

  explicit PageSpecificContentSettings(
      PageSpecificContentSettings::WebContentsHandler& handler,
      Delegate* delegate);

  void AppCacheAccessed(const GURL& manifest_url, bool blocked_by_policy);

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

  // Clears settings changed by the user via PageInfo since the last navigation.
  void ClearContentSettingsChangedViaPageInfo();

  WebContentsHandler& handler_;
  content::RenderFrameHost* main_frame_;

  Delegate* delegate_;

  GURL visible_url_;

  struct ContentSettingsStatus {
    bool blocked;
    bool allowed;
  };
  // Stores which content setting types actually have blocked content.
  std::map<ContentSettingsType, ContentSettingsStatus> content_settings_status_;

  // Profile-bound, this will outlive this class (which is WebContents bound).
  HostContentSettingsMap* map_;

  // Stores the blocked/allowed cookies.
  browsing_data::LocalSharedObjectsContainer allowed_local_shared_objects_;
  browsing_data::LocalSharedObjectsContainer blocked_local_shared_objects_;

  // The origin of the media stream request. Note that we only support handling
  // settings for one request per tab. The latest request's origin will be
  // stored here. http://crbug.com/259794
  GURL media_stream_access_origin_;

  // The microphone and camera state at the last media stream request.
  MicrophoneCameraState microphone_camera_state_;
  // The selected devices at the last media stream request.
  std::string media_stream_selected_audio_device_;
  std::string media_stream_selected_video_device_;

  // The devices to be displayed in the media bubble when the media stream
  // request is requesting certain specific devices.
  std::string media_stream_requested_audio_device_;
  std::string media_stream_requested_video_device_;

  // The Geolocation, camera, and/or microphone permission was granted to this
  // origin from a permission prompt that was triggered by the currently active
  // document.
  bool camera_was_just_granted_on_site_level_ = false;
  bool mic_was_just_granted_on_site_level_ = false;
  bool geolocation_was_just_granted_on_site_level_ = false;

  // Observer to watch for content settings changed.
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      observation_{this};

  // Stores content settings changed by the user via page info since the last
  // navigation. Used to determine whether to display the settings in page info.
  std::set<ContentSettingsType> content_settings_changed_via_page_info_;

  RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<PageSpecificContentSettings> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PageSpecificContentSettings);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_H_
