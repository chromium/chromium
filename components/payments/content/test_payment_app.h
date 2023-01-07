// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_TEST_PAYMENT_APP_H_
#define COMPONENTS_PAYMENTS_CONTENT_TEST_PAYMENT_APP_H_

#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app.h"

namespace payments {

// A fake PaymentApp for use in unittests.
class TestPaymentApp : public PaymentApp {
 public:
  explicit TestPaymentApp(const std::string& method);
  ~TestPaymentApp() override;

  TestPaymentApp(const TestPaymentApp& other) = delete;
  TestPaymentApp& operator=(const TestPaymentApp& other) = delete;

  // PaymentApp:
  void InvokePaymentApp(base::WeakPtr<Delegate> delegate) override;
  bool IsCompleteForPayment() const override;
  bool CanPreselect() const override;
  std::u16string GetMissingInfoLabel() const override;
  bool HasEnrolledInstrument() const override;
  void RecordUse() override;
  bool NeedsInstallation() const override;
  std::string GetId() const override;
  std::u16string GetLabel() const override;
  std::u16string GetSublabel() const override;
  bool IsValidForModifier(const std::string& method) const override;
  base::WeakPtr<PaymentApp> AsWeakPtr() override;
  bool HandlesShippingAddress() const override;
  bool HandlesPayerName() const override;
  bool HandlesPayerEmail() const override;
  bool HandlesPayerPhone() const override;

 private:
  const std::string method_;
  base::WeakPtrFactory<TestPaymentApp> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_TEST_PAYMENT_APP_H_
