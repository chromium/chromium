// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_COORDINATOR_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_COORDINATOR_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"
#include "components/webapps/browser/android/add_to_homescreen_mediator.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"

namespace webapps {

struct AddToHomescreenParams;
class AppBannerManager;

// AddToHomescreenCoordinator is the C++ counterpart of org.chromium.chrome.
// browser.webapps.addtohomescreen.AddToHomescreenCoordinator in Java.
class AddToHomescreenCoordinator : public AddToHomescreenDataFetcher::Observer {
 public:
  // Called from startForAppMenu() (JNI method) to start fetching the
  // information about the given WebContents by creating an
  // AddToHomescreenDataFetcher instance.
  AddToHomescreenCoordinator(
      content::WebContents* web_contents,
      int app_menu_type,
      const base::android::JavaParamRef<jobject>& java_coordinator);

  // Called from the Java side and destructs this object.
  void Destroy(JNIEnv* env);

  // Called for showing the add-to-homescreen UI for AppBannerManager.
  static bool ShowForAppBanner(
      base::WeakPtr<AppBannerManager> weak_manager,
      std::unique_ptr<AddToHomescreenParams> params,
      base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                   const AddToHomescreenParams&)>
          event_callback);

  AddToHomescreenCoordinator() = delete;
  AddToHomescreenCoordinator(const AddToHomescreenCoordinator&) = delete;
  AddToHomescreenCoordinator& operator=(const AddToHomescreenCoordinator&) =
      delete;

 private:
  ~AddToHomescreenCoordinator() override;

  // AddToHomescreenDataFetcher::Observer overrides:
  void OnUserTitleAvailable(const std::u16string& user_title,
                            const GURL& url,
                            AddToHomescreenParams::AppType app_type) override;
  void OnDataAvailable(const ShortcutInfo& info,
                       const SkBitmap& display_icon,
                       AddToHomescreenParams::AppType app_type,
                       InstallableStatusCode status_code) override;

  base::android::ScopedJavaGlobalRef<jobject> java_coordinator_;

  // These are used only in the startForAppMenu() flow.
  std::unique_ptr<AddToHomescreenDataFetcher> data_fetcher_;
  raw_ptr<AddToHomescreenMediator> mediator_ = nullptr;
  int app_menu_type_ = AppBannerSettingsHelper::APP_MENU_OPTION_UNKNOWN;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_COORDINATOR_H_
