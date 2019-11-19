// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace web_app {

class WebAppProviderBase;

// Per-tab web app helper. Allows to associate a tab (web page) with a web app
// (or legacy bookmark app).
class WebAppTabHelper : public content::WebContentsObserver,
                        public content::WebContentsUserData<WebAppTabHelper>,
                        public AppRegistrarObserver {
 public:
  using content::WebContentsUserData<WebAppTabHelper>::CreateForWebContents;
  using content::WebContentsUserData<WebAppTabHelper>::FromWebContents;

  explicit WebAppTabHelper(content::WebContents* web_contents);
  ~WebAppTabHelper() override;

  const AppId& app_id() const { return app_id_; }

  // Set associated app_id.
  void SetAppId(const AppId& app_id);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidCloneToNewWebContents(
      content::WebContents* old_web_contents,
      content::WebContents* new_web_contents) override;

  // These methods require an app associated with the tab (valid app_id()).
  //
  // Returns true if the app was installed by user, false if default installed.
  bool IsUserInstalled() const;
  // For user-installed apps:
  // Returns true if the app was installed through the install button.
  // Returns false if the app was installed through the create shortcut button.
  bool IsFromInstallButton() const;

 private:
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  friend class WebAppAudioFocusBrowserTest;
  friend class content::WebContentsUserData<WebAppTabHelper>;

  // Returns whether the associated web contents belongs to an app window.
  bool IsInAppWindow() const;

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& installed_app_id) override;
  void OnWebAppUninstalled(const AppId& uninstalled_app_id) override;
  void OnAppRegistrarShutdown() override;
  void OnAppRegistrarDestroyed() override;

  void ResetAppId();

  // Runs any logic when the associated app either changes or is removed.
  void OnAssociatedAppChanged();

  // Updates the audio focus group id based on the current web app.
  void UpdateAudioFocusGroupId();

  // Triggers a reinstall of a placeholder app for |url|.
  void ReinstallPlaceholderAppIfNecessary(const GURL& url);

  AppId FindAppIdWithUrlInScope(const GURL& url) const;

  // WebApp associated with this tab. Empty string if no app associated.
  AppId app_id_;

  // The audio focus group id is used to group media sessions together for apps.
  // We store the applied group id locally on the helper for testing.
  base::UnguessableToken audio_focus_group_id_ = base::UnguessableToken::Null();

  ScopedObserver<AppRegistrar, AppRegistrarObserver> observer_{this};
  WebAppProviderBase* provider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebAppTabHelper);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_H_
