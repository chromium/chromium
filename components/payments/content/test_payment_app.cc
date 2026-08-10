// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/test_payment_app.h"

namespace payments {

TestPaymentApp::TestPaymentApp(const std::string& method, PaymentApp::Type type)
    : PaymentApp(/*icon_resource_id=*/0, type), method_(method) {}

TestPaymentApp::TestPaymentApp(
    const std::string& method,
    mojom::PaymentEventResponseType error_response_type,
    PaymentApp::Type type)
    : PaymentApp(/*icon_resource_id=*/0, type),
      method_(method),
      error_response_type_(error_response_type) {}

TestPaymentApp::~TestPaymentApp() = default;

void TestPaymentApp::InvokePaymentApp(
    base::WeakPtr<PaymentApp::Delegate> delegate) {
  if (error_response_type_.has_value()) {
    delegate->OnInstrumentDetailsError(error_response_type_.value(),
                                       "Error message");
    return;
  }
  const std::string stringified_details = "{\"data\":\"details\"}";
  delegate->OnInstrumentDetailsReady(method_, stringified_details, PayerData());
}

bool TestPaymentApp::IsCompleteForPayment() const {
  return true;
}
bool TestPaymentApp::CanPreselect() const {
  return true;
}
std::u16string TestPaymentApp::GetMissingInfoLabel() const {
  return std::u16string();
}
bool TestPaymentApp::HasEnrolledInstrument() const {
  return true;
}
bool TestPaymentApp::NeedsInstallation() const {
  return false;
}
std::string TestPaymentApp::GetId() const {
  return method_;
}
std::u16string TestPaymentApp::GetLabel() const {
  return std::u16string();
}
std::u16string TestPaymentApp::GetSublabel() const {
  return std::u16string();
}
bool TestPaymentApp::IsValidForModifier(const std::string& method) const {
  return false;
}
base::WeakPtr<PaymentApp> TestPaymentApp::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
bool TestPaymentApp::HandlesShippingAddress() const {
  return false;
}
bool TestPaymentApp::HandlesPayerName() const {
  return false;
}
bool TestPaymentApp::HandlesPayerEmail() const {
  return false;
}
bool TestPaymentApp::HandlesPayerPhone() const {
  return false;
}

}  // namespace payments
