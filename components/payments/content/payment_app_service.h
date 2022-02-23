// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_SERVICE_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_SERVICE_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/payments/content/payment_app_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace payments {

// Retrieves payment apps of all types.
class PaymentAppService : public KeyedService {
 public:
  // The |context| pointer is not being saved.
  explicit PaymentAppService(content::BrowserContext* context);

  PaymentAppService(const PaymentAppService&) = delete;
  PaymentAppService& operator=(const PaymentAppService&) = delete;

  ~PaymentAppService() override;

  // Returns the number of payment app factories, which is the number of times
  // that |delegate->OnDoneCreatingPaymentApps()| will be called as a result of
  // Create().
  size_t GetNumberOfFactories() const;

  // Create payment apps for |delegate|.
  void Create(base::WeakPtr<PaymentAppFactory::Delegate> delegate);

  // KeyedService implementation:
  void Shutdown() override;

  void AddFactoryForTesting(std::unique_ptr<PaymentAppFactory> factory);

 private:
  std::vector<std::unique_ptr<PaymentAppFactory>> factories_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_SERVICE_H_
