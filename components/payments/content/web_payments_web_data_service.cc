// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/web_payments_web_data_service.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/payments/content/web_app_manifest_section_table.h"
#include "components/payments/content/web_payments_table.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_database_service.h"

namespace payments {

WebPaymentsWebDataService::WebPaymentsWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : WebDataServiceBase(std::move(wdbs), std::move(ui_task_runner)) {}

WebPaymentsWebDataService::~WebPaymentsWebDataService() = default;

void WebPaymentsWebDataService::AddPaymentWebAppManifest(
    std::vector<WebAppManifestSection> manifest) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::AddPaymentWebAppManifestImpl,
                     this, std::move(manifest)));
}

WebDatabase::State WebPaymentsWebDataService::AddPaymentWebAppManifestImpl(
    const std::vector<WebAppManifestSection>& manifest,
    WebDatabase* db) {
  if (WebAppManifestSectionTable::FromWebDatabase(db)->AddWebAppManifest(
          manifest)) {
    return WebDatabase::COMMIT_NEEDED;
  }

  return WebDatabase::COMMIT_NOT_NEEDED;
}

void WebPaymentsWebDataService::AddPaymentMethodManifest(
    const std::string& payment_method,
    std::vector<std::string> app_package_names) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::AddPaymentMethodManifestImpl,
                     this, payment_method, std::move(app_package_names)));
}

WebDatabase::State WebPaymentsWebDataService::AddPaymentMethodManifestImpl(
    const std::string& payment_method,
    const std::vector<std::string>& app_package_names,
    WebDatabase* db) {
  if (WebPaymentsTable::FromWebDatabase(db)->AddManifest(payment_method,
                                                         app_package_names)) {
    return WebDatabase::COMMIT_NEEDED;
  }

  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDataServiceBase::Handle WebPaymentsWebDataService::GetPaymentWebAppManifest(
    const std::string& web_app,
    WebDataServiceRequestCallback callback) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::GetPaymentWebAppManifestImpl,
                     this, web_app),
      std::move(callback));
}

std::unique_ptr<WDTypedResult>
WebPaymentsWebDataService::GetPaymentWebAppManifestImpl(
    const std::string& web_app,
    WebDatabase* db) {
  RemoveExpiredData(db);
  return std::make_unique<WDResult<std::vector<WebAppManifestSection>>>(
      PAYMENT_WEB_APP_MANIFEST,
      WebAppManifestSectionTable::FromWebDatabase(db)->GetWebAppManifest(
          web_app));
}

WebDataServiceBase::Handle WebPaymentsWebDataService::GetPaymentMethodManifest(
    const std::string& payment_method,
    WebDataServiceRequestCallback callback) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::GetPaymentMethodManifestImpl,
                     this, payment_method),
      std::move(callback));
}

std::unique_ptr<WDTypedResult>
WebPaymentsWebDataService::GetPaymentMethodManifestImpl(
    const std::string& payment_method,
    WebDatabase* db) {
  RemoveExpiredData(db);
  return std::make_unique<WDResult<std::vector<std::string>>>(
      PAYMENT_METHOD_MANIFEST,
      WebPaymentsTable::FromWebDatabase(db)->GetManifest(payment_method));
}

WebDataServiceBase::Handle
WebPaymentsWebDataService::AddSecurePaymentConfirmationCredential(
    std::unique_ptr<SecurePaymentConfirmationCredential> credential,
    WebDataServiceRequestCallback callback) {
  DCHECK(credential);
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::
                         AddSecurePaymentConfirmationCredentialImpl,
                     this, std::move(credential)),
      std::move(callback));
}

std::unique_ptr<WDTypedResult>
WebPaymentsWebDataService::AddSecurePaymentConfirmationCredentialImpl(
    std::unique_ptr<SecurePaymentConfirmationCredential> credential,
    WebDatabase* db) {
  return std::make_unique<WDResult<bool>>(
      BOOL_RESULT, WebPaymentsTable::FromWebDatabase(db)
                       ->AddSecurePaymentConfirmationCredential(*credential));
}

WebDataServiceBase::Handle
WebPaymentsWebDataService::GetSecurePaymentConfirmationCredentials(
    std::vector<std::vector<uint8_t>> credential_ids,
    const std::string& relying_party_id,
    WebDataServiceRequestCallback callback) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::
                         GetSecurePaymentConfirmationCredentialsImpl,
                     this, std::move(credential_ids),
                     std::move(relying_party_id)),
      std::move(callback));
}

std::unique_ptr<WDTypedResult>
WebPaymentsWebDataService::GetSecurePaymentConfirmationCredentialsImpl(
    std::vector<std::vector<uint8_t>> credential_ids,
    const std::string& relying_party_id,
    WebDatabase* db) {
  return std::make_unique<WDResult<
      std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>>(
      SECURE_PAYMENT_CONFIRMATION,
      WebPaymentsTable::FromWebDatabase(db)
          ->GetSecurePaymentConfirmationCredentials(
              std::move(credential_ids), std::move(relying_party_id)));
}

WebDataServiceBase::Handle WebPaymentsWebDataService::SetBrowserBoundKey(
    std::vector<uint8_t> credential_id,
    std::string relying_party_id,
    std::vector<uint8_t> browser_bound_key_id,
    std::optional<base::Time> last_used,
    WebDataServiceRequestCallback callback) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::SetBrowserBoundKeyImpl, this,
                     std::move(credential_id), std::move(relying_party_id),
                     std::move(browser_bound_key_id), std::move(last_used)),
      std::move(callback));
}

