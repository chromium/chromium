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
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
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
//     std::move(key_store), payment_manifest_web_data_service);
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
//     std::move(key_store), payment_manifest_web_data_service);
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
class PasskeyBrowserBinder : public WebDataServiceConsumer {
 public:
  PasskeyBrowserBinder(
      scoped_refptr<BrowserBoundKeyStore> key_store,
      scoped_refptr<PaymentManifestWebDataService> web_data_service);
  PasskeyBrowserBinder(const PasskeyBrowserBinder&) = delete;
  PasskeyBrowserBinder& operator=(const PasskeyBrowserBinder&) = delete;
  ~PasskeyBrowserBinder() override;

  // Represents a browser bound key that has not yet been associated. If
  // BindKey() is not called when this class goes out of scope, the wrapped
  // BrowserBoundKey will be deleted.
  class UnboundKey {
   public:
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
    BrowserBoundKey& Get() { return *browser_bound_key_; }

   private:
    friend PasskeyBrowserBinder;

    // Marks the BrowserBoundKey as bound. After calling, destruction of this
    // UnboundKey will not delete the browser bound key.
    //
    // Do not call other methods on UnboundKey after calling
    // MarkKeyBoundAndReset().
    void MarkKeyBoundAndReset();

    std::vector<uint8_t> browser_bound_key_id_;
    std::unique_ptr<BrowserBoundKey> browser_bound_key_;
    scoped_refptr<BrowserBoundKeyStore> key_store_;
  };

  // Creates a browser bound key that is not yet associated to a passkey. The
  // UnboundKey should be bound using BindKey() after the credential id has
  // been created.
  std::optional<UnboundKey> CreateUnboundKey(
      const BrowserBoundKeyStore::CredentialInfoList& allowed_algorithms);

  // Gets a browser bound key for the given `credential_id` and `relying_party`
  // only if a browser bound key already exists for the credential.
  void GetBoundKeyForPasskey(
      std::vector<uint8_t> credential_id,
      std::string relying_party,
      base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback);

  // Gets or creates a browser bound key for the given `credential_id`,
  // `relying_party` and `allowed_algorithms` returning the browser bound key
  // by running `callback`.
  void GetOrCreateBoundKeyForPasskey(
      std::vector<uint8_t> credential_id,
      std::string relying_party,
      const BrowserBoundKeyStore::CredentialInfoList& allowed_algorithms,
      base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback);

  // Stores the association of the `key` to a `credential_id` and
  // `relying_party`. The UnboundKey must be std::moved and is thus
  // intentionally no longer available to the caller. If the BrowserBoundKey is
  // needed thereafter, then retrieve it using BoundKeyForPasskey().
  void BindKey(UnboundKey key,
               const std::vector<uint8_t>& credential_id,
               const std::string& relying_party);

  // Deletes all unknown browser bound keys, querying using the provided
  // `get_matching_credential_ids_callback` to find credentials matching each
  // relying party in the BBK storage. The
  // `get_matching_credential_ids_callback` must be valid until `callback` is
  // invoked. `callback` may hold and release the reference to this
  // PasskeyBrowserBinder object (DeleteAllUnknownBrowserBoundKeys will run
  // `callback` as its last action).
  void DeleteAllUnknownBrowserBoundKeys(
      base::RepeatingCallback<
          void(const std::string& relying_party_id,
               const std::vector<std::vector<uint8_t>>& credential_ids,
               bool require_third_party_payment_bit_set,
               base::OnceCallback<void(std::vector<std::vector<uint8_t>>)>)>
          get_matching_credential_ids_callback,
      base::OnceClosure callback);

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // Injects the random bytes function for testing.
  void SetRandomBytesAsVectorCallbackForTesting(
      base::RepeatingCallback<std::vector<uint8_t>(size_t length)>);

  BrowserBoundKeyStore* GetBrowserBoundKeyStoreForTesting();
  PaymentManifestWebDataService* GetWebDataServiceForTesting();

 private:
  // Called after retrieving the possibly empty `existing_browser_bound_key_id`
  // to retrieve the matching browser bound key. Runs `callback` with nullptr if
  // there is no matching browser bound key.
  void GetBrowserBoundKey(
      base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback,
      std::vector<uint8_t> existing_browser_bound_key_id);

  // Called after retrieving the possibly empty `existing_browser_bound_key_id`
  // to retrieve the matching browser bound key. Otherwise creates a new browser
  // bound key and saves its id. The browser bound key is returned by running
  // `callback`.
  void GetOrCreateBrowserBoundKey(
      std::vector<uint8_t> credential_id,
      std::string relying_party,
      BrowserBoundKeyStore::CredentialInfoList allowed_algorithms,
      base::OnceCallback<void(std::unique_ptr<BrowserBoundKey>)> callback,
      std::vector<uint8_t> existing_browser_bound_key_id);

  // Called after internal authenticator was called to find stale BBKs.
  // `callback` is Run once the database operation completes.
  void DeleteBrowserBoundKeys(
      base::OnceClosure callback,
      std::vector<BrowserBoundKeyMetadata> stale_bbk_metas);

  scoped_refptr<BrowserBoundKeyStore> key_store_;
  scoped_refptr<PaymentManifestWebDataService> web_data_service_;
  std::map<WebDataServiceBase::Handle, base::OnceCallback<void(bool)>>
      set_browser_bound_key_handlers_;
  std::map<WebDataServiceBase::Handle,
           base::OnceCallback<void(std::vector<uint8_t>)>>
      get_browser_bound_key_handlers_;
  base::RepeatingCallback<std::vector<uint8_t>(size_t)>
      random_bytes_as_vector_callback_;
  base::WeakPtrFactory<PasskeyBrowserBinder> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_PASSKEY_BROWSER_BINDER_H_
