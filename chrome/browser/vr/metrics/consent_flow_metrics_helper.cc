// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/metrics/consent_flow_metrics_helper.h"

#include "base/metrics/histogram_macros.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "chrome/android/features/vr/jni_headers/ConsentFlowMetrics_jni.h"

using base::android::JavaParamRef;
#endif

namespace vr {
namespace {
static constexpr base::TimeDelta kUserActionBounceTime =
    base::TimeDelta::FromMinutes(1);
}  // namespace

ConsentFlowMetricsHelper::ConsentFlowMetricsHelper() = default;
ConsentFlowMetricsHelper::~ConsentFlowMetricsHelper() = default;

// static
ConsentFlowMetricsHelper* ConsentFlowMetricsHelper::InitFromWebContents(
    content::WebContents* contents) {
  ConsentFlowMetricsHelper::CreateForWebContents(contents);
  return ConsentFlowMetricsHelper::FromWebContents(contents);
}

ConsentFlowMetricsHelper::ConsentFlowMetricsHelper(
    content::WebContents* contents) {}

void ConsentFlowMetricsHelper::LogConsentFlowDurationWhenConsentGranted() {
  UMA_HISTOGRAM_MEDIUM_TIMES("XR.WebXR.ConsentFlowDuration.ConsentGranted",
                             base::TimeTicks::Now() - dialog_presented_at_);
}

void ConsentFlowMetricsHelper::LogUserAction(ConsentDialogAction action) {
  UMA_HISTOGRAM_ENUMERATION("XR.WebXR.ConsentFlow", action);
}

void ConsentFlowMetricsHelper::OnDialogClosedWithConsent(const std::string& url,
                                                         bool is_granted) {
  if (is_granted && previous_consent_ && previous_consent_flow_end_time_ &&
      (last_visited_url_ == url)) {
    if (!previous_consent_.value() &&
        (base::Time::Now() - previous_consent_flow_end_time_.value()) <
            kUserActionBounceTime)
      LogUserAction(ConsentDialogAction::kUserAllowedAfterBounce);
  }

  previous_consent_ = is_granted;
  last_visited_url_ = url;
  previous_consent_flow_end_time_ = base::Time::Now();
}

void ConsentFlowMetricsHelper::OnShowDialog() {
  dialog_presented_at_ = base::TimeTicks::Now();
}

void ConsentFlowMetricsHelper::LogConsentFlowDurationWhenConsentNotGranted() {
  UMA_HISTOGRAM_MEDIUM_TIMES("XR.WebXR.ConsentFlowDuration.ConsentNotGranted",
                             base::TimeTicks::Now() - dialog_presented_at_);
}

void ConsentFlowMetricsHelper::LogConsentFlowDurationWhenUserAborted() {
  UMA_HISTOGRAM_MEDIUM_TIMES("XR.WebXR.ConsentFlowDuration.ConsentFlowAborted",
                             base::TimeTicks::Now() - dialog_presented_at_);
}

#if defined(OS_ANDROID)
void ConsentFlowMetricsHelper::OnDialogClosedWithConsent(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jurl,
    jboolean is_granted) {
  OnDialogClosedWithConsent(base::android::ConvertJavaStringToUTF8(env, jurl),
                            !!is_granted);
}

void ConsentFlowMetricsHelper::LogUserAction(
    JNIEnv* env,
    jint action) {
  LogUserAction(static_cast<ConsentDialogAction>(action));
}

void ConsentFlowMetricsHelper::LogConsentFlowDurationWhenConsentGranted(
    JNIEnv* env) {
  LogConsentFlowDurationWhenConsentGranted();
}

void ConsentFlowMetricsHelper::LogConsentFlowDurationWhenConsentNotGranted(
    JNIEnv* env) {
  LogConsentFlowDurationWhenConsentNotGranted();
}

void ConsentFlowMetricsHelper::LogConsentFlowDurationWhenUserAborted(
    JNIEnv* env) {
  LogConsentFlowDurationWhenUserAborted();
}

static jlong JNI_ConsentFlowMetrics_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  return reinterpret_cast<intptr_t>(
      ConsentFlowMetricsHelper::InitFromWebContents(web_contents));
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(ConsentFlowMetricsHelper)
}  // namespace vr
