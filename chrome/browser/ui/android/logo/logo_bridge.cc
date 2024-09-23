// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/logo/logo_bridge.h"

#include <jni.h>
#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "components/search_provider_logos/logo_observer.h"
#include "components/search_provider_logos/logo_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/logo/jni_headers/LogoBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace {

ScopedJavaLocalRef<jobject> JNI_LogoBridge_MakeJavaLogo(
    JNIEnv* env,
    const SkBitmap& bitmap,
    const GURL& on_click_url,
    const std::string& alt_text,
    const GURL& animated_url) {
  ScopedJavaLocalRef<jobject> j_bitmap = gfx::ConvertToJavaBitmap(bitmap);

  ScopedJavaLocalRef<jstring> j_on_click_url;
  if (on_click_url.is_valid())
    j_on_click_url = ConvertUTF8ToJavaString(env, on_click_url.spec());

  ScopedJavaLocalRef<jstring> j_alt_text;
  if (!alt_text.empty())
    j_alt_text = ConvertUTF8ToJavaString(env, alt_text);

  ScopedJavaLocalRef<jstring> j_animated_url;
  if (animated_url.is_valid())
    j_animated_url = ConvertUTF8ToJavaString(env, animated_url.spec());

  return Java_LogoBridge_createLogo(env, j_bitmap, j_on_click_url, j_alt_text,
                                    j_animated_url);
}

// Converts a C++ Logo to a Java Logo.
ScopedJavaLocalRef<jobject> JNI_LogoBridge_ConvertLogoToJavaObject(
    JNIEnv* env,
    const search_provider_logos::Logo* logo) {
  if (!logo)
    return ScopedJavaLocalRef<jobject>();

  return JNI_LogoBridge_MakeJavaLogo(
      env, logo->image, GURL(logo->metadata.on_click_url),
      logo->metadata.alt_text, GURL(logo->metadata.animated_url));
}

class LogoObserverAndroid : public search_provider_logos::LogoObserver {
 public:
  LogoObserverAndroid(base::WeakPtr<LogoBridge> logo_bridge,
                      JNIEnv* env,
                      jobject j_logo_observer)
      : logo_bridge_(logo_bridge) {
    j_logo_observer_.Reset(env, j_logo_observer);
  }

  LogoObserverAndroid(const LogoObserverAndroid&) = delete;
  LogoObserverAndroid& operator=(const LogoObserverAndroid&) = delete;

  ~LogoObserverAndroid() override {}

  // seach_provider_logos::LogoObserver:
  void OnLogoAvailable(const search_provider_logos::Logo* logo,
                       bool from_cache) override {
    if (!logo_bridge_)
      return;

    JNIEnv* env = base::android::AttachCurrentThread();
    ScopedJavaLocalRef<jobject> j_logo =
        JNI_LogoBridge_ConvertLogoToJavaObject(env, logo);
    Java_LogoObserver_onLogoAvailable(env, j_logo_observer_, j_logo,
                                      from_cache);
  }

  void OnObserverRemoved() override { delete this; }

 private:
  // The associated LogoBridge. We won't call back to Java if the LogoBridge has
  // been destroyed.
  base::WeakPtr<LogoBridge> logo_bridge_;

  base::android::ScopedJavaGlobalRef<jobject> j_logo_observer_;
};

}  // namespace

static jlong JNI_LogoBridge_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 Profile* profile) {
  LogoBridge* logo_bridge = new LogoBridge(profile);
  return reinterpret_cast<intptr_t>(logo_bridge);
}

LogoBridge::LogoBridge(Profile* profile) : logo_service_(nullptr) {
  DCHECK(profile);

  logo_service_ = LogoServiceFactory::GetForProfile(profile);
}

LogoBridge::~LogoBridge() {}

void LogoBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void LogoBridge::GetCurrentLogo(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jobject>& j_logo_observer) {
  // |observer| is deleted in LogoObserverAndroid::OnObserverRemoved().
  LogoObserverAndroid* observer = new LogoObserverAndroid(
      weak_ptr_factory_.GetWeakPtr(), env, j_logo_observer);
  logo_service_->GetLogo(observer);
}
