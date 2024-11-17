// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_manifest_web_data_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/payments/content/payment_method_manifest_table.h"
#include "components/payments/content/web_app_manifest_section_table.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_database_service.h"

namespace payments {

PaymentManifestWebDataService::PaymentManifestWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : WebDataServiceBase(std::move(wdbs), std::move(ui_task_runner)) {}

PaymentManifestWebDataService::~PaymentManifestWebDataService() = default;

void PaymentManifestWebDataService::AddPaymentWebAppManifest(
    std::vector<WebAppManifestSection> manifest) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(
          &PaymentManifestWebDataService::AddPaymentWebAppManifestImpl, this,
          std::move(manifest)));
}

WebDatabase::State PaymentManifestWebDataService::AddPaymentWebAppManifestImpl(
    const std::vector<WebAppManifestSection>& manifest,
    WebDatabase* db) {
  if (WebAppManifestSectionTable::FromWebDatabase(db)->AddWebAppManifest(
          manifest)) {
    return WebDatabase::COMMIT_NEEDED;
  }

  return WebDatabase::COMMIT_NOT_NEEDED;
}

void PaymentManifestWebDataService::AddPaymentMethodManifest(
    const std::string& payment_method,
    std::vector<std::string> app_package_names) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(
          &PaymentManifestWebDataService::AddPaymentMethodManifestImpl, this,
          payment_method, std::move(app_package_names)));
}

WebDatabase::State PaymentManifestWebDataService::AddPaymentMethodManifestImpl(
    const std::string& payment_method,
    const std::vector<std::string>& app_package_names,
    WebDatabase* db) {
  if (PaymentMethodManifestTable::FromWebDatabase(db)->AddManifest(
          payment_method, app_package_names)) {
    return WebDatabase::COMMIT_NEEDED;
  }

  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDataServiceBase::Handle
PaymentManifestWebDataService::GetPaymentWebAppManifest(
    const std::string& web_app,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(
          &PaymentManifestWebDataService::GetPaymentWebAppManifestImpl, this,
          web_app),
      consumer);
}

std::unique_ptr<WDTypedResult>
PaymentManifestWebDataService::GetPaymentWebAppManifestImpl(
    const std::string& web_app,
    WebDatabase* db) {
  RemoveExpiredData(db);
  return std::make_unique<WDResult<std::vector<WebAppManifestSection>>>(
      PAYMENT_WEB_APP_MANIFEST,
      WebAppManifestSectionTable::FromWebDatabase(db)->GetWebAppManifest(
          web_app));
}

WebDataServiceBase::Handle
PaymentManifestWebDataService::GetPaymentMethodManifest(
    const std::string& payment_method,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(
          &PaymentManifestWebDataService::GetPaymentMethodManifestImpl, this,
          payment_method),
      consumer);
}

std::unique_ptr<WDTypedResult>
PaymentManifestWebDataService::GetPaymentMethodManifestImpl(
    const std::string& payment_method,
    WebDatabase* db) {
  RemoveExpiredData(db);
  return std::make_unique<WDResult<std::vector<std::string>>>(
      PAYMENT_METHOD_MANIFEST,
      PaymentMethodManifestTable::FromWebDatabase(db)->GetManifest(
          payment_method));
}

WebDataServiceBase::Handle
PaymentManifestWebDataService::AddSecurePaymentConfirmationCredential(
    std::unique_ptr<SecurePaymentConfirmationCredential> credential,
    WebDataServiceConsumer* consumer) {
  DCHECK(credential);
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&PaymentManifestWebDataService::
                         AddSecurePaymentConfirmationCredentialImpl,
                     this, std::move(credential)),
      consumer);
}

std::unique_ptr<WDTypedResult>
PaymentManifestWebDataService::AddSecurePaymentConfirmationCredentialImpl(
    std::unique_ptr<SecurePaymentConfirmationCredential> credential,
    WebDatabase* db) {
  return std::make_unique<WDResult<bool>>(
      BOOL_RESULT, PaymentMethodManifestTable::FromWebDatabase(db)
                       ->AddSecurePaymentConfirmationCredential(*credential));
}

WebDataServiceBase::Handle
PaymentManifestWebDataService::GetSecurePaymentConfirmationCredentials(
    std::vector<std::vector<uint8_t>> credential_ids,
    const std::string& relying_party_id,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&PaymentManifestWebDataService::
                         GetSecurePaymentConfirmationCredentialsImpl,
                     this, std::move(credential_ids),
                     std::move(relying_party_id)),
      consumer);
}

std::unique_ptr<WDTypedResult>
PaymentManifestWebDataService::GetSecurePaymentConfirmationCredentialsImpl(
    std::vector<std::vector<uint8_t>> credential_ids,
    const std::string& relying_party_id,
    WebDatabase* db) {
  return std::make_unique<WDResult<
      std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>>(
      SECURE_PAYMENT_CONFIRMATION,
      PaymentMethodManifestTable::FromWebDatabase(db)
          ->GetSecurePaymentConfirmationCredentials(
              std::move(credential_ids), std::move(relying_party_id)));
}

void PaymentManifestWebDataService::ClearSecurePaymentConfirmationCredentials(
    base::Time begin,
    base::Time end,
    base::OnceClosure callback) {
  WebDataServiceBase::Handle handle = wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&PaymentManifestWebDataService::
                         ClearSecurePaymentConfirmationCredentialsImpl,
                     this, begin, end),
      this);
  clearing_credentials_requests_[handle] = std::move(callback);
}

std::unique_ptr<WDTypedResult>
PaymentManifestWebDataService::ClearSecurePaymentConfirmationCredentialsImpl(
    base::Time begin,
    base::Time end,
    WebDatabase* db) {
  return std::make_unique<WDResult<bool>>(
      BOOL_RESULT, PaymentMethodManifestTable::FromWebDatabase(db)
                       ->ClearSecurePaymentConfirmationCredentials(begin, end));
}

void PaymentManifestWebDataService::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  if (clearing_credentials_requests_.find(h) ==
      clearing_credentials_requests_.end())
    return;

  std::move(clearing_credentials_requests_[h]).Run();
}

void PaymentManifestWebDataService::RemoveExpiredData(WebDatabase* db) {
  PaymentMethodManifestTable::FromWebDatabase(db)->RemoveExpiredData();
  WebAppManifestSectionTable::FromWebDatabase(db)->RemoveExpiredData();
}

}  // namespace payments
