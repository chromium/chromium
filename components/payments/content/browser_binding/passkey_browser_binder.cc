// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/passkey_browser_binder.h"

#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/base64.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/browser_bound_key_metadata.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/webdata/common/web_data_results.h"
#include "crypto/random.h"

namespace payments {
namespace {
// The length of the random browser bound key identifiers.
constexpr size_t kBrowserBoundKeyIdLength = 32;

using GetMatchingCredentialIdsCallback = base::RepeatingCallback<void(
    const std::string& relying_party_id,
    const std::vector<std::vector<uint8_t>>& credential_ids,
    bool require_third_party_payment_bit_set,
    base::OnceCallback<void(std::vector<std::vector<uint8_t>>)>)>;

// Type for a map from string (relying party) to a vector of BBK metadata.
using RelyingPartyToBkkMetadata =
    base::flat_map<std::string, std::vector<BrowserBoundKeyMetadata>>;

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

// Converts a generic `result` of unique_ptr<WDTypedResult> to
// the vector of BrowserBoundKeyMetadata.
static std::vector<BrowserBoundKeyMetadata>
ConvertBrowserBoundKeyMetadataResult(WebDataServiceBase::Handle handle,
                                     std::unique_ptr<WDTypedResult> result) {
  if (result) {
    CHECK(result->GetType() == BROWSER_BOUND_KEY_METADATA);
    return static_cast<WDResult<std::vector<BrowserBoundKeyMetadata>>*>(
               result.get())
        ->GetValue();
  } else {
    return {};
  }
}

static RelyingPartyToBkkMetadata GroupByRelyingPartyId(
    std::vector<BrowserBoundKeyMetadata> bbk_metas) {
  RelyingPartyToBkkMetadata grouped;
  for (auto& bbk_meta : bbk_metas) {
    grouped[bbk_meta.passkey.relying_party_id].push_back(std::move(bbk_meta));
  }
  return grouped;
}

static std::vector<BrowserBoundKeyMetadata> RemoveMatchingCredentialIds(
    std::vector<BrowserBoundKeyMetadata> bbks,
    std::vector<std::vector<uint8_t>> matching_credential_ids) {
  std::erase_if(bbks, [&matching_credential_ids](auto& bbk) {
    return base::Contains(matching_credential_ids, bbk.passkey.credential_id);
  });
  return bbks;
}

// Flattens a vector of vector of metadata to a vector of metadata.
static std::vector<BrowserBoundKeyMetadata> Flatten(
    std::vector<std::vector<BrowserBoundKeyMetadata>> nested) {
  std::vector<BrowserBoundKeyMetadata> flattened;
  for (auto& inner : nested) {
    std::ranges::move(inner, std::back_inserter(flattened));
  }
  return flattened;
}

// Finds the BBKs in `stored_bbks` that are no longer present in
// `get_matching_credential_ids_callback`. `callback` will be invoked with a
// vector of the browser bound key metadatas that are no longer valid.
static void FindDeletedPasskeys(
    GetMatchingCredentialIdsCallback get_matching_credential_ids_callback,
    base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)> callback,
    std::vector<BrowserBoundKeyMetadata> stored_bbks) {
  RelyingPartyToBkkMetadata bbks_by_relying_party =
      GroupByRelyingPartyId(std::move(stored_bbks));
  auto barrier_callback =
      base::BarrierCallback<std::vector<BrowserBoundKeyMetadata>>(
          bbks_by_relying_party.size(),
          base::BindOnce(&Flatten).Then(std::move(callback)));
  for (std::pair<std::string, std::vector<BrowserBoundKeyMetadata>>& entry :
       bbks_by_relying_party) {
    auto stored_credential_ids =
        base::ToVector(entry.second, [](const BrowserBoundKeyMetadata& bbk) {
          return bbk.passkey.credential_id;
        });
    get_matching_credential_ids_callback.Run(
        entry.first, stored_credential_ids,
        /*require_third_party_payment_bit=*/false,
        base::BindOnce(&RemoveMatchingCredentialIds, std::move(entry.second))
            .Then(barrier_callback));
  }
}

}  // namespace

PasskeyBrowserBinder::PasskeyBrowserBinder(
    scoped_refptr<BrowserBoundKeyStore> key_store,
    scoped_refptr<PaymentManifestWebDataService> web_data_service)
    : key_store_(std::move(key_store)),
      web_data_service_(web_data_service),
      random_bytes_as_vector_callback_(
          base::BindRepeating(crypto::RandBytesAsVector)) {}

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
  if (browser_bound_key_) {
    key_store_->DeleteBrowserBoundKey(browser_bound_key_->GetIdentifier());
  }
}

