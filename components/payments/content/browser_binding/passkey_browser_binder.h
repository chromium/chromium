// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_PASSKEY_BROWSER_BINDER_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_PASSKEY_BROWSER_BINDER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace payments {

class BrowserBoundKey;
struct BrowserBoundKeyMetadata;

// Facilitates binding browser bound keys to passkeys.
//
// Browser bound keys (BBK) are temporarily not bound to passkeys while creating
// the passkey. The BBK needs to be created before the passkey since the BBK's
// public key will be set in the client data json and itself is an input to
// creating the passkey. Once the passkey has been created then its credential
// id becomes known and the BBK can be bound to the passkey.
//
// Example passkey creation usage:
//
// auto binder = PasskeyBrowserBinder(
//     std::move(key_store), web_payments_web_data_service);
//
// // Create an unbound key.
// PasskeyBrowserBinder::UnboundKey unbound_key =
//     binder.CreateUnboundKey(std::move(allowed_algorithms));
//
// // Get the public key and include it in some authenticator creation request.
// auto passkey = CreatePasskey(...,
//     unbound_key.Get()->GetPublicKeyAsCoseKey());
//
// // Finally bind the key given the passkey's credential identifier.
// binder.BindKey(
//     std::move(unbound_key), passkey.GetCredentialId(), relying_party_id);
//
//
// Example retrieval usage:
//
// auto passkey = ...; // After retrieving the passkey.
//
// binder_ = std::make_unique<PasskeyBrowserBinder>(
//     std::move(key_store), web_payments_web_data_service);
//
// binder_->GetOrCreateBoundKeyForPasskey(
//     passkey.GetCredentialId(),
//     passkey.GetRelyingPartyId(),
//     allowed_algorithms,
//     [](std::unique_ptr<BrowserBoundKey> key) {
//       if (!key) {
//         // Handle the browser bound key could not be found nor created.
//       }
//       // Use the browser bound key to sign something.
//       key->Sign(client_data_json);
//     });
//
// This class depends on an implementation of BrowserBoundKeyStore and the
// payment manifest web data service for storing the BBK and passkey
// identifiers.
class PasskeyBrowserBinder {
 public:
  // `key_store` and `web_data_service` are required and must be set.
  PasskeyBrowserBinder(
      scoped_refptr<BrowserBoundKeyStore> key_store,
      scoped_refptr<WebPaymentsWebDataService> web_data_service);
  PasskeyBrowserBinder(const PasskeyBrowserBinder&) = delete;
  PasskeyBrowserBinder& operator=(const PasskeyBrowserBinder&) = delete;
  virtual ~PasskeyBrowserBinder();

  // Represents a browser bound key that has not yet been associated. If
  // BindKey() is not called when this class goes out of scope, the wrapped
  // BrowserBoundKey will be deleted.
  class UnboundKey {
   public:
    // Creates an UnboundKey. `browser_bound_key_id` will be used to delete the
    // key from `key_store` when UnboundKey goes out of scope without having
    // been bound. UnboundKey takes ownership of `browser_bound_key` which can
    // be accessed via Get() while the key has not yet been bound.
    UnboundKey(std::vector<uint8_t> browser_bound_key_id,
               std::unique_ptr<BrowserBoundKey> browser_bound_key,
               scoped_refptr<BrowserBoundKeyStore> key_store);
    UnboundKey(const UnboundKey&) = delete;
    UnboundKey& operator=(const UnboundKey&) = delete;
    UnboundKey(UnboundKey&&);
    UnboundKey& operator=(UnboundKey&&);
    ~UnboundKey();

    // Returns a reference to the underlying browser bound key, this is the only
    // way by which the browser bound key can be accessed before having been
    // associated.
    BrowserBoundKey& Get();

    // Returns the browser bound key identifier for tests.
    const std::vector<uint8_t>& GetBrowserBoundKeyIdForTesting() const {
      return browser_bound_key_id_;
    }

   private:
    friend PasskeyBrowserBinder;

    // Marks the BrowserBoundKey as bound. After calling, destruction of this
    // UnboundKey will not delete the browser bound key.
    //
    // Do not call other methods on UnboundKey after calling
    // MarkKeyBoundAndReset().
    void MarkKeyBoundAndReset();

    // The browser bound key id. This is passed to the key store if the BBK
    // needs to be deleted.
    std::vector<uint8_t> browser_bound_key_id_;

    // An owned reference to the browser bound key. This member may be `nullptr`
    // in some PasskeyBrowserBinder internal usages of UnboundKey.
    std::unique_ptr<BrowserBoundKey> browser_bound_key_;

    // A reference to the key store. This key store is invoked if the browser
    // bound key needs to be deleted.
    scoped_refptr<BrowserBoundKeyStore> key_store_;
  };

  // Creates a browser bound key that is not yet associated to a passkey. The
  // UnboundKey should be bound using BindKey() after the credential id has
  // been created.
  void CreateUnboundKey(
      const BrowserBoundKeyStore::CredentialInfoList& allowed_algorithms,
      base::OnceCallback<void(std::optional<UnboundKey>)> callback);

