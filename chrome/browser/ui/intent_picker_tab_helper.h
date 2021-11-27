// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Controls the visibility of IntentPickerView by updating the visibility based
// on stored state. This class is instantiated for both web apps and SWAs.
class IntentPickerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<IntentPickerTabHelper>,
      public web_app::AppRegistrarObserver {
 public:
  IntentPickerTabHelper(const IntentPickerTabHelper&) = delete;
  IntentPickerTabHelper& operator=(const IntentPickerTabHelper&) = delete;

  ~IntentPickerTabHelper() override;

  static void SetShouldShowIcon(content::WebContents* web_contents,
                                bool should_show_icon);

  bool should_show_icon() const { return should_show_icon_; }

  using IntentPickerIconLoaderCallback =
      base::OnceCallback<void(std::vector<apps::IntentPickerAppInfo> apps)>;

  // Load multiple app icons from App Service.
  static void LoadAppIcons(content::WebContents* web_contents,
                           std::vector<apps::IntentPickerAppInfo> apps,
                           IntentPickerIconLoaderCallback callback);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  explicit IntentPickerTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<IntentPickerTabHelper>;

  void OnAppIconLoaded(std::vector<apps::IntentPickerAppInfo> apps,
                       IntentPickerIconLoaderCallback callback,
                       size_t index,
                       apps::IconValuePtr icon_value);

  void LoadAppIcon(std::vector<apps::IntentPickerAppInfo> apps,
                   IntentPickerIconLoaderCallback callback,
                   size_t index);

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // web_app::AppRegistrarObserver:
  void OnWebAppWillBeUninstalled(const web_app::AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  const raw_ptr<web_app::WebAppRegistrar> registrar_;

  bool should_show_icon_ = false;

  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::AppRegistrarObserver>
      registrar_observation_{this};

  base::WeakPtrFactory<IntentPickerTabHelper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_
