// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/passkey_browser_binder.h"

#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/webdata/common/web_data_results.h"
#include "crypto/random.h"

namespace payments {
namespace {
// The length of the random browser bound key identifiers.
constexpr size_t kBrowserBoundKeyIdLength = 32;

// Returns the callback matching `handle` and erases it from the `handlers` map.
template <typename CallbackType>
static CallbackType RemoveHandler(
    std::map<WebDataServiceBase::Handle, CallbackType>& handlers,
    WebDataServiceBase::Handle handle) {
  auto callback_iterator = handlers.find(handle);
  CHECK(callback_iterator != handlers.end());
  CallbackType callback = std::move(callback_iterator->second);
  handlers.erase(callback_iterator);
  return callback;
}

}  // namespace

PasskeyBrowserBinder::PasskeyBrowserBinder(
    std::unique_ptr<BrowserBoundKeyStore> key_store,
    scoped_refptr<PaymentManifestWebDataService> web_data_service)
    : key_store_(std::move(key_store)),
      web_data_service_(web_data_service),
      random_bytes_as_vector_callback_(
          base::BindRepeating(crypto::RandBytesAsVector)) {}

PasskeyBrowserBinder::~PasskeyBrowserBinder() = default;

PasskeyBrowserBinder::UnboundKey::UnboundKey(
    std::vector<uint8_t> browser_bound_key_id,
    std::unique_ptr<BrowserBoundKey> browser_bound_key)
    : browser_bound_key_id_(std::move(browser_bound_key_id)),
      browser_bound_key_(std::move(browser_bound_key)) {}

PasskeyBrowserBinder::UnboundKey::UnboundKey(UnboundKey&&) = default;

PasskeyBrowserBinder::UnboundKey& PasskeyBrowserBinder::UnboundKey::operator=(
    UnboundKey&&) = default;

// TODO(crbug.com/390441081) If the key has not been associated, delete it.
PasskeyBrowserBinder::UnboundKey::~UnboundKey() = default;

std::optional<PasskeyBrowserBinder::UnboundKey>
PasskeyBrowserBinder::CreateUnboundKey(
    const BrowserBoundKeyStore::CredentialInfoList& allowed_credentials) {
  // Creates a new random identifier when new browser bound keys are
  // constructed. The returned value is used as the identifier for the browser
  // bound key to be created. The identifier is expected to be sufficiently
  // random to avoid collisions on chrome profile on one device.
  std::vector<uint8_t> browser_bound_key_id =
      random_bytes_as_vector_callback_.Run(kBrowserBoundKeyIdLength);
  std::unique_ptr<BrowserBoundKey> browser_bound_key =
      key_store_->GetOrCreateBrowserBoundKeyForCredentialId(
          browser_bound_key_id, allowed_credentials);
  if (!browser_bound_key) {
    return std::nullopt;
  }
  return PasskeyBrowserBinder::UnboundKey(std::move(browser_bound_key_id),
                                          std::move(browser_bound_key));
}

void PasskeyBrowserBinder::BindKey(UnboundKey key,
                                   const std::vector<uint8_t>& credential_id,
                                   const std::string& relying_party) {
  if (web_data_service_) {
    // TODO(crbug.com/384954763): Delete the browser bound key from the key
    // store if the result was false (not successful).
    WebDataServiceBase::Handle handle = web_data_service_->SetBrowserBoundKey(
        credential_id, relying_party, std::move(key.browser_bound_key_id_),
        /*consumer=*/this);
    set_browser_bound_key_handlers_[handle] = base::DoNothing();
  }
}

void PasskeyBrowserBinder::GetOrCreateBoundKeyForPasskey(
    std::vector<uint8_t> credential_id,
    std::string relying_party,
    const BrowserBoundKeyStore::CredentialInfoList& allowed_credentials,
    base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback) {
  if (!web_data_service_) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto handle = web_data_service_->GetBrowserBoundKey(
      credential_id, relying_party, /*consumer=*/this);
  // The call back must not strongly reference this to avoid strong reference
  // cycles.
  get_browser_bound_key_handlers_[handle] =
      base::BindOnce(&PasskeyBrowserBinder::GetOrCreateBrowserBoundKey,
                     weak_ptr_factory_.GetWeakPtr(), std::move(credential_id),
                     std::move(relying_party), std::move(allowed_credentials),
                     std::move(callback));
}

void PasskeyBrowserBinder::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  switch (result->GetType()) {
    case WDResultType::BOOL_RESULT:
      RemoveHandler(set_browser_bound_key_handlers_, handle)
          .Run(result ? static_cast<WDResult<bool>*>(result.get())->GetValue()
                      : false);
      break;
    case WDResultType::BROWSER_BOUND_KEY: {
      std::optional<std::vector<uint8_t>> result_value;
      if (result) {
        result_value =
            static_cast<WDResult<std::optional<std::vector<uint8_t>>>*>(
                result.get())
                ->GetValue();
      }
      RemoveHandler(get_browser_bound_key_handlers_, handle)
          .Run(result_value.value_or(std::vector<uint8_t>()));
      break;
    }
    default:
      NOTREACHED();
  }
}

void PasskeyBrowserBinder::SetRandomBytesAsVectorCallbackForTesting(
    base::RepeatingCallback<std::vector<uint8_t>(size_t length)> callback) {
  random_bytes_as_vector_callback_ = std::move(callback);
}

BrowserBoundKeyStore*
PasskeyBrowserBinder::GetBrowserBoundKeyStoreForTesting() {
  return key_store_.get();
}

PaymentManifestWebDataService*
PasskeyBrowserBinder::GetWebDataServiceForTesting() {
  return web_data_service_.get();
}

void PasskeyBrowserBinder::GetOrCreateBrowserBoundKey(
    std::vector<uint8_t> credential_id,
    std::string relying_party,
    BrowserBoundKeyStore::CredentialInfoList allowed_credentials,
    base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback,
    std::vector<uint8_t> browser_bound_key_id) {
  if (browser_bound_key_id.empty()) {
    browser_bound_key_id =
        random_bytes_as_vector_callback_.Run(kBrowserBoundKeyIdLength);
    // TODO(crbug.com/384954763): Delete the browser bound key from the key
    // store if the result was false (not successful).
    WebDataServiceBase::Handle handle = web_data_service_->SetBrowserBoundKey(
        std::move(credential_id), std::move(relying_party),
        browser_bound_key_id,
        /*consumer=*/this);
    set_browser_bound_key_handlers_[handle] = base::DoNothing();
  }
  std::move(callback).Run(key_store_->GetOrCreateBrowserBoundKeyForCredentialId(
      browser_bound_key_id, allowed_credentials));
}

}  // namespace payments
