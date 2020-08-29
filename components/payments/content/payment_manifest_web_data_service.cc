// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_manifest_web_data_service.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "components/payments/content/payment_method_manifest_table.h"
#include "components/payments/content/web_app_manifest_section_table.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_database_service.h"

namespace payments {

PaymentManifestWebDataService::PaymentManifestWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : WebDataServiceBase(std::move(wdbs), std::move(ui_task_runner)) {}

PaymentManifestWebDataService::~PaymentManifestWebDataService() {}

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
PaymentManifestWebDataService::AddSecurePaymentConfirmationInstrument(
    std::unique_ptr<SecurePaymentConfirmationInstrument> instrument,
    WebDataServiceConsumer* consumer) {
  DCHECK(instrument);
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&PaymentManifestWebDataService::
                         AddSecurePaymentConfirmationInstrumentImpl,
                     this, std::move(instrument)),
      consumer);
}

std::unique_ptr<WDTypedResult>
PaymentManifestWebDataService::AddSecurePaymentConfirmationInstrumentImpl(
    std::unique_ptr<SecurePaymentConfirmationInstrument> instrument,
    WebDatabase* db) {
  return std::make_unique<WDResult<bool>>(
      BOOL_RESULT, PaymentMethodManifestTable::FromWebDatabase(db)
                       ->AddSecurePaymentConfirmationInstrument(*instrument));
}

WebDataServiceBase::Handle
PaymentManifestWebDataService::GetSecurePaymentConfirmationInstruments(
    std::vector<std::vector<uint8_t>> credential_ids,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&PaymentManifestWebDataService::
                         GetSecurePaymentConfirmationInstrumentsImpl,
                     this, std::move(credential_ids)),
      consumer);
}

std::unique_ptr<WDTypedResult>
PaymentManifestWebDataService::GetSecurePaymentConfirmationInstrumentsImpl(
    std::vector<std::vector<uint8_t>> credential_ids,
    WebDatabase* db) {
  return std::make_unique<WDResult<
      std::vector<std::unique_ptr<SecurePaymentConfirmationInstrument>>>>(
      SECURE_PAYMENT_CONFIRMATION,
      PaymentMethodManifestTable::FromWebDatabase(db)
          ->GetSecurePaymentConfirmationInstruments(std::move(credential_ids)));
}

void PaymentManifestWebDataService::RemoveExpiredData(WebDatabase* db) {
  PaymentMethodManifestTable::FromWebDatabase(db)->RemoveExpiredData();
  WebAppManifestSectionTable::FromWebDatabase(db)->RemoveExpiredData();
}

}  // namespace payments
