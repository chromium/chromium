// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/jni_payment_app.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "components/payments/content/android/payment_handler_host.h"
#include "components/payments/content/payment_request_converter.h"
#include "components/payments/core/payment_method_data.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/JniPaymentApp_jni.h"

namespace payments {
namespace {

using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF16ToJavaString;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::ScopedJavaLocalRef;
using ::base::android::ToJavaArrayOfStrings;

void OnAbortResult(const ::base::android::JavaRef<jobject>& jcallback,
                   bool aborted) {
  Java_JniPaymentApp_onAbortResult(AttachCurrentThread(), jcallback, aborted);
}

}  // namespace

// static
ScopedJavaLocalRef<jobject> JniPaymentApp::Create(
    JNIEnv* env,
    std::unique_ptr<PaymentApp> payment_app) {
  // The |app| is owned by JniPaymentApp.java and will be destroyed through a
  // JniPaymentApp::FreeNativeObject() call.
  JniPaymentApp* app = new JniPaymentApp(std::move(payment_app));

  ScopedJavaLocalRef<jobject> icon;
  if (app->payment_app_->icon_bitmap() &&
      !app->payment_app_->icon_bitmap()->drawsNothing()) {
    icon = gfx::ConvertToJavaBitmap(*app->payment_app_->icon_bitmap());
  }

  return Java_JniPaymentApp_Constructor(
      env, ConvertUTF8ToJavaString(env, app->payment_app_->GetId()),
      ConvertUTF16ToJavaString(env, app->payment_app_->GetLabel()),
      ConvertUTF16ToJavaString(env, app->payment_app_->GetSublabel()), icon,
      static_cast<jint>(app->payment_app_->type()),
      reinterpret_cast<jlong>(app));
}

ScopedJavaLocalRef<jobjectArray> JniPaymentApp::GetInstrumentMethodNames(
    JNIEnv* env) {
  return ToJavaArrayOfStrings(
      env, std::vector<std::string>(payment_app_->GetAppMethodNames().begin(),
                                    payment_app_->GetAppMethodNames().end()));
}

// TODO(crbug.com/40182225): Remove jdata_byte_buffer here, as it is no longer
// used.
bool JniPaymentApp::IsValidForPaymentMethodData(
    JNIEnv* env,
    const JavaParamRef<jstring>& jmethod,
    const JavaParamRef<jobject>& jdata_byte_buffer) {
  if (!jdata_byte_buffer) {
    bool is_valid = false;
    payment_app_->IsValidForPaymentMethodIdentifier(
        ConvertJavaStringToUTF8(env, jmethod), &is_valid);
    return is_valid;
  }

  mojom::PaymentMethodDataPtr mojo_data;
  bool success = android::DeserializeFromJavaByteBuffer(env, jdata_byte_buffer,
                                                        &mojo_data);
  DCHECK(success);

  PaymentMethodData data = ConvertPaymentMethodData(mojo_data);
  return payment_app_->IsValidForModifier(
      ConvertJavaStringToUTF8(env, jmethod));
}

bool JniPaymentApp::HandlesShippingAddress(JNIEnv* env) {
  return payment_app_->HandlesShippingAddress();
}

bool JniPaymentApp::HandlesPayerName(JNIEnv* env) {
  return payment_app_->HandlesPayerName();
}

bool JniPaymentApp::HandlesPayerEmail(JNIEnv* env) {
  return payment_app_->HandlesPayerEmail();
}

bool JniPaymentApp::HandlesPayerPhone(JNIEnv* env) {
  return payment_app_->HandlesPayerPhone();
}

bool JniPaymentApp::HasEnrolledInstrument(JNIEnv* env) {
  // ChromePaymentRequestService.java uses this value to determine whether
  // PaymentRequest.hasEnrolledInstrument() should return true.
  return payment_app_->HasEnrolledInstrument();
}

bool JniPaymentApp::CanPreselect(JNIEnv* env) {
  return payment_app_->CanPreselect();
}

void JniPaymentApp::InvokePaymentApp(JNIEnv* env,
                                     const JavaParamRef<jobject>& jcallback) {
  invoke_callback_ = jcallback;
  payment_app_->InvokePaymentApp(/*delegate=*/weak_ptr_factory_.GetWeakPtr());
}

void JniPaymentApp::UpdateWith(
    JNIEnv* env,
    const JavaParamRef<jobject>& jresponse_byte_buffer) {
  mojom::PaymentRequestDetailsUpdatePtr response;
  bool success = android::DeserializeFromJavaByteBuffer(
      env, jresponse_byte_buffer, &response);
  DCHECK(success);
  payment_app_->UpdateWith(std::move(response));
}

void JniPaymentApp::OnPaymentDetailsNotUpdated(JNIEnv* env) {
  payment_app_->OnPaymentDetailsNotUpdated();
}

bool JniPaymentApp::IsWaitingForPaymentDetailsUpdate(JNIEnv* env) {
  return payment_app_->IsWaitingForPaymentDetailsUpdate();
}

void JniPaymentApp::AbortPaymentApp(JNIEnv* env,
                                    const JavaParamRef<jobject>& jcallback) {
  payment_app_->AbortPaymentApp(base::BindOnce(
      &OnAbortResult,
      base::android::ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

ScopedJavaLocalRef<jstring> JniPaymentApp::GetApplicationIdentifierToHide(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(
      env, payment_app_->GetApplicationIdentifierToHide());
}

ScopedJavaLocalRef<jobjectArray>
JniPaymentApp::GetApplicationIdentifiersThatHideThisApp(JNIEnv* env) {
  const std::set<std::string>& ids =
      payment_app_->GetApplicationIdentifiersThatHideThisApp();
  return ToJavaArrayOfStrings(env,
                              std::vector<std::string>(ids.begin(), ids.end()));
}

jlong JniPaymentApp::GetUkmSourceId(JNIEnv* env) {
  return payment_app_->UkmSourceId();
}

void JniPaymentApp::SetPaymentHandlerHost(
    JNIEnv* env,
    const JavaParamRef<jobject>& jpayment_handler_host) {
  payment_app_->SetPaymentHandlerHost(
      android::PaymentHandlerHost::FromJavaPaymentHandlerHost(
          env, jpayment_handler_host));
}

base::android::ScopedJavaLocalRef<jbyteArray>
JniPaymentApp::SetAppSpecificResponseFields(
    JNIEnv* env,
    const JavaParamRef<jobject>& jpayment_response) {
  mojom::PaymentResponsePtr response;
  bool success =
      android::DeserializeFromJavaByteBuffer(env, jpayment_response, &response);
  DCHECK(success);
  mojom::PaymentResponsePtr result =
      payment_app_->SetAppSpecificResponseFields(std::move(response));
  return base::android::ToJavaByteArray(
      env, mojom::PaymentResponse::Serialize(&result));
}

void JniPaymentApp::FreeNativeObject(JNIEnv* env) {
  delete this;
}

void JniPaymentApp::OnInstrumentDetailsReady(
    const std::string& method_name,
    const std::string& stringified_details,
    const PayerData& payer_data) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> jshipping_address =
      payer_data.shipping_address
          ? Java_JniPaymentApp_createShippingAddress(
                env,
                ConvertUTF8ToJavaString(env,
                                        payer_data.shipping_address->country),
                ToJavaArrayOfStrings(env,
                                     payer_data.shipping_address->address_line),
                ConvertUTF8ToJavaString(env,
                                        payer_data.shipping_address->region),
                ConvertUTF8ToJavaString(env, payer_data.shipping_address->city),
                ConvertUTF8ToJavaString(
                    env, payer_data.shipping_address->dependent_locality),
                ConvertUTF8ToJavaString(
                    env, payer_data.shipping_address->postal_code),
                ConvertUTF8ToJavaString(
                    env, payer_data.shipping_address->sorting_code),
                ConvertUTF8ToJavaString(
                    env, payer_data.shipping_address->organization),
                ConvertUTF8ToJavaString(env,
                                        payer_data.shipping_address->recipient),
                ConvertUTF8ToJavaString(env,
                                        payer_data.shipping_address->phone))
          : nullptr;

  ScopedJavaLocalRef<jobject> jpayer_data = Java_JniPaymentApp_createPayerData(
      env, ConvertUTF8ToJavaString(env, payer_data.payer_name),
      ConvertUTF8ToJavaString(env, payer_data.payer_phone),
      ConvertUTF8ToJavaString(env, payer_data.payer_email), jshipping_address,
      ConvertUTF8ToJavaString(env, payer_data.selected_shipping_option_id));

  Java_JniPaymentApp_onInvokeResult(
      env, invoke_callback_, ConvertUTF8ToJavaString(env, method_name),
      ConvertUTF8ToJavaString(env, stringified_details), jpayer_data);
}

void JniPaymentApp::OnInstrumentDetailsError(const std::string& error_message) {
  JNIEnv* env = AttachCurrentThread();
  Java_JniPaymentApp_onInvokeError(env, invoke_callback_,
                                   ConvertUTF8ToJavaString(env, error_message));
}

JniPaymentApp::JniPaymentApp(std::unique_ptr<PaymentApp> payment_app)
    : payment_app_(std::move(payment_app)) {}

JniPaymentApp::~JniPaymentApp() = default;

}  // namespace payments
