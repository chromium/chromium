// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/passkey_browser_binder.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/browser_bound_key_metadata.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
#include "components/webdata/common/web_data_results.h"
#include "crypto/random.h"

namespace payments {
namespace {
// The length of the random browser bound key identifiers.
constexpr size_t kBrowserBoundKeyIdLength = 32;

// Converts a generic `result` of unique_ptr<WDTypedResult> to a vector of
// uint8_t, representing a BrowserBoundKey.
static std::vector<uint8_t> ConvertWDTypedResultToBrowserBoundKey(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  if (result) {
    CHECK(result->GetType() == BROWSER_BOUND_KEY);
    return static_cast<WDResult<std::optional<std::vector<uint8_t>>>*>(
               result.get())
        ->GetValue()
        .value_or(std::vector<uint8_t>());
  }
  return {};
}

// Converts a generic `result` of unique_ptr<WDTypedResult> to
// the vector of BrowserBoundKeyMetadata.
static std::vector<BrowserBoundKeyMetadata>
ConvertWDTypedResultToBrowserBoundKeyMetadata(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  if (result) {
    CHECK(result->GetType() == BROWSER_BOUND_KEY_METADATA);
    return static_cast<WDResult<std::vector<BrowserBoundKeyMetadata>>*>(
               result.get())
        ->GetValue();
  }
  return {};
}

// Converts a generic `result` of unique_ptr<WDTypedResult> to a boolean.
static bool ConvertWDTypedResultToBool(WebDataServiceBase::Handle handle,
                                       std::unique_ptr<WDTypedResult> result) {
  if (result) {
    CHECK(result->GetType() == WDResultType::BOOL_RESULT);
    return static_cast<WDResult<bool>*>(result.get())->GetValue();
  }
  return false;
}

}  // namespace

PasskeyBrowserBinder::PasskeyBrowserBinder(
    scoped_refptr<BrowserBoundKeyStore> key_store,
    scoped_refptr<WebPaymentsWebDataService> web_data_service)
    : key_store_(std::move(key_store)),
      web_data_service_(std::move(web_data_service)),
      random_bytes_as_vector_callback_(
          base::BindRepeating(crypto::RandBytesAsVector)) {
  CHECK(key_store_);
  CHECK(web_data_service_);
}

PasskeyBrowserBinder::~PasskeyBrowserBinder() = default;

PasskeyBrowserBinder::UnboundKey::UnboundKey(
    std::vector<uint8_t> browser_bound_key_id,
    std::unique_ptr<BrowserBoundKey> browser_bound_key,
    scoped_refptr<BrowserBoundKeyStore> key_store)
    : browser_bound_key_id_(std::move(browser_bound_key_id)),
      browser_bound_key_(std::move(browser_bound_key)),
      key_store_(std::move(key_store)) {}

PasskeyBrowserBinder::UnboundKey::UnboundKey(UnboundKey&&) = default;

PasskeyBrowserBinder::UnboundKey& PasskeyBrowserBinder::UnboundKey::operator=(
    UnboundKey&&) = default;

PasskeyBrowserBinder::UnboundKey::~UnboundKey() {
  // When browser_bound_key_ is still present, then we have not yet bound the
  // key, (in PasskeyBrowserBinder::BindKey()). To prevent this key from being
  // orphaned we delete it now.
  if (!browser_bound_key_id_.empty()) {
    key_store_->DeleteBrowserBoundKey(browser_bound_key_id_);
  }
}

BrowserBoundKey& PasskeyBrowserBinder::UnboundKey::Get() {
  return CHECK_DEREF(browser_bound_key_.get());
}

void PasskeyBrowserBinder::UnboundKey::MarkKeyBoundAndReset() {
  browser_bound_key_id_.clear();
}

void PasskeyBrowserBinder::CreateUnboundKey(
    const BrowserBoundKeyStore::CredentialInfoList& allowed_algorithms,
    base::OnceCallback<void(std::optional<UnboundKey>)> callback) {
  // Creates a new random identifier when new browser bound keys are
  // constructed. The returned value is used as the identifier for the browser
  // bound key to be created. The identifier is expected to be sufficiently
  // random to avoid collisions on chrome profile on one device.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(
          &BrowserBoundKeyStore::GetOrCreateBrowserBoundKeyForCredentialId,
          key_store_,
          random_bytes_as_vector_callback_.Run(kBrowserBoundKeyIdLength),
          allowed_algorithms),
      base::BindOnce(&PasskeyBrowserBinder::OnCreateUnboundKey,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PasskeyBrowserBinder::BindKey(UnboundKey key,
                                   const std::vector<uint8_t>& credential_id,
                                   const std::string& relying_party,
                                   std::optional<base::Time> last_used) {
  // Copy `key.browser_bound_key_id_`, as it must be passed to
  // `GetBrowserBoundKey` but `key` is also moved into the callback chain.
  std::vector<uint8_t> browser_bound_key_id_copy = key.browser_bound_key_id_;
  web_data_service_->SetBrowserBoundKey(
      credential_id, relying_party, browser_bound_key_id_copy,
      std::move(last_used),
      base::BindOnce(&ConvertWDTypedResultToBool)
          .Then(base::BindOnce(
              [](UnboundKey key, bool success) {
                if (success) {
                  key.MarkKeyBoundAndReset();
                  // Do not call methods on key past this point.
                }
              },
              std::move(key))));
}

void PasskeyBrowserBinder::UpdateKeyLastUsedToNow(
    const std::vector<uint8_t>& credential_id,
    const std::string& relying_party) {
  web_data_service_->UpdateBrowserBoundKeyLastUsed(
      credential_id, relying_party, base::Time::NowFromSystemTime(),
      base::BindOnce(&ConvertWDTypedResultToBool)
          .Then(base::BindOnce(RecordBrowserBoundKeyMetadataUpdated)));
}

void PasskeyBrowserBinder::GetAllBrowserBoundKeys(
    base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)> callback) {
  web_data_service_->GetAllBrowserBoundKeys(
      base::BindOnce(&ConvertWDTypedResultToBrowserBoundKeyMetadata)
          .Then(std::move(callback)));
}

void PasskeyBrowserBinder::DeleteBrowserBoundKeys(
    base::OnceClosure callback,
    std::vector<BrowserBoundKeyMetadata> bbk_metas) {
  if (bbk_metas.empty()) {
    std::move(callback).Run();
    return;
  }
  web_data_service_->DeleteBrowserBoundKeys(
      base::ToVector(bbk_metas,
                     [](auto& meta) { return std::move(meta.passkey); }),
      std::move(callback));
  for (const BrowserBoundKeyMetadata& bbk_meta : bbk_metas) {
    key_store_->DeleteBrowserBoundKey(bbk_meta.browser_bound_key_id);
  }
}

void PasskeyBrowserBinder::GetBoundKeyForPasskey(
    std::vector<uint8_t> credential_id,
    std::string relying_party,
    base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback) {
  web_data_service_->GetBrowserBoundKey(
      std::move(credential_id), std::move(relying_party),
      base::BindOnce(&ConvertWDTypedResultToBrowserBoundKey)
          .Then(base::BindOnce(&PasskeyBrowserBinder::GetBrowserBoundKey,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback))));
}

