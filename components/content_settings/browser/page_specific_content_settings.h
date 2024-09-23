// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {
class WebContents;
class WebContentsObserver;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace content_settings {

enum class SiteDataType {
  kUnknown,
  kStorage,
  kCookies,
  kServiceWorker,
  kSharedWorker,
  kInterestGroup,
  kTopic,
  kTrustToken,
};

enum class AccessType {
  // This value represents situations where by the PSCS doesn't have enough
  // information to assess the accurate nature of site data access.
  kUnknown,
  kRead,
  kWrite,
};

// Holds extra details about the site data access deemed useful by PSCS
// observers.
struct AccessDetails {
  AccessDetails();
  AccessDetails(SiteDataType site_data_type,
                AccessType access_type,
                GURL url,
                bool blocked_by_policy,
                bool is_from_primary_page);
  ~AccessDetails();
  bool operator<(const AccessDetails& other) const;

  SiteDataType site_data_type = SiteDataType::kUnknown;
  AccessType access_type = AccessType::kUnknown;
  GURL url;
  bool blocked_by_policy = false;
  // Specifies whether the access occurred in the primary page.
  bool is_from_primary_page = false;
};

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
      public content::PageUserData<PageSpecificContentSettings> {
 public:
  // Fields describing the current mic/camera state. If a page has attempted to
  // access a device, the kXxxAccessed bit will be set. If access was blocked,
  // kXxxBlocked will be set.
  enum MicrophoneCameraStateFlags {
    kMicrophoneAccessed,
    kMicrophoneBlocked,
    kCameraAccessed,
    kCameraBlocked,

    kMinValue = kMicrophoneAccessed,
    kMaxValue = kCameraBlocked,
  };
  using MicrophoneCameraState =
      base::EnumSet<MicrophoneCameraStateFlags,
                    MicrophoneCameraStateFlags::kMinValue,
                    MicrophoneCameraStateFlags::kMaxValue>;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when content settings state changes that might require updating
    // the location bar.
    virtual void UpdateLocationBar() = 0;

    // Gets the pref service for the current web contents.
    virtual PrefService* GetPrefs() = 0;

    // Gets the settings map for the current web contents.
    virtual HostContentSettingsMap* GetSettingsMap() = 0;

    // Gets the `BrowsingDataModel::Delegate` for the current profile.
    virtual std::unique_ptr<BrowsingDataModel::Delegate>
    CreateBrowsingDataModelDelegate() = 0;

    // Allows delegate to override content setting rules that will be sent to
    // the renderer.
    virtual void SetDefaultRendererContentSettingRules(
        content::RenderFrameHost* rfh,
        RendererContentSettingRules* rules) = 0;

    // Allows the delegate to provide additional logic for getting microphone
    // and camera state on top of the microphone and camera state at the last
    // media stream request.
    virtual MicrophoneCameraState GetMicrophoneCameraState() = 0;

    // If there is a document picture-in-picture window open, check if the given
    // web contents is the opener web contents or child web contents, and return
    // the other web contents to be synced.
    virtual content::WebContents* MaybeGetSyncedWebContentsForPictureInPicture(
        content::WebContents* web_contents) = 0;

    // Notifies the delegate a particular content settings type was allowed for
    // the first time on this page.
    virtual void OnContentAllowed(ContentSettingsType type) = 0;

    // Notifies the delegate a particular content settings type was blocked.
    virtual void OnContentBlocked(ContentSettingsType type) = 0;

    // Returns `true` if ContentSettingsType is blocked on the system level.
    virtual bool IsBlockedOnSystemLevel(ContentSettingsType type) = 0;

    // Returns true if `render_frame_host` should be allowlisted for using
    // JavaScript.
    virtual bool IsFrameAllowlistedForJavaScript(
        content::RenderFrameHost* render_frame_host) = 0;
  };

  // Classes that want to be notified about site data events must implement
  // this abstract class and add themselves as observer to the
  // |PageSpecificContentSettings|.
  class SiteDataObserver {
   public:
    explicit SiteDataObserver(content::WebContents* web_contents);

    SiteDataObserver(const SiteDataObserver&) = delete;
    SiteDataObserver& operator=(const SiteDataObserver&) = delete;

    virtual ~SiteDataObserver();

    // Called whenever site data is accessed.
    virtual void OnSiteDataAccessed(const AccessDetails& access_details) = 0;

