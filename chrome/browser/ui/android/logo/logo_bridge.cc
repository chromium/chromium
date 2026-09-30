// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/logo/logo_bridge.h"

#include <jni.h>
#include <stdint.h>

#include "base/android/jni_android.h"
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
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/android/logo/jni_headers/LogoBridge_jni.h"

using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;

namespace {

std::optional<std::string> ValidUrlToOptionalSpec(const GURL& url) {
  return url.is_valid() ? std::make_optional(url.spec()) : std::nullopt;
}

// Converts a C++ Logo to a Java Logo.
ScopedJavaLocalRef<jobject> JNI_LogoBridge_ConvertLogoToJavaObject(
    JNIEnv* env,
    const search_provider_logos::Logo* logo) {
  if (!logo) {
    return nullptr;
  }

  return Java_LogoBridge_createLogo(
      env, logo->image, logo->dark_image,
      ValidUrlToOptionalSpec(logo->metadata.on_click_url),
      logo->metadata.alt_text.empty()
          ? std::nullopt
          : std::make_optional(logo->metadata.alt_text),
      ValidUrlToOptionalSpec(logo->metadata.animated_url),
      ValidUrlToOptionalSpec(logo->metadata.dark_animated_url),
      ValidUrlToOptionalSpec(logo->metadata.log_url),
      ValidUrlToOptionalSpec(logo->metadata.dark_log_url),
      ValidUrlToOptionalSpec(logo->metadata.cta_log_url),
      ValidUrlToOptionalSpec(logo->metadata.dark_cta_log_url));
}

class LogoObserverAndroid : public search_provider_logos::LogoObserver {
 public:
  LogoObserverAndroid(base::WeakPtr<LogoBridge> logo_bridge,
                      JNIEnv* env,
                      const jni_zero::JavaRef<jobject>& j_logo_observer)
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

  jni_zero::ScopedJavaGlobalRef<jobject> j_logo_observer_;
};

}  // namespace

static int64_t JNI_LogoBridge_Init(Profile* profile) {
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

void LogoBridge::Destroy() {
  delete this;
}

void LogoBridge::GetCurrentLogo(JNIEnv* env,
                                const JavaRef<jobject>& j_logo_observer) {
  // |observer| is deleted in LogoObserverAndroid::OnObserverRemoved().
  LogoObserverAndroid* observer = new LogoObserverAndroid(
      weak_ptr_factory_.GetWeakPtr(), env, j_logo_observer);
  logo_service_->GetLogo(observer);
}

void LogoBridge::RecordImpression(std::string_view log_url) {
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
