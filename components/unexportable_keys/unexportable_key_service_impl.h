// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_

#include <algorithm>
#include <map>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

namespace {
class MaybePendingUnexportableKeyId;
}

class UnexportableKeyTaskManager;

class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) UnexportableKeyServiceImpl
    : public UnexportableKeyService {
 public:
  // `task_manager` must outlive `UnexportableKeyServiceImpl`.
  explicit UnexportableKeyServiceImpl(UnexportableKeyTaskManager& task_manager);

  ~UnexportableKeyServiceImpl() override;

  // Returns whether the current platform has a support for unexportable signing
  // keys. If this returns false, all service methods will return
  // `ServiceError::kNoKeyProvider`.
  static bool IsUnexportableKeyProviderSupported(
      crypto::UnexportableKeyProvider::Config config);

  // UnexportableKeyService:
  void GenerateSigningKeySlowlyAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback)
      override;
  void FromWrappedSigningKeySlowlyAsync(
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback)
      override;
  void SignSlowlyAsync(
      const UnexportableKeyId& key_id,
      base::span<const uint8_t> data,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback)
      override;
  ServiceErrorOr<std::vector<uint8_t>> GetSubjectPublicKeyInfo(
      UnexportableKeyId key_id) const override;
  ServiceErrorOr<std::vector<uint8_t>> GetWrappedKey(
      UnexportableKeyId key_id) const override;
  ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm> GetAlgorithm(
      UnexportableKeyId key_id) const override;

 private:
  // Comparator object that allows comparing containers of different types that
  // are convertible to base::span<const uint8_t>.
  struct WrappedKeyCmp {
    using is_transparent = void;
    bool operator()(base::span<const uint8_t> lhs,
                    base::span<const uint8_t> rhs) const {
      return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(),
                                          rhs.end());
    }
  };

  using WrappedKeyMap = std::
      map<std::vector<uint8_t>, MaybePendingUnexportableKeyId, WrappedKeyCmp>;
  using KeyIdMap = std::map<UnexportableKeyId,
                            scoped_refptr<RefCountedUnexportableSigningKey>>;

  // Callback for `GenerateSigningKeySlowlyAsync()`.
  void OnKeyGenerated(
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>
          client_callback,
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
          key_or_error);

  // Callback for `FromWrappedSigningKeySlowlyAsync()`.
  void OnKeyCreatedFromWrappedKey(
      WrappedKeyMap::iterator pending_entry_it,
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
          key_or_error);

  const raw_ref<UnexportableKeyTaskManager, DanglingUntriaged> task_manager_;

  // Helps mapping multiple `FromWrappedSigningKeySlowlyAsync()` requests with
  // the same wrapped key into the same key ID.
  WrappedKeyMap key_id_by_wrapped_key_;

  // Stores unexportable signing keys that were created during the current
  // session.
  KeyIdMap key_by_key_id_;

  base::WeakPtrFactory<UnexportableKeyServiceImpl> weak_ptr_factory_{this};
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_