void PasskeyBrowserBinder::UnboundKey::MarkKeyBoundAndReset() {
  browser_bound_key_ = nullptr;
}

std::optional<PasskeyBrowserBinder::UnboundKey>
PasskeyBrowserBinder::CreateUnboundKey(
    const BrowserBoundKeyStore::CredentialInfoList& allowed_algorithms) {
  // Creates a new random identifier when new browser bound keys are
  // constructed. The returned value is used as the identifier for the browser
  // bound key to be created. The identifier is expected to be sufficiently
  // random to avoid collisions on chrome profile on one device.
  std::vector<uint8_t> browser_bound_key_id =
      random_bytes_as_vector_callback_.Run(kBrowserBoundKeyIdLength);
  std::unique_ptr<BrowserBoundKey> browser_bound_key =
      key_store_->GetOrCreateBrowserBoundKeyForCredentialId(
          browser_bound_key_id, allowed_algorithms);
  if (!browser_bound_key) {
    return std::nullopt;
  }
  return PasskeyBrowserBinder::UnboundKey(std::move(browser_bound_key_id),
                                          std::move(browser_bound_key),
                                          key_store_);
}

void PasskeyBrowserBinder::BindKey(UnboundKey key,
                                   const std::vector<uint8_t>& credential_id,
                                   const std::string& relying_party) {
  if (web_data_service_) {
    WebDataServiceBase::Handle handle = web_data_service_->SetBrowserBoundKey(
        credential_id, relying_party, std::move(key.browser_bound_key_id_),
        /*consumer=*/this);
    set_browser_bound_key_handlers_[handle] = base::BindOnce(
        [](UnboundKey key, bool success) {
          if (success) {
            key.MarkKeyBoundAndReset();
            // Do not call methods on key past this point.
          }
        },
        std::move(key));
  }
}

void PasskeyBrowserBinder::DeleteAllUnknownBrowserBoundKeys(
    GetMatchingCredentialIdsCallback get_matching_credential_ids_callback,
    base::OnceClosure callback) {
  // `callback` may be holding the reference to this PasskeyBrowserBinder, so
  // after `callback` is run `this` may not be accessed.
  base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>
      delete_browser_bound_keys_and_finish =
          base::BindOnce(&PasskeyBrowserBinder::DeleteBrowserBoundKeys,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  web_data_service_->GetAllBrowserBoundKeys(
      base::BindOnce(&ConvertBrowserBoundKeyMetadataResult)
          .Then(base::BindOnce(
              &FindDeletedPasskeys, get_matching_credential_ids_callback,
              std::move(delete_browser_bound_keys_and_finish))));
}

void PasskeyBrowserBinder::DeleteBrowserBoundKeys(
    base::OnceClosure callback,
    std::vector<BrowserBoundKeyMetadata> stale_bbk_metas) {
  if (stale_bbk_metas.empty()) {
    return;
  }
  web_data_service_->DeleteBrowserBoundKeys(
      base::ToVector(stale_bbk_metas,
                     [](auto& meta) { return std::move(meta.passkey); }),
      std::move(callback));
}

void PasskeyBrowserBinder::GetBoundKeyForPasskey(
    std::vector<uint8_t> credential_id,
    std::string relying_party,
    base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback) {
  if (!web_data_service_) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto handle = web_data_service_->GetBrowserBoundKey(
      std::move(credential_id), std::move(relying_party), /*consumer=*/this);
  get_browser_bound_key_handlers_[handle] =
      base::BindOnce(&PasskeyBrowserBinder::GetBrowserBoundKey,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
}

void PasskeyBrowserBinder::GetOrCreateBoundKeyForPasskey(
    std::vector<uint8_t> credential_id,
    std::string relying_party,
    const BrowserBoundKeyStore::CredentialInfoList& allowed_algorithms,
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
                     std::move(relying_party), std::move(allowed_algorithms),
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

void PasskeyBrowserBinder::GetBrowserBoundKey(
    base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback,
    std::vector<uint8_t> existing_browser_bound_key_id) {
  if (existing_browser_bound_key_id.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }
  // The BBK is only retrieved: With an empty `allowed_algorithms` no BBK will
  // be created.
  std::move(callback).Run(key_store_->GetOrCreateBrowserBoundKeyForCredentialId(
      existing_browser_bound_key_id, /*allowed_algorithms=*/{}));
}

void PasskeyBrowserBinder::GetOrCreateBrowserBoundKey(
    std::vector<uint8_t> credential_id,
    std::string relying_party,
    BrowserBoundKeyStore::CredentialInfoList allowed_algorithms,
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
      browser_bound_key_id, allowed_algorithms));
}

}  // namespace payments
