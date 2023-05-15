// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_mediator.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "components/webapps/browser/android/webapps_jni_headers/AddToHomescreenMediator_jni.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace webapps {

namespace {

// The length of time to allow the add to homescreen data fetcher to run before
// timing out and generating an icon.
const int kDataTimeoutInMilliseconds = 8000;

// These need to be kept the same order as in enums.xml.
enum class AppTypeToMenuEntry {
  kUnknownMenuEntryForWebApp,
  kAddToHomeScreenShownForWebApp,
  kInstallShownForWebApp,
  kUnknownMenuEntryForShortcut,
  kAddToHomeScreenShownForShortcut,
  kInstallShownForShortcut,
  kAppTypeFinalEntry,  // Must be last.
};

}  // namespace

// static
jlong JNI_AddToHomescreenMediator_Initialize(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_ref) {
  return reinterpret_cast<intptr_t>(new AddToHomescreenMediator(java_ref));
}

AddToHomescreenMediator::AddToHomescreenMediator(
    const JavaParamRef<jobject>& java_ref) {
  java_ref_.Reset(java_ref);
}

void AddToHomescreenMediator::StartForAppBanner(
    base::WeakPtr<AppBannerManager> weak_manager,
    std::unique_ptr<AddToHomescreenParams> params,
    base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                 const AddToHomescreenParams&)>
        event_callback) {
  weak_app_banner_manager_ = weak_manager;
  params_ = std::move(params);
  event_callback_ = std::move(event_callback);
  // Call UI_SHOWN early since the UI is already shown on Java coordinator
  // initialization.
  event_callback_.Run(AddToHomescreenInstaller::Event::UI_SHOWN, *params_);

  if (params_->app_type == AddToHomescreenParams::AppType::NATIVE) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AddToHomescreenMediator_setNativeAppInfo(env, java_ref_,
                                                  params_->native_app_data);
  } else {
    bool is_webapk =
        (params_->app_type == AddToHomescreenParams::AppType::WEBAPK);
    SetWebAppInfo(params_->shortcut_info->name, params_->shortcut_info->url,
                  is_webapk);
  }
  // In this code path (show A2HS dialog from app banner), a maskable primary
  // icon isn't padded yet. We'll need to pad it here.
  SetIcon(params_->primary_icon, params_->HasMaskablePrimaryIcon());
}

void AddToHomescreenMediator::StartForAppMenu(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents,
    int title_id) {
  title_id_ = title_id;
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  data_fetcher_ = std::make_unique<AddToHomescreenDataFetcher>(
      web_contents, kDataTimeoutInMilliseconds, this);

  // base::Unretained() is safe because the lifetime of this object is
  // controlled by its Java counterpart. It will be destroyed when the add to
  // home screen prompt is dismissed, which occurs after the last time
  // RecordEventForAppMenu() can be called.
  event_callback_ = base::BindRepeating(
      &AddToHomescreenMediator::RecordEventForAppMenu, base::Unretained(this));
}

void AddToHomescreenMediator::AddToHomescreen(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_user_title) {
  if (!params_ || GetWebContents() == nullptr)
    return;

  if (params_->app_type == AddToHomescreenParams::AppType::SHORTCUT) {
    params_->shortcut_info->user_title =
        base::android::ConvertJavaStringToUTF16(env, j_user_title);
  } else if (params_->app_type == AddToHomescreenParams::AppType::WEBAPK) {
    AppBannerManager* app_banner_manager =
        AppBannerManager::FromWebContents(GetWebContents());
    app_banner_manager->TrackInstallPath(/* bottom_sheet= */ false,
                                         params_->install_source);
  }

  AddToHomescreenInstaller::Install(GetWebContents(), *params_,
                                    event_callback_);
}

void AddToHomescreenMediator::OnUiDismissed(JNIEnv* env) {
  if (params_) {
    event_callback_.Run(AddToHomescreenInstaller::Event::UI_CANCELLED,
                        *params_);
  }
}

void AddToHomescreenMediator::OnNativeDetailsShown(JNIEnv* env) {
  event_callback_.Run(AddToHomescreenInstaller::Event::NATIVE_DETAILS_SHOWN,
                      *params_);
}

void AddToHomescreenMediator::Destroy(JNIEnv* env) {
  delete this;
}

AddToHomescreenMediator::~AddToHomescreenMediator() = default;

