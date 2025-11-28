// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_coordinator.h"

#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"
#include "components/webapps/browser/android/add_to_homescreen_mediator.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/webapps_jni_headers/AddToHomescreenCoordinator_jni.h"

using base::android::JavaParamRef;

namespace webapps {

namespace {

// The length of time to allow the add to homescreen data fetcher to run before
// timing out and generating an icon.
const int kDataTimeoutInMilliseconds = 8000;

}  // namespace

AddToHomescreenCoordinator::AddToHomescreenCoordinator(
    content::WebContents* web_contents,
    int app_menu_type,
    const JavaParamRef<jobject>& java_coordinator) {
  app_menu_type_ = app_menu_type;
  java_coordinator_ = java_coordinator;

  data_fetcher_ = std::make_unique<AddToHomescreenDataFetcher>(
      web_contents, kDataTimeoutInMilliseconds, this);
}

void AddToHomescreenCoordinator::OnUserTitleAvailable(
    const std::u16string& user_title,
    const GURL& url,
    AddToHomescreenParams::AppType app_type) {
  // TODO: crbug.com/449581904 - Skip the creation of mediator and view if
  // auto-minted TWA will be installed.

  JNIEnv* env = base::android::AttachCurrentThread();
  mediator_ = reinterpret_cast<AddToHomescreenMediator*>(
      Java_AddToHomescreenCoordinator_buildMediatorAndShowDialog(
          env, java_coordinator_));
  CHECK(mediator_);

  if (app_menu_type_ ==
      AppBannerSettingsHelper::APP_MENU_OPTION_ADD_TO_HOMESCREEN) {
    // The user triggered this flow via the Universal Install dialog and
    // explicitly requested Add a shortcut (not Install). Therefore we must
    // ask the Java install dialog to treat this as a shortcut and not a
    // webapp.
    app_type = AppType::SHORTCUT;
  }

  mediator_->OnAppMetadataAvailable(user_title, url, app_type);
}

void AddToHomescreenCoordinator::OnDataAvailable(
    const ShortcutInfo& info,
    const SkBitmap& display_icon,
    AddToHomescreenParams::AppType app_type,
    InstallableStatusCode status_code) {
  // OnUserTitleAvailable should be called beforehand.
  CHECK(mediator_);

  auto params = std::make_unique<AddToHomescreenParams>(
      app_type, std::make_unique<ShortcutInfo>(info), display_icon, status_code,
      InstallableMetrics::GetInstallSource(data_fetcher_->web_contents(),
                                           InstallTrigger::MENU));

  mediator_->OnFullAppDataAvailable(std::move(params));
}

AddToHomescreenCoordinator::~AddToHomescreenCoordinator() = default;

void AddToHomescreenCoordinator::Destroy(JNIEnv* env) {
  delete this;
}

// static
bool AddToHomescreenCoordinator::ShowForAppBanner(
    base::WeakPtr<AppBannerManager> weak_manager,
    std::unique_ptr<AddToHomescreenParams> params,
    base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                 const AddToHomescreenParams&)>
        event_callback) {
  // Don't start if app info is not available.
  if ((params->app_type == AddToHomescreenParams::AppType::NATIVE &&
       params->native_app_data.is_null()) ||
      (params->app_type == AddToHomescreenParams::AppType::WEBAPK &&
       !params->shortcut_info)) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  AddToHomescreenMediator* mediator = (AddToHomescreenMediator*)
      Java_AddToHomescreenCoordinator_initMvcAndReturnMediator(
          env, weak_manager->web_contents()->GetJavaWebContents());
  if (!mediator)
    return false;

  mediator->StartForAppBanner(std::move(params), std::move(event_callback));
  return true;
}

// static
jlong JNI_AddToHomescreenCoordinator_StartForAppMenu(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_coordinator,
    const base::android::JavaParamRef<jobject>& java_web_contents,
    int app_menu_type) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  return reinterpret_cast<intptr_t>(new AddToHomescreenCoordinator(
      web_contents, app_menu_type, java_coordinator));
}

}  // namespace webapps

DEFINE_JNI(AddToHomescreenCoordinator)
