// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace webapps {
class LaunchQueue;
}

namespace web_app {

class WebAppProvider;

// Per-tab web app helper. Allows to associate a tab (web page) with a web app.
class WebAppTabHelper : public content::WebContentsUserData<WebAppTabHelper>,
                        public content::WebContentsObserver,
                        public WebAppInstallManagerObserver {
 public:
  // `contents` can be different from `tab->GetContents()` during tab discard.
  // TODO(https://crbug.com/347770670): This method can be simplified to not
  // take `contents`.
  static void Create(tabs::TabInterface* tab, content::WebContents* contents);

  // Retrieves the WebAppTabHelper's app ID off |web_contents|, returns
  // nullptr if there is no tab helper or app ID.
  static const webapps::AppId* GetAppId(
      const content::WebContents* web_contents);

#if BUILDFLAG(IS_MAC)
  // Like the above method, but also checks if notification attribution should
  // apply to the app in the web contents. This checks the base::Feature as well
  // as makes sure the app is installed.
  static std::optional<webapps::AppId> GetAppIdForNotificationAttribution(
      content::WebContents* web_contents);
#endif

  WebAppTabHelper(tabs::TabInterface* tab, content::WebContents* contents);
  WebAppTabHelper(const WebAppTabHelper&) = delete;
  WebAppTabHelper& operator=(const WebAppTabHelper&) = delete;
  ~WebAppTabHelper() override;

  // Sets the app id for this web contents. Ideally the app id would always be
  // equal to the id of whatever app the last committed primary main frame URL
  // is in scope for (and WebAppTabHelper resets it to that any time a
  // navigation commits), but for legacy reasons sometimes the app id is set
  // explicitly from elsewhere.
  void SetAppId(std::optional<webapps::AppId> app_id);

  // Called by `WebAppBrowserController` and `WebKioskBrowserControllerBase`'s
  // `OnTabInserted` and `OnTabRemoved` methods to indicate if this web contents
  // is currently being displayed inside an app window. `window_app_id` is the
  // id of the app.
  void SetIsInAppWindow(std::optional<webapps::AppId> window_app_id);

  void SetCallbackToRunOnTabChanges(base::OnceClosure callback);

  // Used to listen to the tab entering the background via the `TabInterface`.
  void OnTabBackgrounded(tabs::TabInterface* tab_interface);

  // Used to listen to the tab being detached from the tab strip via the
  // `TabInterface`. The tab will either be destroyed, or is in the middle of
  // being put in a different window.
  void OnTabDetached(tabs::TabInterface* tab_interface,
                     tabs::TabInterface::DetachReason detach_reason);

  const base::UnguessableToken& GetAudioFocusGroupIdForTesting() const;

  // Returns the installed web app that 'controls' the last committed url of
  // this tab. This is populated for this tab no matter where it is, whether in
  // a browser window, or in a standalone app window.
  // - 'controls' means it's the web app who's scope contains the last committed
  //    url. If there are multiple web apps that satisfy this constraint, then
  //    it chooses the one with the longest (aka most specific) scope prefix.
  // - 'installed' means the web app is
  //   InstallState::INSTALLED_WITH_OS_INTEGRATION or
  //   InstallState::INSTALLED_WITHOUT_OS_INTEGRATION (which is usually only
  //   preinstalled apps). And thus this excludes the
  //   InstallState::SUGGESTED_FROM_ANOTHER_DEVICE state.
  //
  // Note: This is populated on construction from the current tab's
  // `GetLastCommittedURL()`, and afterwards only after navigation is committed.
  //
  // Note: If we are in an app window, this is not guaranteed to match
  // `window_app_id()` - for example, if the web contents of an app navigates
  // out of scope of the app, this will be std::nullopt.
  const std::optional<webapps::AppId>& app_id() const { return app_id_; }

  // Returns the installed web app window that contains this tab, or
  // std::nullopt if this tab is in a normal browser window. This is not
  // guaranteed to match `app_id()`, because app windows can display content
  // that is out of scope of the app (and even in scope of another app).
  const std::optional<webapps::AppId>& window_app_id() const {
    return window_app_id_;
  }

  // True when this web contents is currently being displayed inside an app
  // window instead of in a browser tab.
  bool is_in_app_window() const { return window_app_id_.has_value(); }

  bool is_pinned_home_tab() const { return is_pinned_home_tab_; }
  void set_is_pinned_home_tab(bool is_pinned_home_tab) {
    is_pinned_home_tab_ = is_pinned_home_tab;
  }

  webapps::LaunchQueue& EnsureLaunchQueue();

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // Because the launch queue is communicated via a dedicated mojo pipe,
  // ordering can be tricky in tests. This method allows tests to call
  // `FlushForTesting()` on the launch queue mojo connection to ensure that all
  // launch queue messages have been sent to the renderer.
  void FlushLaunchQueueForTesting() const;

  // Returns if the current web contents can be used for the 'focus-existing'
  // behavior of navigation capturing, where the tab is focused and a
  // 'LaunchParams' is given to a javascript 'launch consumer' on the page. This
  // returns if the current page can feasibly run javascript to actually set
  // this launch consumer, as without that, any captured links would simply do
  // nothing.
  // Specifically, this turns `true` if the current page's mime-type is html or
  // xhtml.
  bool CanBeUsedForFocusExisting() const;

 private:
  friend class WebAppAudioFocusBrowserTest;
  friend class content::WebContentsUserData<WebAppTabHelper>;

  // WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& installed_app_id) override;
  void OnWebAppWillBeUninstalled(
      const webapps::AppId& uninstalled_app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  void ResetTabSubscriptions(tabs::TabInterface* tab);

  // Sets the state of this tab helper. This will call
  // `WebAppUiManager::OnAssociatedAppChanged` if the id has changed, and
  // `UpdateAudioFocusGroupId()` if either has changed.
  void SetState(std::optional<webapps::AppId> app_id,
                std::optional<webapps::AppId> window_app_id);

  // Runs any logic when the associated app is added, changed or removed.
  void OnAssociatedAppChanged(
      const std::optional<webapps::AppId>& previous_app_id,
      const std::optional<webapps::AppId>& new_app_id);

  // Updates the audio focus group id based on the current web app.
  void UpdateAudioFocusGroupId();

  // Triggers a reinstall of a placeholder app for |url|.
  void ReinstallPlaceholderAppIfNecessary(const GURL& url);

  // When a `TabInterface` is updated on being detached and attached to a new
  // window, update the subscriptions as needed.
  void SubscribeToTabState(tabs::TabInterface* tab_interface);

  // Asynchronously run `on_tab_details_changed_callback_` after tab states have
  // changed.
  void MaybeNotifyTabChanged();

  // Cache the information that an app launch has happened and a WebFeature use
  // counter needs to be measured. Trigger measurement which may or may not
  // happen depending on whether page load has finished.
  void ScheduleManifestAppliedUseCounter();

  // Record the `UseCounter` for an app launch after page loading has
  // completed. Resets all flags post `UseCounter` measurement so that this
  // happens only once.
  void MaybeRecordManifestAppliedUseCounter();

  std::optional<webapps::AppId> app_id_;
  std::optional<webapps::AppId> window_app_id_;

  // True when this tab is the pinned home tab of a tabbed web app.
  bool is_pinned_home_tab_ = false;

  // The audio focus group id is used to group media sessions together for apps.
  // We store the applied group id locally on the helper for testing.
  base::UnguessableToken audio_focus_group_id_ = base::UnguessableToken::Null();

  // Use unique_ptr for lazy instantiation as most browser tabs have no need to
  // incur this memory overhead.
  std::unique_ptr<webapps::LaunchQueue> launch_queue_;

  // A callback that runs whenever the `tab` is destroyed, navigates or goes to
  // the background.
  base::OnceClosure on_tab_details_changed_callback_;

  // Used to subscribe to various changes happening in the current tab from the
  // `TabInterface`.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Listen to whether page load has completed in the web contents to measure
  // the `UseCounter` of launching a web app.
  bool can_record_manifest_applied_ = false;

  // Cache the information that an app launch `UseCounter` needs to be measured.
  bool meaure_manifest_applied_use_counter_ = false;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      observation_{this};
  raw_ptr<WebAppProvider> provider_ = nullptr;

  base::WeakPtrFactory<WebAppTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_
