// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/journey_logger_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/JourneyLogger_jni.h"

namespace payments {
namespace {

using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::JavaParamRef;
using base::android::JavaRef;

}  // namespace

JourneyLoggerAndroid::JourneyLoggerAndroid(ukm::SourceId source_id)
    : journey_logger_(source_id) {}

JourneyLoggerAndroid::~JourneyLoggerAndroid() = default;

void JourneyLoggerAndroid::Destroy(JNIEnv* env,
                                   const JavaParamRef<jobject>& jcaller) {
  delete this;
}

void JourneyLoggerAndroid::SetNumberOfSuggestionsShown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jsection,
    jint jnumber,
    jboolean jhas_complete_suggestion) {
  DCHECK_GE(jsection, 0);
  DCHECK_LT(jsection, JourneyLogger::Section::SECTION_MAX);
  journey_logger_.SetNumberOfSuggestionsShown(
      static_cast<JourneyLogger::Section>(jsection), jnumber,
      jhas_complete_suggestion);
}

void JourneyLoggerAndroid::SetOptOutOffered(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetOptOutOffered();
}

void JourneyLoggerAndroid::SetActivationlessShow(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetActivationlessShow();
}

void JourneyLoggerAndroid::SetSkippedShow(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetSkippedShow();
}

void JourneyLoggerAndroid::SetShown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetShown();
}

void JourneyLoggerAndroid::SetPayClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetPayClicked();
}

void JourneyLoggerAndroid::SetSelectedMethod(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jPaymentMethodCategory) {
  DCHECK_GE(jPaymentMethodCategory, 0);
  DCHECK_LE(static_cast<unsigned int>(jPaymentMethodCategory),
            static_cast<unsigned int>(
                JourneyLogger::PaymentMethodCategory::kMaxValue));
  journey_logger_.SetSelectedMethod(
      static_cast<JourneyLogger::PaymentMethodCategory>(
          jPaymentMethodCategory));
}

void JourneyLoggerAndroid::SetRequestedInformation(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean requested_shipping,
    jboolean requested_email,
    jboolean requested_phone,
    jboolean requested_name) {
  journey_logger_.SetRequestedInformation(requested_shipping, requested_email,
                                          requested_phone, requested_name);
}

void JourneyLoggerAndroid::SetRequestedPaymentMethods(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jintArray>& jmethods) {
  std::vector<int> int_methods;
  base::android::JavaIntArrayToIntVector(env, jmethods, &int_methods);
  std::vector<JourneyLogger::PaymentMethodCategory> method_categories;
  for (auto& int_method : int_methods) {
    method_categories.push_back(
        static_cast<JourneyLogger::PaymentMethodCategory>(int_method));
  }
  journey_logger_.SetRequestedPaymentMethods(method_categories);
}

void JourneyLoggerAndroid::SetCompleted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetCompleted();
}

void JourneyLoggerAndroid::SetAborted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jreason) {
  DCHECK_GE(jreason, 0);
  DCHECK_LT(jreason, JourneyLogger::AbortReason::ABORT_REASON_MAX);
  journey_logger_.SetAborted(static_cast<JourneyLogger::AbortReason>(jreason));
}

void JourneyLoggerAndroid::SetNotShown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetNotShown();
}

void JourneyLoggerAndroid::SetNoMatchingCredentialsShown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetNoMatchingCredentialsShown();
}

void JourneyLoggerAndroid::RecordCheckoutStep(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jstep) {
  journey_logger_.RecordCheckoutStep(
      static_cast<JourneyLogger::CheckoutFunnelStep>(jstep));
}

void JourneyLoggerAndroid::SetPaymentAppUkmSourceId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    ukm::SourceId source_id) {
  journey_logger_.SetPaymentAppUkmSourceId(source_id);
}

static jlong JNI_JourneyLogger_InitJourneyLoggerAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);  // Verified in Java before invoking this function.
  return reinterpret_cast<jlong>(new JourneyLoggerAndroid(
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()));
}

}  // namespace payments