  // Gets a browser bound key for the given `credential_id` and `relying_party`
  // only if a browser bound key already exists for the credential.
  void GetBoundKeyForPasskey(
      std::vector<uint8_t> credential_id,
      std::string relying_party,
      base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback);

  // Gets or creates a browser bound key for the given `credential_id`,
  // `relying_party` and `allowed_algorithms` returning the browser bound key
  // by running `callback`. An optional `last_used` timestamp can be provided
  // to record when the browser bound key was created. If a key already
  // exists, `last_used` will be ignored.
  void GetOrCreateBoundKeyForPasskey(
      std::vector<uint8_t> credential_id,
      std::string relying_party,
      const BrowserBoundKeyStore::CredentialInfoList& allowed_algorithms,
      std::optional<base::Time> last_used,
      base::OnceCallback<void(bool is_new, std::unique_ptr<BrowserBoundKey>)>
          callback);

  // Stores the association of the `key` to a `credential_id` and
  // `relying_party`. The UnboundKey must be std::moved and is thus
  // intentionally no longer available to the caller. If the BrowserBoundKey is
  // needed thereafter, then retrieve it using BoundKeyForPasskey(). An optional
  // `last_used` timestamp can be provided to record when the browser bound key
  // was last used.
  void BindKey(UnboundKey key,
               const std::vector<uint8_t>& credential_id,
               const std::string& relying_party,
               std::optional<base::Time> last_used);

  // Updates the browser bound key's `last_used` timestamp to the current
  // system time.
  void UpdateKeyLastUsedToNow(const std::vector<uint8_t>& credential_id,
                              const std::string& relying_party);

  // Retrieves all browser bound keys from the web data service and runs
  // `callback` with the result.
  virtual void GetAllBrowserBoundKeys(
      base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)> callback);

  // Deletes the provided browser bound keys from the web data service.
  // `callback` is run once the database operation completes.
  virtual void DeleteBrowserBoundKeys(
      base::OnceClosure callback,
      std::vector<BrowserBoundKeyMetadata> bbk_metas);

  // Injects the random bytes function for testing.
  void SetRandomBytesAsVectorCallbackForTesting(
      base::RepeatingCallback<std::vector<uint8_t>(size_t length)>);

  BrowserBoundKeyStore* GetBrowserBoundKeyStoreForTesting();
  WebPaymentsWebDataService* GetWebDataServiceForTesting();

 private:
  // Called after retrieving the possibly empty `existing_browser_bound_key_id`
  // to retrieve the matching browser bound key.
  void GetBrowserBoundKey(
      base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback,
      std::vector<uint8_t> existing_browser_bound_key_id);

  // Called after getting the browser bound key from the store. Runs `callback`
  // with nullptr if there is no matching browser bound key.
  void OnGetBrowserBoundKey(
      base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback,
      std::unique_ptr<BrowserBoundKey> browser_bound_key);

  // Called after retrieving the possibly empty `existing_browser_bound_key_id`
  // to retrieve the matching browser bound key. Otherwise creates a new browser
  // bound key and saves its id.
  void GetOrCreateBrowserBoundKey(
      std::vector<uint8_t> credential_id,
      std::string relying_party,
      BrowserBoundKeyStore::CredentialInfoList allowed_algorithms,
      std::optional<base::Time> last_used,
      base::OnceCallback<void(bool is_new, std::unique_ptr<BrowserBoundKey>)>
          callback,
      std::vector<uint8_t> existing_browser_bound_key_id);

  // Called after getting or creating the browser bound key from the store. The
  // browser bound key is returned by running `callback` with a boolean
  // indicating whether the browser bound key is new.
  void OnGetOrCreateBrowserBoundKey(
      bool needs_to_be_created,
      std::vector<uint8_t> credential_id,
      std::string relying_party,
      std::optional<base::Time> last_used,
      base::OnceCallback<void(bool is_new, std::unique_ptr<BrowserBoundKey>)>
          callback,
      std::unique_ptr<BrowserBoundKey> browser_bound_key);

  // Called after creating an unbound browser bound key from the store. The
  // browser bound key is returned by running `callback`.
  void OnCreateUnboundKey(
      base::OnceCallback<void(std::optional<UnboundKey>)> callback,
      std::unique_ptr<BrowserBoundKey> browser_bound_key);

  // Records a creation or retrieval metric.
  void RecordCreationOrRetrieval(bool is_creation, bool did_succeed);

  scoped_refptr<BrowserBoundKeyStore> key_store_;
  scoped_refptr<WebPaymentsWebDataService> web_data_service_;
  base::RepeatingCallback<std::vector<uint8_t>(size_t)>
      random_bytes_as_vector_callback_;
  base::WeakPtrFactory<PasskeyBrowserBinder> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_PASSKEY_BROWSER_BINDER_H_
