// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_MOCK_WEB_PAYMENTS_WEB_DATA_SERVICE_H_
#define COMPONENTS_PAYMENTS_CONTENT_MOCK_WEB_PAYMENTS_WEB_DATA_SERVICE_H_

#include "base/time/time.h"
#include "components/payments/content/browser_binding/browser_bound_key_metadata.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

class MockWebPaymentsWebDataService : public WebPaymentsWebDataService {
 public:
  MockWebPaymentsWebDataService();

  MOCK_METHOD(WebDataServiceBase::Handle,
              GetSecurePaymentConfirmationCredentials,
              (std::vector<std::vector<uint8_t>> credential_ids,
               const std::string& relying_party_id,
               WebDataServiceRequestCallback callback),
              (override));
  MOCK_METHOD(void,
              ClearSecurePaymentConfirmationCredentials,
              (base::Time begin, base::Time end, base::OnceClosure callback),
              (override));
  MOCK_METHOD(WebDataServiceBase::Handle,
              SetBrowserBoundKey,
              (std::vector<uint8_t> credential_id,
               std::string relying_party_id,
               std::vector<uint8_t> browser_bound_key_id,
               std::optional<base::Time> last_used,
               WebDataServiceRequestCallback callback),
              (override));
  MOCK_METHOD(WebDataServiceBase::Handle,
              GetBrowserBoundKey,
              (std::vector<uint8_t> credential_id,
               std::string relying_party_id,
               WebDataServiceRequestCallback callback),
              (override));
  MOCK_METHOD(WebDataServiceBase::Handle,
              GetAllBrowserBoundKeys,
              (WebDataServiceRequestCallback callback),
              (override));
  MOCK_METHOD(WebDataServiceBase::Handle,
              UpdateBrowserBoundKeyLastUsed,
              (std::vector<uint8_t> credential_id,
               std::string relying_party_id,
               base::Time last_used,
               WebDataServiceRequestCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteBrowserBoundKeys,
              (std::vector<BrowserBoundKeyMetadata::RelyingPartyAndCredentialId>
                   passkeys,
               base::OnceClosure callback),
              (override));

 protected:
  ~MockWebPaymentsWebDataService() override;
};

class MockWebDataServiceWrapper : public WebDataServiceWrapper {
 public:
  MockWebDataServiceWrapper();
  MOCK_METHOD(scoped_refptr<WebPaymentsWebDataService>,
              GetWebPaymentsWebData,
              (),
              ());
  MOCK_METHOD(void, Shutdown, (), (override));

  ~MockWebDataServiceWrapper() override;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_MOCK_WEB_PAYMENTS_WEB_DATA_SERVICE_H_
