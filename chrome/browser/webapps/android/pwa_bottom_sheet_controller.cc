// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/android/pwa_bottom_sheet_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/banners/app_banner_manager_android.h"
#include "chrome/browser/webapps/android/features.h"
#include "chrome/browser/webapps/android/jni_headers/PwaBottomSheetControllerProvider_jni.h"
#include "chrome/browser/webapps/android/jni_headers/PwaBottomSheetController_jni.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/android/java_bitmap.h"

using base::ASCIIToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

bool CanShowBottomSheet(content::WebContents* web_contents,
                        const base::string16& description,
                        const std::vector<base::string16>& categories,
                        const std::map<GURL, SkBitmap>& screenshots) {
  if (!base::FeatureList::IsEnabled(
          webapps::features::kPwaInstallUseBottomSheet))
    return false;

  if (description.size() == 0 || categories.size() == 0 ||
      screenshots.size() == 0)
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PwaBottomSheetControllerProvider_canShowPwaBottomSheetInstaller(
      env, web_contents->GetJavaWebContents());
}

}  // anonymous namespace

namespace webapps {

PwaBottomSheetController::~PwaBottomSheetController() = default;

// static
void JNI_PwaBottomSheetController_CreateAndShowBottomSheetInstaller(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  AppBannerManagerAndroid* app_banner_manager =
      AppBannerManagerAndroid::FromWebContents(web_contents);

  const blink::Manifest& manifest = app_banner_manager->manifest();
  PwaBottomSheetController::MaybeCreateAndShow(
      nullptr, web_contents, app_banner_manager->GetAppName(),
      app_banner_manager->primary_icon(),
      app_banner_manager->has_maskable_primary_icon(),
      app_banner_manager->validated_url(), app_banner_manager->screenshots(),
      manifest.description.value_or(base::string16()), manifest.categories,
      /* show_expanded= */ true);
}

// static
void PwaBottomSheetController::MaybeCreateAndShow(
    base::WeakPtr<InstallableAmbientBadgeInfoBarDelegate::Client> weak_client,
    content::WebContents* web_contents,
    const base::string16& app_name,
    const SkBitmap& primary_icon,
    const bool is_primary_icon_maskable,
    const GURL& start_url,
    const std::map<GURL, SkBitmap>& screenshots,
    const base::string16& description,
    const std::vector<base::string16>& categories,
    bool show_expanded) {
  if (CanShowBottomSheet(web_contents, description, categories, screenshots)) {
    // Lifetime of this object is managed by the Java counterpart, iff bottom
    // sheets can be shown (otherwise an infobar is used and this class is no
    // longer needed).
    PwaBottomSheetController* controller = new PwaBottomSheetController(
        app_name, primary_icon, is_primary_icon_maskable, start_url,
        screenshots, description, categories, show_expanded);
    controller->ShowBottomSheetInstaller(web_contents);
    return;
  }

  InstallableAmbientBadgeInfoBarDelegate::Create(
      web_contents, weak_client, app_name, primary_icon,
      is_primary_icon_maskable, start_url);
}

PwaBottomSheetController::PwaBottomSheetController(
    const base::string16& app_name,
    const SkBitmap& primary_icon,
    const bool is_primary_icon_maskable,
    const GURL& start_url,
    const std::map<GURL, SkBitmap>& screenshots,
    const base::string16& description,
    const std::vector<base::string16>& categories,
    bool show_expanded)
    : app_name_(app_name),
      primary_icon_(primary_icon),
      is_primary_icon_maskable_(is_primary_icon_maskable),
      start_url_(start_url),
      screenshots_(screenshots),
      description_(description),
      categories_(categories),
      show_expanded_(show_expanded) {}

void PwaBottomSheetController::Destroy(JNIEnv* env) {
  delete this;
}

void PwaBottomSheetController::OnAddToHomescreen(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return;
  AppBannerManagerAndroid* app_banner_manager =
      AppBannerManagerAndroid::FromWebContents(web_contents);
  if (!app_banner_manager)
    return;

  app_banner_manager->Install();
}

void PwaBottomSheetController::ShowBottomSheetInstaller(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_user_title =
      ConvertUTF16ToJavaString(env, app_name_);
  // Trim down the app URL to the origin. Elide cryptographic schemes so HTTP
  // is still shown.
  ScopedJavaLocalRef<jstring> j_url = ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               start_url_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  ScopedJavaLocalRef<jstring> j_description =
      ConvertUTF16ToJavaString(env, description_);

  base::string16 category_list =
      base::JoinString(categories_, ASCIIToUTF16(", "));
  ScopedJavaLocalRef<jstring> j_categories =
      ConvertUTF16ToJavaString(env, category_list);

  ScopedJavaLocalRef<jobject> j_bitmap =
      gfx::ConvertToJavaBitmap(primary_icon_);

  Java_PwaBottomSheetControllerProvider_showPwaBottomSheetInstaller(
      env, reinterpret_cast<intptr_t>(this), web_contents->GetJavaWebContents(),
      show_expanded_, j_bitmap, is_primary_icon_maskable_, j_user_title, j_url,
      j_description, j_categories);

  for (const auto& item : screenshots_) {
    if (!item.second.isNull())
      UpdateScreenshot(item.second, web_contents);
  }
}

void PwaBottomSheetController::UpdateScreenshot(
    const SkBitmap& screenshot,
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_screenshot =
      gfx::ConvertToJavaBitmap(screenshot);
  Java_PwaBottomSheetController_addWebAppScreenshot(
      env, java_screenshot, web_contents->GetJavaWebContents());
}

}  // namespace webapps
