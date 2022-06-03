// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CAPTIVE_PORTAL_HELPER_ANDROID_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CAPTIVE_PORTAL_HELPER_ANDROID_H_

#include <jni.h>
#include <string>

namespace security_interstitials {

std::string GetCaptivePortalServerUrl(JNIEnv* env);

void ReportNetworkConnectivity(JNIEnv* env);

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CAPTIVE_PORTAL_HELPER_ANDROID_H_
