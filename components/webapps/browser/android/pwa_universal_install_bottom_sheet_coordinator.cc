// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include <memory>
#include <string>

#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"
#include "components/webapps/browser/android/webapps_jni_headers/PwaUniversalInstallBottomSheetCoordinator_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

namespace {

// The length of time to allow the add to homescreen data fetcher to run before
// timing out and generating an icon.
const int kDataTimeoutInMilliseconds = 8000;

class IconFetcher : public webapps::AddToHomescreenDataFetcher::Observer {
 public:
  IconFetcher(content::WebContents* web_contents,
              JNIEnv* env,
              const JavaParamRef<jobject>& jcaller) {
    env_ = env;
    java_watcher_.Reset(env, jcaller);

    data_fetcher_ = std::make_unique<webapps::AddToHomescreenDataFetcher>(
        web_contents, kDataTimeoutInMilliseconds, this);
  }

 private:
  void OnUserTitleAvailable(const std::u16string& title,
                            const GURL& url,
                            bool is_webapk_compatible) override {}

  void OnDataAvailable(
      const webapps::ShortcutInfo& info,
      const SkBitmap& primary_icon,
      webapps::AddToHomescreenParams::AppType app_type,
      const webapps::InstallableStatusCode installable_status) override {
    webapps::Java_PwaUniversalInstallBottomSheetCoordinator_onAppIconFetched(
        env_, java_watcher_,
        !primary_icon.isNull()
            ? gfx::ConvertToJavaBitmap(primary_icon,
                                       gfx::OomBehavior::kReturnNullOnOom)
            : nullptr,
        info.is_primary_icon_maskable);

    delete this;
  }

  // Fetches data required to add a shortcut.
  std::unique_ptr<webapps::AddToHomescreenDataFetcher> data_fetcher_;

  raw_ptr<JNIEnv> env_;
  base::android::ScopedJavaGlobalRef<jobject> java_watcher_;
};

}  // namespace

namespace webapps {

void JNI_PwaUniversalInstallBottomSheetCoordinator_FetchAppIcon(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  // This class manages its own lifetime:
  new IconFetcher(content::WebContents::FromJavaWebContents(java_web_contents),
                  env, jcaller);
}

}  // namespace webapps