    // Called whenever this page is loaded via a redirect with stateful bounces.
    virtual void OnStatefulBounceDetected() = 0;

    content::WebContents* web_contents() { return web_contents_; }

    // Called when the WebContents is destroyed; nulls out
    // the local reference.
    void WebContentsDestroyed();

   private:
    raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  };

  class PermissionUsageObserver : public base::CheckedObserver {
   public:
    virtual void OnPermissionUsageChange() = 0;
  };

  PageSpecificContentSettings(const PageSpecificContentSettings&) = delete;
  PageSpecificContentSettings& operator=(const PageSpecificContentSettings&) =
      delete;

  ~PageSpecificContentSettings() override;

  static void CreateForWebContents(content::WebContents* web_contents,
                                   std::unique_ptr<Delegate> delegate);
  static void DeleteForWebContentsForTest(content::WebContents* web_contents);

  // Returns the object given a RenderFrameHost ids. Returns nullptr if the
  // frame no longer exist or there are no PageSpecificContentSettings attached
  // to the document.
  static PageSpecificContentSettings* GetForFrame(
      const content::GlobalRenderFrameHostId& id);

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

  static void StorageAccessed(
      mojom::ContentSettingsManager::StorageType storage_type,
      absl::variant<content::GlobalRenderFrameHostToken,
                    content::GlobalRenderFrameHostId> frame_id,
      const blink::StorageKey& storage_key,
      bool blocked_by_policy);

  static void BrowsingDataAccessed(content::RenderFrameHost* rfh,
                                   BrowsingDataModel::DataKey data_key,
                                   BrowsingDataModel::StorageType storage_type,
                                   bool blocked);

  // Called when content access is blocked in the renderer process.
  static void ContentBlocked(
      const content::GlobalRenderFrameHostToken& frame_token,
      ContentSettingsType type);

  // Called when a specific Shared Worker was accessed.
  static void SharedWorkerAccessed(
      int render_process_id,
      int render_frame_id,
      const GURL& worker_url,
      const std::string& name,
      const blink::StorageKey& storage_key,
      const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies,
      bool blocked_by_policy);

  // Called when |api_origin| attempts to join an interest group via the
  // Interest Group API.
  static void InterestGroupJoined(content::RenderFrameHost* rfh,
                                  const url::Origin& api_origin,
                                  bool blocked_by_policy);

  // Called when |api_origin| attempts to access browsing topics.
  static void TopicAccessed(content::RenderFrameHost* rfh,
                            const url::Origin& api_origin,
                            bool blocked_by_policy,
                            privacy_sandbox::CanonicalTopic topic);

  // Called when notifications are accessed on `rfh`.
  static void NotificationsAccessed(content::RenderFrameHost* rfh,
                                    bool blocked);

  static content::WebContentsObserver* GetWebContentsObserverForTest(
      content::WebContents* web_contents);

  // Returns a WeakPtr to this instance. Given that PageSpecificContentSettings
  // instances are tied to a page it is generally unsafe to store these
  // references, instead a WeakPtr should be used instead.
  base::WeakPtr<PageSpecificContentSettings> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Add/remove observer.
  void AddPermissionUsageObserver(PermissionUsageObserver* observer);
  void RemovePermissionUsageObserver(PermissionUsageObserver* observer);

  // Changes the |content_blocked_| entry for popups.
  void ClearPopupsBlocked();

  // Called when audio has been blocked on the page.
  void OnAudioBlocked();

  // Records one additional stateful bounce during the navigation that led to
  // this page.
  void IncrementStatefulBounceCount();

  // Returns whether a particular kind of content has been blocked for this
  // page.
  bool IsContentBlocked(ContentSettingsType content_type) const;

  // Returns whether a particular kind of content has been allowed. Currently
  // only tracks cookies.
  bool IsContentAllowed(ContentSettingsType content_type) const;

  // Returns a map from sites that requested |content_setting| to whether the
  // permission was granted. This method is only supported for permissions that
  // are scoped to sites and apply to embedded content, e.g. StorageAccess.
  std::map<net::SchemefulSite, /*is_allowed*/ bool> GetTwoSiteRequests(
      ContentSettingsType content_type);

