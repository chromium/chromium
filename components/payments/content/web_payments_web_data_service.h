// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_WEB_PAYMENTS_WEB_DATA_SERVICE_H_
#define COMPONENTS_PAYMENTS_CONTENT_WEB_PAYMENTS_WEB_DATA_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/payments/content/browser_binding/browser_bound_key_metadata.h"
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
// WebPaymentsTable.
class WebPaymentsWebDataService : public WebDataServiceBase {
 public:
  WebPaymentsWebDataService(
      scoped_refptr<WebDatabaseService> wdbs,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  WebPaymentsWebDataService(const WebPaymentsWebDataService&) = delete;
  WebPaymentsWebDataService& operator=(const WebPaymentsWebDataService&) =
      delete;

  // Adds the web app `manifest`.
  void AddPaymentWebAppManifest(std::vector<WebAppManifestSection> manifest);

  // Adds the `payment_method`'s manifest.
  void AddPaymentMethodManifest(const std::string& payment_method,
                                std::vector<std::string> app_package_names);

  // Gets the `web_app`'s manifest and  returns it to the `callback`.
  WebDataServiceBase::Handle GetPaymentWebAppManifest(
      const std::string& web_app,
      WebDataServiceRequestCallback callback);

  // Gets the `payment_method`'s manifest and returns it the `callback`.
  WebDataServiceBase::Handle GetPaymentMethodManifest(
      const std::string& payment_method,
      WebDataServiceRequestCallback callback);

  // Adds the secure payment confirmation `credential` and returns a boolean
  // status to the `callback`. The `credential` should not be null.
  WebDataServiceBase::Handle AddSecurePaymentConfirmationCredential(
      std::unique_ptr<SecurePaymentConfirmationCredential> credential,
      WebDataServiceRequestCallback callback);

  // Gets the secure payment confirmation credential information for the given
  // `credential_ids` and returns it to the `callback`. Please use `std::move()`
  // for `credential_ids` parameter to avoid extra copies.
  virtual WebDataServiceBase::Handle GetSecurePaymentConfirmationCredentials(
      std::vector<std::vector<uint8_t>> credential_ids,
      const std::string& relying_party_id,
      WebDataServiceRequestCallback callback);

  // Clears all of the the secure payment confirmation credential information
  // created in the given time range `begin` and `end`, and invokes `callback`
  // when the clearing is completed.
  virtual void ClearSecurePaymentConfirmationCredentials(
      base::Time begin,
      base::Time end,
      base::OnceClosure callback);

  // Set the `browser_bound_key_id` for the given `credential_id` and
  // `relying_party_id`, and returns a boolean status to the `callback`.
  // An optional `last_used` timestamp can be provided to record when the
  // browser bound key was last used.
  virtual WebDataServiceBase::Handle SetBrowserBoundKey(
      std::vector<uint8_t> credential_id,
      std::string relying_party_id,
      std::vector<uint8_t> browser_bound_key_id,
      std::optional<base::Time> last_used,
      WebDataServiceRequestCallback callback);

  // Get the browser bound key id given the `credential_id` and
  // `relying_party_id`. Returns the key id or nullopt to the `callback`.
  virtual WebDataServiceBase::Handle GetBrowserBoundKey(
      const std::vector<uint8_t> credential_id,
      std::string relying_party_id,
      WebDataServiceRequestCallback callback);

  // Get all browser bound keys. Returns the BrowserBoundKeyMetadata structs in
  // a vector to the `callback`.
  virtual WebDataServiceBase::Handle GetAllBrowserBoundKeys(
      WebDataServiceRequestCallback callback);

  // Updates the `last_used` column of the browser bound key entry
  // identified by the given `credential_id` and `relying_party_id`, and returns
  // a boolean status to the `callback`.
  virtual WebDataServiceBase::Handle UpdateBrowserBoundKeyLastUsed(
      const std::vector<uint8_t> credential_id,
      std::string relying_party_id,
      base::Time last_used,
      WebDataServiceRequestCallback callback);

  // Deletes the browser bound keys associated to `passkeys` - the list of the
  // relying party and credential id pairs. `callback` is invoked once the
  // webdatabase operation has completed.
  virtual void DeleteBrowserBoundKeys(
      std::vector<BrowserBoundKeyMetadata::RelyingPartyAndCredentialId>
          passkeys,
      base::OnceClosure callback);

 protected:
  ~WebPaymentsWebDataService() override;

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
  std::unique_ptr<WDTypedResult> SetBrowserBoundKeyImpl(
      std::vector<uint8_t> credential_id,
      std::string relying_party_id,
      std::vector<uint8_t> browser_bound_key_id,
      std::optional<base::Time> last_used,
      WebDatabase* db);
  std::unique_ptr<WDTypedResult> GetBrowserBoundKeyImpl(
      std::vector<uint8_t> credential_id,
      std::string relying_party_id,
      WebDatabase* db);
  std::unique_ptr<WDTypedResult> GetAllBrowserBoundKeysImpl(WebDatabase* db);
  std::unique_ptr<WDTypedResult> UpdateBrowserBoundKeyLastUsedImpl(
      std::vector<uint8_t> credential_id,
      std::string relying_party_id,
      base::Time last_used,
      WebDatabase* db);
  WebDatabase::State DeleteBrowserBoundKeysImpl(
      std::vector<BrowserBoundKeyMetadata::RelyingPartyAndCredentialId>
          passkeys,
      base::OnceClosure callback,
      WebDatabase* db);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_WEB_PAYMENTS_WEB_DATA_SERVICE_H_
