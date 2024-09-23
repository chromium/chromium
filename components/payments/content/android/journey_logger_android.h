// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_JOURNEY_LOGGER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_JOURNEY_LOGGER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "components/payments/core/journey_logger.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace payments {

// Forwarding calls to payments::JourneyLogger.
class JourneyLoggerAndroid {
 public:
  explicit JourneyLoggerAndroid(ukm::SourceId source_id);

  JourneyLoggerAndroid(const JourneyLoggerAndroid&) = delete;
  JourneyLoggerAndroid& operator=(const JourneyLoggerAndroid&) = delete;

  ~JourneyLoggerAndroid();

  // Message from Java to destroy this object.
  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jcaller);

  void SetNumberOfSuggestionsShown(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint jsection,
      jint jnumber,
      jboolean jhas_complete_suggestion);
  void SetOptOutOffered(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jcaller);
  void SetActivationlessShow(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void SetSkippedShow(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jcaller);
  void SetShown(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& jcaller);
  void SetPayClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller);
  void SetSelectedMethod(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         jint jPaymentMethodCategory);
  void SetRequestedInformation(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean requested_shipping,
      jboolean requested_email,
      jboolean requested_phone,
      jboolean requested_name);
  void SetRequestedPaymentMethods(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jintArray>& jmethods);
  void SetCompleted(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& jcaller);
  void SetAborted(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller,
                  jint jreason);
  void SetNotShown(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jcaller);
  void SetNoMatchingCredentialsShown(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void RecordCheckoutStep(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jcaller,
                          jint jstep);
  void SetPaymentAppUkmSourceId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      ukm::SourceId source_id);

 private:
  JourneyLogger journey_logger_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_JOURNEY_LOGGER_ANDROID_H_
