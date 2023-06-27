// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_WEB_DATA_SERVICE_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_WEB_DATA_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/payments/content/web_app_manifest.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

class WDTypedResult;
class WebDatabaseService;

namespace base {
class SequencedTaskRunner;
}

namespace payments {

struct SecurePaymentConfirmationCredential;

// Web data service to read/write data in WebAppManifestSectionTable and
// PaymentMethodManifestTable.
class PaymentManifestWebDataService : public WebDataServiceBase,
                                      public WebDataServiceConsumer {
 public:
  PaymentManifestWebDataService(
      scoped_refptr<WebDatabaseService> wdbs,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  PaymentManifestWebDataService(const PaymentManifestWebDataService&) = delete;
  PaymentManifestWebDataService& operator=(
      const PaymentManifestWebDataService&) = delete;

  // Adds the web app `manifest`.
  void AddPaymentWebAppManifest(std::vector<WebAppManifestSection> manifest);

  // Adds the `payment_method`'s manifest.
  void AddPaymentMethodManifest(const std::string& payment_method,
                                std::vector<std::string> app_package_names);

  // Gets the `web_app`'s manifest and  returns it to the `consumer`, which must
  // outlive the DB operation, because DB tasks cannot be cancelled.
  WebDataServiceBase::Handle GetPaymentWebAppManifest(
      const std::string& web_app,
      WebDataServiceConsumer* consumer);

  // Gets the `payment_method`'s manifest and returns it the `consumer`, which
  // must outlive the DB operation, because DB tasks cannot be cancelled.
  WebDataServiceBase::Handle GetPaymentMethodManifest(
      const std::string& payment_method,
      WebDataServiceConsumer* consumer);

  // Adds the secure payment confirmation `credential` and returns a boolean
  // status to the `consumer`, which must outlive the DB operation, because DB
  // tasks cannot be cancelled. The `credential` should not be null.
  WebDataServiceBase::Handle AddSecurePaymentConfirmationCredential(
      std::unique_ptr<SecurePaymentConfirmationCredential> credential,
      WebDataServiceConsumer* consumer);

  // Gets the secure payment confirmation credential information for the given
  // `credential_ids` and returns it to the `consumer`, which must outlive the
  // DB operation, because DB tasks cannot be cancelled. Please use
  // `std::move()` for `credential_ids` parameter to avoid extra copies.
  WebDataServiceBase::Handle GetSecurePaymentConfirmationCredentials(
      std::vector<std::vector<uint8_t>> credential_ids,
      const std::string& relying_party_id,
      WebDataServiceConsumer* consumer);

  // Clears all of the the secure payment confirmation credential information
  // created in the given time range `begin` and `end`, and invokes `callback`
  // when the clearing is completed.
  virtual void ClearSecurePaymentConfirmationCredentials(
      base::Time begin,
      base::Time end,
      base::OnceClosure callback);

  // Override WebDataServiceConsumer interface.
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

 protected:
  ~PaymentManifestWebDataService() override;

 private:
  std::unique_ptr<WDTypedResult> ClearSecurePaymentConfirmationCredentialsImpl(
      base::Time begin,
      base::Time end,
      WebDatabase* db);

  void RemoveExpiredData(WebDatabase* db);

  WebDatabase::State AddPaymentWebAppManifestImpl(
      const std::vector<WebAppManifestSection>& manifest,
      WebDatabase* db);
  WebDatabase::State AddPaymentMethodManifestImpl(
      const std::string& payment_method,
      const std::vector<std::string>& app_package_names,
      WebDatabase* db);
  std::unique_ptr<WDTypedResult> AddSecurePaymentConfirmationCredentialImpl(
      std::unique_ptr<SecurePaymentConfirmationCredential> credential,
      WebDatabase* db);

  std::unique_ptr<WDTypedResult> GetPaymentWebAppManifestImpl(
      const std::string& web_app,
      WebDatabase* db);
  std::unique_ptr<WDTypedResult> GetPaymentMethodManifestImpl(
      const std::string& payment_method,
      WebDatabase* db);
  std::unique_ptr<WDTypedResult> GetSecurePaymentConfirmationCredentialsImpl(
      std::vector<std::vector<uint8_t>> credential_ids,
      const std::string& relying_party_id,
      WebDatabase* db);

  std::map<WebDataServiceBase::Handle, base::OnceClosure>
      clearing_credentials_requests_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_WEB_DATA_SERVICE_H_