void PasskeyBrowserBinder::GetOrCreateBoundKeyForPasskey(
    std::vector<uint8_t> credential_id,
    std::string relying_party,
    const BrowserBoundKeyStore::CredentialInfoList& allowed_algorithms,
    std::optional<base::Time> last_used,
    base::OnceCallback<void(bool, std::unique_ptr<BrowserBoundKey>)> callback) {
  // Copy `credential_id` and `relying_party`, as they must be passed to
  // `GetBrowserBoundKey` and also moved into the callback chain.
  std::vector<uint8_t> credential_id_copy = credential_id;
  std::string relying_party_copy = relying_party;
  web_data_service_->GetBrowserBoundKey(
      std::move(credential_id_copy), std::move(relying_party_copy),
      base::BindOnce(&ConvertWDTypedResultToBrowserBoundKey)
          .Then(base::BindOnce(
              &PasskeyBrowserBinder::GetOrCreateBrowserBoundKey,
              weak_ptr_factory_.GetWeakPtr(), std::move(credential_id),
              std::move(relying_party), std::move(allowed_algorithms),
              std::move(last_used), std::move(callback))));
}

void PasskeyBrowserBinder::SetRandomBytesAsVectorCallbackForTesting(
    base::RepeatingCallback<std::vector<uint8_t>(size_t length)> callback) {
  random_bytes_as_vector_callback_ = std::move(callback);
}

BrowserBoundKeyStore*
PasskeyBrowserBinder::GetBrowserBoundKeyStoreForTesting() {
  return key_store_.get();
}

WebPaymentsWebDataService* PasskeyBrowserBinder::GetWebDataServiceForTesting() {
  return web_data_service_.get();
}

