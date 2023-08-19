// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PAYMENTS_MOCK_PAYMENT_APP_INSTANCE_H_
#define CHROMEOS_COMPONENTS_PAYMENTS_MOCK_PAYMENT_APP_INSTANCE_H_

#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

// The mock payment_app.mojom interface.
class MockPaymentAppInstance
    : public chromeos::payments::mojom::PaymentAppInstance {
 public:
  MockPaymentAppInstance();
  ~MockPaymentAppInstance() override;

  MockPaymentAppInstance(const MockPaymentAppInstance& other) = delete;
  MockPaymentAppInstance& operator=(const MockPaymentAppInstance& other) =
      delete;

  MOCK_METHOD2(
      IsPaymentImplemented,
      void(const std::string& package_name,
           base::OnceCallback<
               void(chromeos::payments::mojom::IsPaymentImplementedResultPtr)>
               callback));
  MOCK_METHOD2(IsReadyToPay,
               void(chromeos::payments::mojom::PaymentParametersPtr,
                    base::OnceCallback<void(
                        chromeos::payments::mojom::IsReadyToPayResultPtr)>));
  MOCK_METHOD2(
      InvokePaymentApp,
      void(chromeos::payments::mojom::PaymentParametersPtr,
           base::OnceCallback<
               void(chromeos::payments::mojom::InvokePaymentAppResultPtr)>));
  MOCK_METHOD2(AbortPaymentApp,
               void(const std::string&, base::OnceCallback<void(bool)>));

  mojo::Receiver<chromeos::payments::mojom::PaymentAppInstance> receiver_{this};
};

}  // namespace payments

#endif  // CHROMEOS_COMPONENTS_PAYMENTS_MOCK_PAYMENT_APP_INSTANCE_H_
