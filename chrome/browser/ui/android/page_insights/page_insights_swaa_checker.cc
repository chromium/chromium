// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ui//android/page_insights/jni_headers/PageInsightsSwaaChecker_jni.h"
#include "components/history/core/browser/web_history_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

namespace {
net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
    net::DefinePartialNetworkTrafficAnnotation("page_insights",
                                               "web_history_service",
                                               R"(
        semantics {
          sender: "Google Bottom Bar"
          description:
            "Check whether sWAA (supplemental Web and App Activity) is set "
            "in My Google Activity. If it is active and the other conditions "
            "are met as well, the custom tab launched by Android Google "
            "search app can instantiate page insights sheet, a part of Google "
            "Bottom Bar feature."
          trigger:
            "Everyt 5 minutes to keep the value up to date, or after the user "
            "changes their primary account."
          data:
            "The request includes an OAuth2 token authenticating the user."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "gbb-cct-eng@google.com"
            }
            contacts {
              email: "jinsukkim@chromium.org"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2023-05-31"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This fetch is only enabled for signed-in users. There's no "
            "direct Chromium's setting to disable this, but users can manage "
            "their preferences by visiting myactivity.google.com."
          chrome_policy {
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");

void SwaaCallback(const ScopedJavaGlobalRef<jobject>& j_checker, bool enabled) {
  JNIEnv* env = AttachCurrentThread();
  Java_PageInsightsSwaaChecker_onSwaaResponse(env, j_checker, enabled);
}
}  // namespace

static void JNI_PageInsightsSwaaChecker_QueryStatus(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_checker,
    const base::android::JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  auto* service = WebHistoryServiceFactory::GetForProfile(profile);
  if (!service) {
    SwaaCallback(ScopedJavaGlobalRef<jobject>(j_checker), false);
    return;
  }
  service->QueryWebAndAppActivity(
      base::BindOnce(&SwaaCallback, ScopedJavaGlobalRef<jobject>(j_checker)),
      partial_traffic_annotation);
}
