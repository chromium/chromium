// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_mediator.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/webapps_jni_headers/AddToHomescreenMediator_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace webapps {

// static
static jlong JNI_AddToHomescreenMediator_Initialize(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_ref,
    const JavaParamRef<jobject>& java_web_contents) {
  return reinterpret_cast<intptr_t>(
      new AddToHomescreenMediator(java_ref, java_web_contents));
}

AddToHomescreenMediator::AddToHomescreenMediator(
    const JavaParamRef<jobject>& java_ref,
    const JavaParamRef<jobject>& java_web_contents) {
  java_ref_.Reset(java_ref);

  web_contents_ = content::WebContents::FromJavaWebContents(java_web_contents)
                      ->GetWeakPtr();
}

void AddToHomescreenMediator::StartForAppBanner(
    std::unique_ptr<AddToHomescreenParams> params,
    base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                 const AddToHomescreenParams&)>
        event_callback) {
  params_ = std::move(params);
  event_callback_ = std::move(event_callback);
  // Call UI_SHOWN early since the UI is already shown on Java coordinator
  // initialization.
  event_callback_.Run(AddToHomescreenInstaller::Event::UI_SHOWN, *params_);

  if (params_->app_type == AppType::NATIVE) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AddToHomescreenMediator_setNativeAppInfo(env, java_ref_,
                                                  params_->native_app_data);
  } else {
    SetWebAppInfo(params_->shortcut_info->name, params_->shortcut_info->url,
                  params_->app_type);
  }
  // In this code path (show A2HS dialog from app banner), a maskable primary
  // icon isn't padded yet. We'll need to pad it here.
  SetIcon(params_->primary_icon);
}

void AddToHomescreenMediator::OnAppMetadataAvailable(
    const std::u16string& user_title,
    const GURL& url,
    AddToHomescreenParams::AppType app_type) {
  // base::Unretained() is safe because the lifetime of this object is
  // controlled by its Java counterpart. It will be destroyed when the add to
  // home screen prompt is dismissed, which occurs after the last time
  // RecordEventForAppMenu() can be called.
  event_callback_ = base::BindRepeating(
      &AddToHomescreenMediator::RecordEventForAppMenu, base::Unretained(this));

  SetWebAppInfo(user_title, url, app_type);
}

void AddToHomescreenMediator::OnFullAppDataAvailable(
    std::unique_ptr<AddToHomescreenParams> params) {
  params_ = std::move(params);

  SetIcon(params_->primary_icon);

  if (params_->IsWebApk()) {
    webapps::WebappsClient::Get()->OnWebApkInstallInitiatedFromAppMenu(
        web_contents_.get());
  }
}

void AddToHomescreenMediator::AddToHomescreen(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_user_title) {
  if (!params_ || !web_contents_) {
    return;
  }

  if (params_->app_type == AppType::SHORTCUT ||
      params_->app_type == AppType::WEBAPK_DIY) {
    params_->shortcut_info->user_title =
        base::android::ConvertJavaStringToUTF16(env, j_user_title);
    params_->shortcut_info->has_custom_title = true;
  }

  // Shortcuts always open in a browser tab.
  if (params_->app_type == AppType::SHORTCUT) {
    params_->shortcut_info->display = blink::mojom::DisplayMode::kBrowser;
  }

  if (params_->IsWebApk()) {
    PwaInstallPathTracker::TrackInstallPath(/* bottom_sheet= */ false,
                                            params_->install_source);
  }

  AddToHomescreenInstaller::Install(web_contents_.get(), *params_,
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

void AddToHomescreenMediator::SetIcon(const SkBitmap& display_icon) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(!display_icon.drawsNothing());
  base::android::ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(display_icon);
  Java_AddToHomescreenMediator_setIcon(env, java_ref_, java_bitmap,
                                       params_->HasMaskablePrimaryIcon());
}

void AddToHomescreenMediator::SetWebAppInfo(const std::u16string& user_title,
                                            const GURL& url,
                                            AppType app_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_user_title =
      base::android::ConvertUTF16ToJavaString(env, user_title);
  // Trim down the app URL to the origin. Elide cryptographic schemes so HTTP
  // is still shown.
  ScopedJavaLocalRef<jstring> j_url = base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

  Java_AddToHomescreenMediator_setWebAppInfo(env, java_ref_, j_user_title,
                                             j_url, static_cast<int>(app_type));
}

void AddToHomescreenMediator::RecordEventForAppMenu(
    AddToHomescreenInstaller::Event event,
    const AddToHomescreenParams& a2hs_params) {
  if (!web_contents_ || a2hs_params.app_type == AppType::NATIVE) {
    return;
  }

  if (event == AddToHomescreenInstaller::Event::INSTALL_REQUEST_FINISHED) {
    AppBannerManager* app_banner_manager =
        AppBannerManager::FromWebContents(web_contents_.get());
    // Fire the appinstalled event and do install time logging.
    if (app_banner_manager) {
      app_banner_manager->OnInstall(
          a2hs_params.shortcut_info->display,
          /*set_current_web_app_not_installable=*/false);
    }
  }
}

}  // namespace webapps

DEFINE_JNI(AddToHomescreenMediator)