std::unique_ptr<WDTypedResult>
WebPaymentsWebDataService::SetBrowserBoundKeyImpl(
    std::vector<uint8_t> credential_id,
    std::string relying_party_id,
    std::vector<uint8_t> browser_bound_key_id,
    std::optional<base::Time> last_used,
    WebDatabase* db) {
  return std::make_unique<WDResult<bool>>(
      BOOL_RESULT, WebPaymentsTable::FromWebDatabase(db)->SetBrowserBoundKey(
                       std::move(credential_id), std::move(relying_party_id),
                       std::move(browser_bound_key_id), std::move(last_used)));
}

WebDataServiceBase::Handle WebPaymentsWebDataService::GetBrowserBoundKey(
    std::vector<uint8_t> credential_id,
    std::string relying_party_id,
    WebDataServiceRequestCallback callback) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::GetBrowserBoundKeyImpl, this,
                     std::move(credential_id), std::move(relying_party_id)),
      std::move(callback));
}

std::unique_ptr<WDTypedResult>
WebPaymentsWebDataService::GetBrowserBoundKeyImpl(
    std::vector<uint8_t> credential_id,
    std::string relying_party_id,
    WebDatabase* db) {
  return std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
      BROWSER_BOUND_KEY,
      WebPaymentsTable::FromWebDatabase(db)->GetBrowserBoundKey(
          std::move(credential_id), std::move(relying_party_id)));
}

WebDataServiceBase::Handle WebPaymentsWebDataService::GetAllBrowserBoundKeys(
    WebDataServiceRequestCallback callback) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::GetAllBrowserBoundKeysImpl,
                     this),
      std::move(callback));
}

std::unique_ptr<WDTypedResult>
WebPaymentsWebDataService::GetAllBrowserBoundKeysImpl(WebDatabase* db) {
  return std::make_unique<WDResult<std::vector<BrowserBoundKeyMetadata>>>(
      BROWSER_BOUND_KEY_METADATA,
      WebPaymentsTable::FromWebDatabase(db)->GetAllBrowserBoundKeys());
}

WebDataServiceBase::Handle
WebPaymentsWebDataService::UpdateBrowserBoundKeyLastUsed(
    std::vector<uint8_t> credential_id,
    std::string relying_party_id,
    base::Time last_used,
    WebDataServiceRequestCallback callback) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(
          &WebPaymentsWebDataService::UpdateBrowserBoundKeyLastUsedImpl, this,
          std::move(credential_id), std::move(relying_party_id),
          std::move(last_used)),
      std::move(callback));
}

std::unique_ptr<WDTypedResult>
WebPaymentsWebDataService::UpdateBrowserBoundKeyLastUsedImpl(
    std::vector<uint8_t> credential_id,
    std::string relying_party_id,
    base::Time last_used,
    WebDatabase* db) {
  return std::make_unique<WDResult<bool>>(
      BOOL_RESULT, WebPaymentsTable::FromWebDatabase(db)
                       ->UpdateBrowserBoundKeyLastUsedColumn(
                           std::move(credential_id),
                           std::move(relying_party_id), std::move(last_used)));
}

void WebPaymentsWebDataService::DeleteBrowserBoundKeys(
    std::vector<BrowserBoundKeyMetadata::RelyingPartyAndCredentialId> passkeys,
    base::OnceClosure callback) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::DeleteBrowserBoundKeysImpl,
                     this, std::move(passkeys),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

WebDatabase::State WebPaymentsWebDataService::DeleteBrowserBoundKeysImpl(
    std::vector<BrowserBoundKeyMetadata::RelyingPartyAndCredentialId> passkeys,
    base::OnceClosure callback,
    WebDatabase* db) {
  if (WebPaymentsTable::FromWebDatabase(db)->DeleteBrowserBoundKeys(
          std::move(passkeys))) {
    std::move(callback).Run();
    return WebDatabase::State::COMMIT_NEEDED;
  }

  std::move(callback).Run();
  return WebDatabase::State::COMMIT_NOT_NEEDED;
}

void WebPaymentsWebDataService::ClearSecurePaymentConfirmationCredentials(
    base::Time begin,
    base::Time end,
    base::OnceClosure callback) {
  wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce(&WebPaymentsWebDataService::
                         ClearSecurePaymentConfirmationCredentialsImpl,
                     this, begin, end),
      base::IgnoreArgs<WebDataServiceBase::Handle,
                       std::unique_ptr<WDTypedResult>>(std::move(callback)));
}

std::unique_ptr<WDTypedResult>
WebPaymentsWebDataService::ClearSecurePaymentConfirmationCredentialsImpl(
    base::Time begin,
    base::Time end,
    WebDatabase* db) {
  return std::make_unique<WDResult<bool>>(
      BOOL_RESULT, WebPaymentsTable::FromWebDatabase(db)
                       ->ClearSecurePaymentConfirmationCredentials(begin, end));
}



void WebPaymentsWebDataService::RemoveExpiredData(WebDatabase* db) {
  WebAppManifestSectionTable::FromWebDatabase(db)->RemoveExpiredData();
  WebPaymentsTable::FromWebDatabase(db)->RemoveExpiredData();
}

}  // namespace payments
