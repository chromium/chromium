// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/captive_portal_helper_android.h"
#include "components/security_interstitials/content/captive_portal_helper.h"
#include "content/public/browser/browser_thread.h"

#include <stddef.h>

#include <memory>

#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "components/security_interstitials/content/ssl_error_assistant.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "content/public/browser/browser_thread.h"
#include "net/android/network_library.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/security_interstitials/content/android/jni_headers/CaptivePortalHelper_jni.h"

namespace security_interstitials {

void JNI_CaptivePortalHelper_SetOSReportsCaptivePortalForTesting(
    JNIEnv* env,
    jboolean os_reports_captive_portal) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(SSLErrorHandler::SetOSReportsCaptivePortalForTesting,
                     os_reports_captive_portal));
}

void ReportNetworkConnectivity(JNIEnv* env) {
  Java_CaptivePortalHelper_reportNetworkConnectivity(env);
}

std::string GetCaptivePortalServerUrl(JNIEnv* env) {
  return base::android::ConvertJavaStringToUTF8(
      Java_CaptivePortalHelper_getCaptivePortalServerUrl(env));
}

bool IsBehindCaptivePortal() {
  return net::android::GetIsCaptivePortal();
}

}  // namespace security_interstitials
