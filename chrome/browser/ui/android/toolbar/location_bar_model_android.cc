// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/location_bar_model_android.h"

#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/android/toolbar/jni_headers/LocationBarModel_jni.h"

using jni_zero::JavaRef;

namespace {

bool IsShowDomainOnlyEnabled() {
  return base::FeatureList::IsEnabled(chrome::android::kAndroidBottomBar) &&
         chrome::android::kAndroidBottomBarShowDomainOnlyParam.Get();
}

}  // namespace

LocationBarModelAndroid::LocationBarModelAndroid(const JavaRef<jobject>& obj)
    : location_bar_model_(
          std::make_unique<LocationBarModelImpl>(this,
                                                 content::kMaxURLDisplayChars)),
      java_object_(obj) {}

LocationBarModelAndroid::~LocationBarModelAndroid() = default;

void LocationBarModelAndroid::Destroy() {
  delete this;
}

std::u16string LocationBarModelAndroid::GetFormattedFullURL() {
  return location_bar_model_->GetFormattedFullURL();
}

std::u16string LocationBarModelAndroid::GetURLForDisplay() {
  return location_bar_model_->GetURLForDisplay();
}

GURL LocationBarModelAndroid::GetUrlOfVisibleNavigationEntry() {
  return location_bar_model_->GetURL();
}

int32_t LocationBarModelAndroid::GetPageClassification(bool is_prefetch) const {
  return location_bar_model_->GetPageClassification(is_prefetch);
}

content::WebContents* LocationBarModelAndroid::GetActiveWebContents() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_LocationBarModel_getWebContents(env, java_object_);
}

bool LocationBarModelAndroid::IsNewTabPage() const {
  GURL url;
  if (!GetURL(&url)) {
    return false;
  }

  // Android Chrome has its own Instant NTP page implementation.
  if (url.SchemeIs(chrome::kChromeNativeScheme) &&
      url.host() == chrome::kChromeUINewTabHost) {
    return true;
  }
  if (search::IsWebUiNtpEnabledForDesktopAndroid() && url.SchemeIs(content::kChromeUIScheme) &&
      url.host() == chrome::kChromeUINewTabPageHost) {
    return true;
  }

  return false;
}

bool LocationBarModelAndroid::ShouldTrimDisplayUrlAfterHostName() const {
  GURL url;
  return IsShowDomainOnlyEnabled() && GetURL(&url) &&
         url.SchemeIsHTTPOrHTTPS() && IsToolbarUiRefactorEnabled();
}

bool LocationBarModelAndroid::IsToolbarUiRefactorEnabled() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_LocationBarModel_isToolbarUiRefactorEnabled(env, java_object_);
}

// static
static int64_t JNI_LocationBarModel_Init(const JavaRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new LocationBarModelAndroid(obj));
}

DEFINE_JNI(LocationBarModel)
