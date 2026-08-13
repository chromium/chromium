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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "components/search_provider_logos/logo_observer.h"
#include "components/search_provider_logos/logo_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/logo/jni_headers/LogoBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace {

static ScopedJavaLocalRef<jobject> JNI_LogoBridge_MakeJavaLogo(
    JNIEnv* env,
    const SkBitmap& bitmap,
    const SkBitmap& dark_bitmap,
    const GURL& on_click_url,
    const std::string& alt_text,
    const GURL& animated_url,
    const GURL& dark_animated_url,
    const GURL& log_url,
    const GURL& dark_log_url,
    const GURL& cta_log_url,
    const GURL& dark_cta_log_url) {
  ScopedJavaLocalRef<jobject> j_bitmap = gfx::ConvertToJavaBitmap(bitmap);

  ScopedJavaLocalRef<jobject> j_dark_bitmap;
  if (!dark_bitmap.drawsNothing()) {
    j_dark_bitmap = gfx::ConvertToJavaBitmap(dark_bitmap);
  }

  ScopedJavaLocalRef<jstring> j_on_click_url;
  if (on_click_url.is_valid()) {
    j_on_click_url = ConvertUTF8ToJavaString(env, on_click_url.spec());
  }

  ScopedJavaLocalRef<jstring> j_alt_text;
  if (!alt_text.empty()) {
    j_alt_text = ConvertUTF8ToJavaString(env, alt_text);
  }

  ScopedJavaLocalRef<jstring> j_animated_url;
  if (animated_url.is_valid()) {
    j_animated_url = ConvertUTF8ToJavaString(env, animated_url.spec());
  }

  ScopedJavaLocalRef<jstring> j_dark_animated_url;
  if (dark_animated_url.is_valid()) {
    j_dark_animated_url =
        ConvertUTF8ToJavaString(env, dark_animated_url.spec());
  }

  ScopedJavaLocalRef<jstring> j_log_url;
  if (log_url.is_valid()) {
    j_log_url = ConvertUTF8ToJavaString(env, log_url.spec());
  }

  ScopedJavaLocalRef<jstring> j_dark_log_url;
  if (dark_log_url.is_valid()) {
    j_dark_log_url = ConvertUTF8ToJavaString(env, dark_log_url.spec());
  }

  ScopedJavaLocalRef<jstring> j_cta_log_url;
  if (cta_log_url.is_valid()) {
    j_cta_log_url = ConvertUTF8ToJavaString(env, cta_log_url.spec());
  }

  ScopedJavaLocalRef<jstring> j_dark_cta_log_url;
  if (dark_cta_log_url.is_valid()) {
    j_dark_cta_log_url = ConvertUTF8ToJavaString(env, dark_cta_log_url.spec());
  }

  return Java_LogoBridge_createLogo(
      env, j_bitmap, j_dark_bitmap, j_on_click_url, j_alt_text, j_animated_url,
      j_dark_animated_url, j_log_url, j_dark_log_url, j_cta_log_url,
      j_dark_cta_log_url);
}

// Converts a C++ Logo to a Java Logo.
static ScopedJavaLocalRef<jobject> JNI_LogoBridge_ConvertLogoToJavaObject(
    JNIEnv* env,
    const search_provider_logos::Logo* logo) {
  if (!logo) {
    return ScopedJavaLocalRef<jobject>();
  }

  return JNI_LogoBridge_MakeJavaLogo(
      env, logo->image, logo->dark_image, GURL(logo->metadata.on_click_url),
      logo->metadata.alt_text, GURL(logo->metadata.animated_url),
      GURL(logo->metadata.dark_animated_url), GURL(logo->metadata.log_url),
      GURL(logo->metadata.dark_log_url), GURL(logo->metadata.cta_log_url),
      GURL(logo->metadata.dark_cta_log_url));
}

class LogoObserverAndroid : public search_provider_logos::LogoObserver {
 public:
  LogoObserverAndroid(base::WeakPtr<LogoBridge> logo_bridge,
                      JNIEnv* env,
                      const base::android::JavaRef<jobject>& j_logo_observer)
      : logo_bridge_(logo_bridge) {
    j_logo_observer_.Reset(env, j_logo_observer);
  }

  LogoObserverAndroid(const LogoObserverAndroid&) = delete;
  LogoObserverAndroid& operator=(const LogoObserverAndroid&) = delete;

  ~LogoObserverAndroid() override = default;

  // seach_provider_logos::LogoObserver:
  void OnLogoAvailable(const search_provider_logos::Logo* logo,
                       bool from_cache) override {
    if (!logo_bridge_) {
      return;
    }

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

static int64_t JNI_LogoBridge_Init(JNIEnv* env, Profile* profile) {
  LogoBridge* logo_bridge = new LogoBridge(profile);
  return reinterpret_cast<intptr_t>(logo_bridge);
}

LogoBridge::LogoBridge(Profile* profile)
    : url_loader_factory_(profile->GetURLLoaderFactory()),
      logo_service_(nullptr) {
  DCHECK(profile);

  logo_service_ = LogoServiceFactory::GetForProfile(profile);
}

LogoBridge::~LogoBridge() = default;

void LogoBridge::Destroy(JNIEnv* env) {
  delete this;
}

void LogoBridge::GetCurrentLogo(JNIEnv* env,
                                const JavaRef<jobject>& j_logo_observer) {
  // |observer| is deleted in LogoObserverAndroid::OnObserverRemoved().
  LogoObserverAndroid* observer = new LogoObserverAndroid(
      weak_ptr_factory_.GetWeakPtr(), env, j_logo_observer);
  logo_service_->GetLogo(observer);
}

void LogoBridge::RecordImpression(JNIEnv* env, std::string_view log_url) {
  GURL url(log_url);
  if (!url.is_valid()) {
    return;
  }

  auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("doodle_impression_android", R"(
        semantics {
          sender: "Logo impression logger"
          description: "Ping to record that a doodle was shown."
          trigger: "A doodle is shown on the new tab page."
          data: "URL for logging impressions, provided by the server."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "clank-start@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2026-07-06"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  auto loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce([](std::unique_ptr<network::SimpleURLLoader> loader,
                        scoped_refptr<net::HttpResponseHeaders> headers) {},
                     std::move(loader)));
}

DEFINE_JNI(LogoBridge)
