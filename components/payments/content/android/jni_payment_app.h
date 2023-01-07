// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_JNI_PAYMENT_APP_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_JNI_PAYMENT_APP_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/payments/content/payment_app.h"

namespace payments {

// Forwarding calls to a PaymentApp. Owned by JniPaymentApp.java.
class JniPaymentApp : public PaymentApp::Delegate {
 public:
  static base::android::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      std::unique_ptr<PaymentApp> payment_app);

  // Disallow copy and assign.
  JniPaymentApp(const JniPaymentApp& other) = delete;
  JniPaymentApp& operator=(const JniPaymentApp& other) = delete;

  base::android::ScopedJavaLocalRef<jobjectArray> GetInstrumentMethodNames(
      JNIEnv* env);

  bool IsValidForPaymentMethodData(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jmethod,
      const base::android::JavaParamRef<jobject>& jdata_byte_buffer);

  bool HandlesShippingAddress(JNIEnv* env);

  bool HandlesPayerName(JNIEnv* env);

  bool HandlesPayerEmail(JNIEnv* env);

  bool HandlesPayerPhone(JNIEnv* env);

  bool HasEnrolledInstrument(JNIEnv* env);

  bool CanPreselect(JNIEnv* env);

  void InvokePaymentApp(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jcallback);

  void UpdateWith(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jresponse_byte_buffer);

  void OnPaymentDetailsNotUpdated(JNIEnv* env);

  bool IsWaitingForPaymentDetailsUpdate(JNIEnv* env);

  void AbortPaymentApp(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& jcallback);

  base::android::ScopedJavaLocalRef<jstring> GetApplicationIdentifierToHide(
      JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobjectArray>
  GetApplicationIdentifiersThatHideThisApp(JNIEnv* env);

  jlong GetUkmSourceId(JNIEnv* env);

  void SetPaymentHandlerHost(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jpayment_handler_host);

  base::android::ScopedJavaLocalRef<jbyteArray> SetAppSpecificResponseFields(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jpayment_response);

  void FreeNativeObject(JNIEnv* env);

 private:
  // PaymentApp::Delegate implementation:
  void OnInstrumentDetailsReady(const std::string& method_name,
                                const std::string& stringified_details,
                                const PayerData& payer_data) override;
  void OnInstrumentDetailsError(const std::string& error_message) override;

  explicit JniPaymentApp(std::unique_ptr<PaymentApp> payment_app);
  ~JniPaymentApp() override;

  std::unique_ptr<PaymentApp> payment_app_;
  base::android::ScopedJavaGlobalRef<jobject> invoke_callback_;

  base::WeakPtrFactory<JniPaymentApp> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_JNI_PAYMENT_APP_H_
