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
  void Destroy(JNIEnv* env);

  void SetNumberOfSuggestionsShown(JNIEnv* env,
                                   jint jsection,
                                   jint jnumber,
                                   jboolean jhas_complete_suggestion);
  void SetOptOutOffered(JNIEnv* env);
  void SetActivationlessShow(JNIEnv* env);
  void SetSkippedShow(JNIEnv* env);
  void SetShown(JNIEnv* env);
  void SetPayClicked(JNIEnv* env);
  void SetSelectedMethod(JNIEnv* env,
                         jint jPaymentMethodCategory);
  void SetRequestedInformation(JNIEnv* env,
                               jboolean requested_shipping,
                               jboolean requested_email,
                               jboolean requested_phone,
                               jboolean requested_name);
  void SetRequestedPaymentMethods(
      JNIEnv* env,
      const base::android::JavaParamRef<jintArray>& jmethods);
  void SetCompleted(JNIEnv* env);
  void SetAborted(JNIEnv* env,
                  jint jreason);
  void SetNotShown(JNIEnv* env);
  void SetNoMatchingCredentialsShown(JNIEnv* env);
  void RecordCheckoutStep(JNIEnv* env,
                          jint jstep);
  void SetPaymentAppUkmSourceId(JNIEnv* env, ukm::SourceId source_id);

 private:
  JourneyLogger journey_logger_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_JOURNEY_LOGGER_ANDROID_H_
