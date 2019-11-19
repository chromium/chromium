// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_MOCK_IDENTITY_OBSERVER_H_
#define COMPONENTS_PAYMENTS_CONTENT_MOCK_IDENTITY_OBSERVER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/service_worker_payment_app.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace payments {

class MockIdentityObserver : public ServiceWorkerPaymentApp::IdentityObserver {
 public:
  MockIdentityObserver();
  ~MockIdentityObserver() override;
  MOCK_METHOD2(SetInvokedServiceWorkerIdentity,
               void(const url::Origin& origin, int64_t registration_id));

  base::WeakPtr<ServiceWorkerPaymentApp::IdentityObserver> AsWeakPtr();

 private:
  base::WeakPtrFactory<MockIdentityObserver> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockIdentityObserver);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_MOCK_IDENTITY_OBSERVER_H_