void AddToHomescreenMediator::SetIcon(const SkBitmap& display_icon,
                                      bool need_to_add_padding) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(!display_icon.drawsNothing());
  base::android::ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(display_icon);
  Java_AddToHomescreenMediator_setIcon(env, java_ref_, java_bitmap,
                                       params_->HasMaskablePrimaryIcon(),
                                       need_to_add_padding);
}

void AddToHomescreenMediator::SetWebAppInfo(const std::u16string& user_title,
                                            const GURL& url,
                                            bool is_webapk) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_user_title =
      base::android::ConvertUTF16ToJavaString(env, user_title);
  // Trim down the app URL to the origin. Elide cryptographic schemes so HTTP
  // is still shown.
  ScopedJavaLocalRef<jstring> j_url = base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  Java_AddToHomescreenMediator_setWebAppInfo(env, java_ref_, j_user_title,
                                             j_url, is_webapk /* isWebApk */);
}

void AddToHomescreenMediator::OnUserTitleAvailable(
    const std::u16string& user_title,
    const GURL& url,
    bool is_webapk_compatible) {
  SetWebAppInfo(user_title, url, is_webapk_compatible);
}

void AddToHomescreenMediator::OnDataAvailable(
    const ShortcutInfo& info,
    const SkBitmap& display_icon,
    const InstallableStatusCode status_code) {
  params_ = std::make_unique<AddToHomescreenParams>();
  params_->app_type = info.source == ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_PWA
                          ? AddToHomescreenParams::AppType::WEBAPK
                          : AddToHomescreenParams::AppType::SHORTCUT;
  params_->shortcut_info = std::make_unique<ShortcutInfo>(info);
  params_->primary_icon = data_fetcher_->primary_icon();
  params_->install_source = InstallableMetrics::GetInstallSource(
      data_fetcher_->web_contents(), InstallTrigger::MENU);
  params_->installable_status = status_code;

  // AddToHomescreenMediator::OnDataAvailable() is called in the code path
  // to show A2HS dialog from app menu. In this code path, display_icon is
  // already correctly padded if it's maskable.
  SetIcon(display_icon, false /*need_to_add_padding*/);

  // Log what was shown in the App menu and what action was taken here.
  bool is_webapk = params_->app_type == AddToHomescreenParams::AppType::WEBAPK;
  auto entry = AppTypeToMenuEntry::kAppTypeFinalEntry;

  DCHECK_NE(-1, title_id_);
  switch (title_id_) {
    case AppBannerSettingsHelper::APP_MENU_OPTION_UNKNOWN: {
      entry = is_webapk ? AppTypeToMenuEntry::kUnknownMenuEntryForWebApp
                        : AppTypeToMenuEntry::kUnknownMenuEntryForShortcut;
      break;
    }
    case AppBannerSettingsHelper::APP_MENU_OPTION_ADD_TO_HOMESCREEN: {
      entry = is_webapk ? AppTypeToMenuEntry::kAddToHomeScreenShownForWebApp
                        : AppTypeToMenuEntry::kAddToHomeScreenShownForShortcut;
      break;
    }
    case AppBannerSettingsHelper::APP_MENU_OPTION_INSTALL: {
      entry = is_webapk ? AppTypeToMenuEntry::kInstallShownForWebApp
                        : AppTypeToMenuEntry::kInstallShownForShortcut;
      break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Webapp.AddToHomescreenMediator.AppTypeToMenuEntry",
                            entry, AppTypeToMenuEntry::kAppTypeFinalEntry);

  if (is_webapk) {
    webapps::WebappsClient::Get()->OnWebApkInstallInitiatedFromAppMenu(
        data_fetcher_->web_contents());
  }
}

void AddToHomescreenMediator::RecordEventForAppMenu(
    AddToHomescreenInstaller::Event event,
    const AddToHomescreenParams& a2hs_params) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents ||
      a2hs_params.app_type == AddToHomescreenParams::AppType::NATIVE) {
    return;
  }

  if (event == AddToHomescreenInstaller::Event::INSTALL_REQUEST_FINISHED) {
    AppBannerManager* app_banner_manager =
        AppBannerManager::FromWebContents(web_contents);
    // Fire the appinstalled event and do install time logging.
    if (app_banner_manager) {
      app_banner_manager->OnInstall(a2hs_params.shortcut_info->display);
    }
  }
}

content::WebContents* AddToHomescreenMediator::GetWebContents() {
  if (weak_app_banner_manager_.get())
    return weak_app_banner_manager_->web_contents();

  if (data_fetcher_)
    return data_fetcher_->web_contents();

  return nullptr;
}

}  // namespace webapps