  const GURL& media_stream_access_origin() const {
    return media_stream_access_origin_;
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

  // Called when notifications permission was auto-denied because the system
  // level permission for a PWA was previously denied. This is currently only
  // possible/used on macOS.
  void SetNotificationsWasDeniedBecauseOfSystemPermission();

  bool notifications_was_denied_because_of_system_permission() {
    return notifications_was_denied_because_of_system_permission_;
  }

  // Returns the state of the camera and microphone usage.
  // The return value always includes all active media capture devices, on top
  // of the devices from the last request.
  MicrophoneCameraState GetMicrophoneCameraState() const;

  // Returns whether the camera or microphone permission or media device setting
  // has changed since the last permission request.
  bool IsMicrophoneCameraStateChanged() const;

  int stateful_bounce_count() const { return stateful_bounce_count_; }

  BrowsingDataModel* allowed_browsing_data_model() const {
    return allowed_browsing_data_model_.get();
  }

  BrowsingDataModel* blocked_browsing_data_model() const {
    return blocked_browsing_data_model_.get();
  }

  void OnContentBlocked(ContentSettingsType type);
  void OnContentAllowed(ContentSettingsType type);

  // Call when a two-site permission was prompted or modified in order to
  // display a ContentSettingsImageModel icon.
  void OnTwoSitePermissionChanged(ContentSettingsType type,
                                  net::SchemefulSite requesting_site,
                                  ContentSetting content_setting);
  void OnInterestGroupJoined(const url::Origin& api_origin,
                             bool blocked_by_policy);
  void OnTopicAccessed(const url::Origin& api_origin,
                       bool blocked_by_policy,
                       privacy_sandbox::CanonicalTopic topic);
  void OnTrustTokenAccessed(const url::Origin& api_origin, bool blocked);
  void OnBrowsingDataAccessed(BrowsingDataModel::DataKey data_key,
                              BrowsingDataModel::StorageType storage_type,
                              bool blocked,
                              content::Page* originating_page = nullptr);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  void OnProtectedMediaIdentifierPermissionSet(const GURL& requesting_frame,
                                               bool allowed);
#endif

  // This method is called to update the status about the microphone and
  // camera stream access.
  void OnMediaStreamPermissionSet(
      const GURL& request_origin,
      MicrophoneCameraState new_microphone_camera_state);

  // |originating_page| is non-null when it differs from page(), which happens
  // when an embedding page's PSCS is notified of an access that happens in an
  // embedded page (through |MaybeUpdateParent|).
  void OnCookiesAccessed(const content::CookieAccessDetails& details,
                         content::Page* originating_page = nullptr);
  void OnServiceWorkerAccessed(const GURL& scope,
                               const blink::StorageKey& storage_key,
                               content::AllowServiceWorkerResult allowed,
                               content::Page* originating_page = nullptr);

  // Block all content. Used for testing content setting bubbles.
  void BlockAllContentForTesting();

  // Stores content settings changed by the user via PageInfo.
  void ContentSettingChangedViaPageInfo(ContentSettingsType type);

  // Returns true if the user changed the given ContentSettingsType via PageInfo
  // since the last navigation.
  bool HasContentSettingChangedViaPageInfo(ContentSettingsType type) const;

  // Returns true if the user was joined to an interest group and if the page
  // is the joining origin.
  bool HasJoinedUserToInterestGroup() const;

  // Returns true if the page has accessed the Topics API.
  bool HasAccessedTopics() const;

  // Returns the topics that were accessed by this page.
  std::vector<privacy_sandbox::CanonicalTopic> GetAccessedTopics() const;

  // Runs any queued updates in |updates_queued_during_prerender_|, should be
  // called after the page activates.
  void OnPrerenderingPageActivation();

  // This method is called when audio or video capturing is started or finished.
  void OnCapturingStateChanged(ContentSettingsType type, bool is_capturing);

  // Returns true if a page is currently using a feature gated behind `type`
  // permission. Returns false otherwise.
  bool IsInUse(ContentSettingsType type) { return in_use_.contains(type); }

  // Returns a time of last usage of a feature gated behind `type` permission.
  // Returns base::Time() if `type` was not used in the last 24 hours.
  const base::Time GetLastUsedTime(ContentSettingsType type);

  // This method is called when audio or video activity indicator is opened.
  void OnActivityIndicatorBubbleOpened(ContentSettingsType type);

  // This method is called when audio or video activity indicator is closed.
  void OnActivityIndicatorBubbleClosed(ContentSettingsType type);

  // Returns `true` if an activity indicator is displaying for
  // `ContentSettingsType`. Returns `false` otherwise.
  bool IsIndicatorVisible(ContentSettingsType type) const;
  // Save `ContentSettingsType` to a set of currently displaying activity
  // indicators.
  void OnPermissionIndicatorShown(ContentSettingsType type);
  // Remove `ContentSettingsType` from a set of currently displaying activity
  // indicators.
  void OnPermissionIndicatorHidden(ContentSettingsType type);

  // If permission requests need to be cleaned up due to a page refresh, PSCS
  // should be temporarily frozen to prevent unnecessary update of activity
  // indicators.
  void OnPermissionRequestCleanupStart() { freeze_indicators_ = true; }
  void OnPermissionRequestCleanupEnd() { freeze_indicators_ = false; }

  // This method resets a media blocked state for `type`. If `update_indicators`
  // is true, then it will try to update activity indicators in the location
  // bar.
  void ResetMediaBlockedState(ContentSettingsType type, bool update_indicators);

  void set_media_stream_access_origin_for_testing(const GURL& url) {
    media_stream_access_origin_ = url;
  }

  void set_last_used_time_for_testing(ContentSettingsType type,
                                      base::Time time) {
    last_used_time_[type] = time;
  }

  std::map<ContentSettingsType, base::OneShotTimer>&
  get_media_blocked_indicator_timer_for_testing() {
    return media_blocked_indicator_timer_;
  }

  std::map<ContentSettingsType, base::OneShotTimer>&
  get_indicators_hiding_delay_timer_for_testing() {
    return indicators_hiding_delay_timer_;
  }

 private:
  friend class content::PageUserData<PageSpecificContentSettings>;

  struct PendingUpdates {
    PendingUpdates();
    ~PendingUpdates();

    std::vector<base::OnceClosure> delegate_updates;
    bool site_data_accessed = false;
    AccessDetails access_details;
  };

  explicit PageSpecificContentSettings(content::Page& page, Delegate* delegate);

  // Updates `microphone_camera_state_` after audio/video is started/finished.
  void OnCapturingStateChangedInternal(ContentSettingsType type,
                                       bool is_capturing);

  // This method is called when a camera and/or mic blocked indicator is
  // displayed.
  void StartBlockedIndicatorTimer(ContentSettingsType type);

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

  // Clears settings changed by the user via PageInfo since the last navigation.
  void ClearContentSettingsChangedViaPageInfo();

  bool IsPagePrerendering() const;
  bool IsEmbeddedPage() const;

  // Delays the call of the delegate method if the page is currently
  // prerendering until the page is activated; directly calls the method
  // otherwise.
  template <typename DelegateMethod, typename... Args>
  void NotifyDelegate(DelegateMethod method, Args... args) {
    if (IsEmbeddedPage()) {
      return;
    }
    if (IsPagePrerendering()) {
      DCHECK(updates_queued_during_prerender_);
      updates_queued_during_prerender_->delegate_updates.emplace_back(
          base::BindOnce(method, base::Unretained(delegate_), args...));
      return;
    }
    (*delegate_.*method)(args...);
  }
  // Used to notify the parent page's PSCS of a content access.
  template <typename PSCSMethod, typename... Args>
  void MaybeUpdateParent(PSCSMethod method, Args... args) {
    if (IsEmbeddedPage()) {
      PageSpecificContentSettings* pscs =
          PageSpecificContentSettings::GetForFrame(
              page().GetMainDocument().GetParentOrOuterDocument());
      DCHECK(pscs);
      (*pscs.*method)(args...);
    }
  }

  // Notifies observers. Like |NotifyDelegate|, the notification is delayed for
  // prerendering pages until the page is activated. Embedded pages will not
  // notify observers directly and rely on the outermost page to do so.
  void MaybeNotifySiteDataObservers(const AccessDetails& access_details);

  // Tells the delegate to update the location bar. This method is a no-op if
  // the page is currently prerendering or is embedded.
  void MaybeUpdateLocationBar();

  content::WebContents* GetWebContents() const;

  // Document picture-in-picture allows changing content settings in both the
  // browser window and the PiP window. When the settings is changed in one
  // place, return the settings in another place to be synced as well. We should
  // update settings in either place at most once, so we will avoid getting into
  // deadlock by using |is_updating_synced_pscs_|.
  PageSpecificContentSettings* MaybeGetSyncedSettingsForPictureInPicture();

  // An auto reset variable to make sure we do not get into deadlock when
  // updating synced PageSpecificContentSettings for the document
  // picture-in-picture case.
  bool is_updating_synced_pscs_ = false;

  // If `false` PSCS is allowed to save a new blocked state for an activity
  // indicator. If `true` PSCS is frozen and a new blocked state will be
  // ignored. This variable is controlled by PermissionRequestManager to prevent
  // showing indicators after requests were cleaned up due to a new navigation.
  bool freeze_indicators_ = false;

  raw_ptr<Delegate> delegate_;

  struct ContentSettingsStatus {
    bool blocked;
    bool allowed;
  };
  // Stores which content setting types actually have blocked content.
  std::map<ContentSettingsType, ContentSettingsStatus> content_settings_status_;

  // Stores embedded sites that requested a permission. Only applies to
  // permissions that are scoped to two sites, e.g. StorageAccess.
  std::map<ContentSettingsType, std::map<net::SchemefulSite, bool>>
      content_settings_two_site_requests_;

  // Profile-bound, this will outlive this class (which is WebContents bound).
  raw_ptr<HostContentSettingsMap> map_;

  // Stores the count of stateful bounces during the navigation that led to this
  // page.
  int stateful_bounce_count_ = 0u;

  std::unique_ptr<BrowsingDataModel> allowed_browsing_data_model_;
  std::unique_ptr<BrowsingDataModel> blocked_browsing_data_model_;

  // The origin of the media stream request. Note that we only support handling
  // settings for one request per tab. The latest request's origin will be
  // stored here. http://crbug.com/259794
  // TODO(crbug.com/40276922): Remove `media_stream_access_origin_` and
  // calculate a proper origin internaly.
  GURL media_stream_access_origin_;

  // The microphone and camera state at the last media stream request.
  MicrophoneCameraState microphone_camera_state_;

  // Contains URLs which attempted to join interest groups. Note: The UI will
  // only currently show the top frame as having attempted to join.
  std::vector<url::Origin> allowed_interest_group_api_;
  std::vector<url::Origin> blocked_interest_group_api_;

  // Contains topics that were accessed by this page.
  base::flat_set<privacy_sandbox::CanonicalTopic> accessed_topics_;

  // The Geolocation, camera, and/or microphone permission was granted to this
  // origin from a permission prompt that was triggered by the currently active
  // document.
  bool camera_was_just_granted_on_site_level_ = false;
  bool mic_was_just_granted_on_site_level_ = false;
  bool geolocation_was_just_granted_on_site_level_ = false;

  bool notifications_was_denied_because_of_system_permission_ = false;

  // The time when the media indicator was displayed.
  base::TimeTicks media_indicator_time_;

  // Stores timers for delaying hiding an activity indicators.
  std::map<ContentSettingsType, base::OneShotTimer>
      indicators_hiding_delay_timer_;
  // Stores last used time when a permission-gate feature is no longer in use.
  std::map<ContentSettingsType, base::Time> last_used_time_;
  // Stores `ContentSettingsType` that is currently used by a page.
  std::set<ContentSettingsType> in_use_;

  // A timer to removed a blocked media indicator.
  std::map<ContentSettingsType, base::OneShotTimer>
      media_blocked_indicator_timer_;

  // Stores `ContentSettingsType` that is currently displaying. It is used only
  // for the Left-Hand Side indicators.
  std::set<ContentSettingsType> visible_indicators_;

  // Observer to watch for content settings changed.
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      observation_{this};

  // Stores content settings changed by the user via page info since the last
  // navigation. Used to determine whether to display the settings in page info.
  std::set<ContentSettingsType> content_settings_changed_via_page_info_;

  // Calls to |delegate_| and SiteDataObservers that have been queued up while
  // the page is prerendering. These calls are run when the page is activated.
  std::unique_ptr<PendingUpdates> updates_queued_during_prerender_;

  base::ObserverList<PermissionUsageObserver> permission_usage_observers_;

  PAGE_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<PageSpecificContentSettings> weak_factory_{this};
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_PAGE_SPECIFIC_CONTENT_SETTINGS_H_
