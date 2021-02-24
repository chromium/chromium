// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_

#include "base/scoped_observer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace web_app {

class WebAppProviderBase;

// Per-tab web app helper. Allows to associate a tab (web page) with a web app
// (or legacy bookmark app).
class WebAppTabHelper : public WebAppTabHelperBase,
                        public content::WebContentsObserver,
                        public AppRegistrarObserver {
 public:
  static void CreateForWebContents(content::WebContents* contents);

  explicit WebAppTabHelper(content::WebContents* web_contents);
  WebAppTabHelper(const WebAppTabHelper&) = delete;
  WebAppTabHelper& operator=(const WebAppTabHelper&) = delete;
  ~WebAppTabHelper() override;

  // WebAppTabHelperBase:
  const AppId& GetAppId() const override;
  void SetAppId(const AppId& app_id) override;
  const base::UnguessableToken& GetAudioFocusGroupIdForTesting() const override;
  bool HasLoadedNonAboutBlankPage() const override;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void DidCloneToNewWebContents(
      content::WebContents* old_web_contents,
      content::WebContents* new_web_contents) override;

 private:
  friend class WebAppAudioFocusBrowserTest;
  friend class content::WebContentsUserData<WebAppTabHelper>;

  // Returns whether the associated web contents belongs to an app window.
  bool IsInAppWindow() const;

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& installed_app_id) override;
  void OnWebAppWillBeUninstalled(const AppId& uninstalled_app_id) override;
  void OnAppRegistrarShutdown() override;
  void OnAppRegistrarDestroyed() override;

  void ResetAppId();

  // Runs any logic when the associated app either changes or is removed.
  void OnAssociatedAppChanged(const AppId& previous_app_id,
                              const AppId& new_app_id);

  // Updates the audio focus group id based on the current web app.
  void UpdateAudioFocusGroupId();

  // Triggers a reinstall of a placeholder app for |url|.
  void ReinstallPlaceholderAppIfNecessary(const GURL& url);

  AppId FindAppIdWithUrlInScope(const GURL& url) const;

  // WebApp associated with this tab. Empty string if no app associated.
  AppId app_id_;

  // Indicates if the current page is an error page (e.g. the page failed to
  // load). We store this because it isn't accessible off a |WebContents| or a
  // |RenderFrameHost|.
  bool is_error_page_ = false;

  // The audio focus group id is used to group media sessions together for apps.
  // We store the applied group id locally on the helper for testing.
  base::UnguessableToken audio_focus_group_id_ = base::UnguessableToken::Null();

  bool has_loaded_non_about_blank_page_ = false;

  ScopedObserver<AppRegistrar, AppRegistrarObserver> observer_{this};
  WebAppProviderBase* provider_ = nullptr;

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_
