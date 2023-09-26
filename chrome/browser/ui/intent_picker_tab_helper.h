// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/models/image_model.h"
#include "url/origin.h"

// Controls the visibility of IntentPickerView by updating the visibility based
// on stored state. This class is instantiated for both web apps and SWAs.
class IntentPickerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<IntentPickerTabHelper>,
      public web_app::WebAppInstallManagerObserver {
 public:
  IntentPickerTabHelper(const IntentPickerTabHelper&) = delete;
  IntentPickerTabHelper& operator=(const IntentPickerTabHelper&) = delete;

  ~IntentPickerTabHelper() override;

  // Displays the intent picker icon in the omnibox, based on the last committed
  // URL in |web_contents|.
  static void MaybeShowIntentPickerIcon(content::WebContents* web_contents);

  // Shows the intent picker bubble to present a choice between apps to handle
  // |url|. May launch directly into an app based on user preferences and
  // installed apps.
  static void ShowIntentPickerBubbleOrLaunchApp(
      content::WebContents* web_contents,
      const GURL& url);

  // Shows or hides the intent picker icon for |web_contents|. Always shows a
  // generic picker icon, even if MaybeShowIconForApps() had previously applied
  // app-specific customizations.
  static void ShowOrHideIcon(content::WebContents* web_contents,
                             bool should_show_icon);

  // Shows or hides the intent picker icon for this tab a list of |apps| which
  // can handle a link intent. Visible for testing.
  void MaybeShowIconForApps(std::vector<apps::IntentPickerAppInfo> apps);

  bool should_show_icon() const { return should_show_icon_; }

  // Returns true if the icon should be shown using an expanded chip-style
  // button.
  bool ShouldShowExpandedChip() const {
    return show_expanded_chip_from_usage_ || current_app_is_preferred_;
  }

  const ui::ImageModel& app_icon() const { return current_app_icon_; }


  // Sets a OnceClosure callback which will be called next time the icon is
  // updated. If include_latest_navigation is true, and the latest navigation
  // was finished, the callback is called immediately.
  void SetIconUpdateCallbackForTesting(base::OnceClosure callback,
                                       bool include_latest_navigation = false);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  explicit IntentPickerTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<IntentPickerTabHelper>;

  using IntentPickerIconLoaderCallback =
      base::OnceCallback<void(std::vector<apps::IntentPickerAppInfo> apps)>;

  // Logic to load icons etc for apps in the intent picker:

  void OnAppIconLoaded(std::vector<apps::IntentPickerAppInfo> apps,
                       IntentPickerIconLoaderCallback callback,
                       size_t index,
                       apps::IconValuePtr icon_value);

  void LoadAppIcon(std::vector<apps::IntentPickerAppInfo> apps,
                   size_t index,
                   IntentPickerIconLoaderCallback callback);

  void UpdateExpandedState(bool should_show_icon);
  void OnAppIconLoadedForChip(const std::string& app_id,
                              apps::IconValuePtr icon);
  // Shows or hides the intent icon, with customizations specific to link intent
  // handling.
  void ShowIconForLinkIntent(bool should_show_icon);
  void ShowOrHideIconInternal(bool should_show_icon);

  // Logic to launch the app from the intent picker:

  void ShowIntentPickerOrLaunchAppImpl(
      const GURL& url,
      std::vector<apps::IntentPickerAppInfo> apps);

  void OnIntentPickerClosedMaybeLaunch(
      const GURL& url,
      const std::string& launch_name,
      apps::PickerEntryType entry_type,
      apps::IntentPickerCloseReason close_reason,
      bool should_persist);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // web_app::WebAppInstallManagerObserver:
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  const raw_ptr<web_app::WebAppRegistrar, DanglingUntriaged> registrar_;
  const raw_ptr<web_app::WebAppInstallManager> install_manager_;

  bool should_show_icon_ = false;
  bool icon_resolved_after_last_navigation_ = false;
  url::Origin last_shown_origin_;
  // True if the icon should be shown as in an expanded chip style due to usage
  // on this origin.
  bool show_expanded_chip_from_usage_ = false;

  // Tracks the number of commits on this page, to allow for checking to make
  // sure that asynchronous invocations do not cause a stale intent picker.
  int commit_count_ = 0;

  // Contains the app ID of an app which can be opened through the intent
  // picker. This is only set when MaybeShowIconForApps() is called with a
  // single app. Will be set to the empty string in all other cases (e.g. when
  // there are multiple apps available, or when the icon is not visible).
  std::string current_app_id_;
  // True if |current_app_id_| is set as the preferred app for its http/https
  // links.
  bool current_app_is_preferred_ = false;
  ui::ImageModel current_app_icon_;

  base::OnceClosure icon_update_closure_;

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  // This weak ptr factory is invalidated when a new navigation finishes.
  base::WeakPtrFactory<IntentPickerTabHelper> per_navigation_weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_
