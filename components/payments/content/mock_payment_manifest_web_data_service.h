// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_MOCK_PAYMENT_MANIFEST_WEB_DATA_SERVICE_H_
#define COMPONENTS_PAYMENTS_CONTENT_MOCK_PAYMENT_MANIFEST_WEB_DATA_SERVICE_H_

#include "base/time/time.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

class MockPaymentManifestWebDataService : public PaymentManifestWebDataService {
 public:
  MockPaymentManifestWebDataService();
  MOCK_METHOD(void,
              ClearSecurePaymentConfirmationCredentials,
              (base::Time begin, base::Time end, base::OnceClosure callback),
              (override));

 protected:
  ~MockPaymentManifestWebDataService() override;
};

class MockWebDataServiceWrapper : public WebDataServiceWrapper {
 public:
  MockWebDataServiceWrapper();
  MOCK_METHOD(scoped_refptr<PaymentManifestWebDataService>,
              GetPaymentManifestWebData,
              (),
              ());
  MOCK_METHOD(void, Shutdown, (), (override));

  ~MockWebDataServiceWrapper() override;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_MOCK_PAYMENT_MANIFEST_WEB_DATA_SERVICE_H_