void PasskeyBrowserBinder::GetBrowserBoundKey(
    base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback,
    std::vector<uint8_t> existing_browser_bound_key_id) {
  if (existing_browser_bound_key_id.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // The BBK is only retrieved: With an empty `CredentialInfoList` no BBK will
  // be created.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(
          &BrowserBoundKeyStore::GetOrCreateBrowserBoundKeyForCredentialId,
          key_store_, existing_browser_bound_key_id,
          BrowserBoundKeyStore::CredentialInfoList{}),
      base::BindOnce(&PasskeyBrowserBinder::OnGetBrowserBoundKey,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PasskeyBrowserBinder::OnGetBrowserBoundKey(
    base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback,
    std::unique_ptr<BrowserBoundKey> browser_bound_key) {
  RecordCreationOrRetrieval(
      /*is_creation=*/false,
      /*did_succeed=*/!!browser_bound_key);
  std::move(callback).Run(std::move(browser_bound_key));
}

void PasskeyBrowserBinder::GetOrCreateBrowserBoundKey(
    std::vector<uint8_t> credential_id,
    std::string relying_party,
    BrowserBoundKeyStore::CredentialInfoList allowed_algorithms,
    std::optional<base::Time> last_used,
    base::OnceCallback<void(bool, std::unique_ptr<BrowserBoundKey>)> callback,
    std::vector<uint8_t> browser_bound_key_id) {
  bool needs_to_be_created = browser_bound_key_id.empty();
  if (needs_to_be_created) {
    browser_bound_key_id =
        random_bytes_as_vector_callback_.Run(kBrowserBoundKeyIdLength);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(
          &BrowserBoundKeyStore::GetOrCreateBrowserBoundKeyForCredentialId,
          key_store_, browser_bound_key_id, allowed_algorithms),
      base::BindOnce(&PasskeyBrowserBinder::OnGetOrCreateBrowserBoundKey,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(needs_to_be_created), std::move(credential_id),
                     std::move(relying_party), std::move(last_used),
                     std::move(callback)));
}

void PasskeyBrowserBinder::OnGetOrCreateBrowserBoundKey(
    bool needs_to_be_created,
    std::vector<uint8_t> credential_id,
    std::string relying_party,
    std::optional<base::Time> last_used,
    base::OnceCallback<void(bool, std::unique_ptr<BrowserBoundKey>)> callback,
    std::unique_ptr<BrowserBoundKey> browser_bound_key) {
  if (needs_to_be_created && browser_bound_key) {
    BindKey(UnboundKey(browser_bound_key->GetIdentifier(),
                       /*browser_bound_key=*/{}, key_store_),
            std::move(credential_id), std::move(relying_party),
            std::move(last_used));
  }
  RecordCreationOrRetrieval(/*is_creation=*/needs_to_be_created,
                            /*did_succeed=*/!!browser_bound_key);
  std::move(callback).Run(/*is_new=*/needs_to_be_created,
                          std::move(browser_bound_key));
}

void PasskeyBrowserBinder::OnCreateUnboundKey(
    base::OnceCallback<void(std::optional<UnboundKey>)> callback,
    std::unique_ptr<BrowserBoundKey> browser_bound_key) {
  RecordCreationOrRetrieval(
      /*is_creation=*/true,
      /*did_succeed=*/!!browser_bound_key);
  if (browser_bound_key) {
    // Copy the BBK ID to avoid any use after move errors.
    auto bbk_id = browser_bound_key->GetIdentifier();
    std::move(callback).Run(PasskeyBrowserBinder::UnboundKey(
        std::move(bbk_id), std::move(browser_bound_key), key_store_));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void PasskeyBrowserBinder::RecordCreationOrRetrieval(bool is_creation,
                                                     bool did_succeed) {
  bool has_device_hardware_support =
      key_store_->GetDeviceSupportsHardwareKeys();
  auto metrics_result =
      did_succeed
          ? (has_device_hardware_support
                 ? SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
                       kSuccessWithDeviceHardware
                 : SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
                       kSuccessWithoutDeviceHardware)
          : (has_device_hardware_support
                 ? SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
                       kFailureWithDeviceHardware
                 : SecurePaymentConfirmationBrowserBoundKeyDeviceResult::
                       kFailureWithoutDeviceHardware);
  if (is_creation) {
    RecordBrowserBoundKeyCreation(metrics_result);
  } else {
    RecordBrowserBoundKeyRetrieval(metrics_result);
  }
}

}  // namespace payments
