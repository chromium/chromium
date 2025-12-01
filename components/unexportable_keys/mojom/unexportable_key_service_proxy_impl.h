// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEY_SERVICE_PROXY_IMPL_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEY_SERVICE_PROXY_IMPL_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace unexportable_keys {

inline constexpr size_t kMaxWrappedKeySize =
    3 * std::numeric_limits<uint16_t>::max();

// An implementation of the `mojom::UnexportableKeyService` Mojo interface.
//
// This class receives IPC requests via its Mojo receiver and delegates calls
// to an underlying `::unexportable_keys::UnexportableKeyService` instance.
// The underlying service must outlive this proxy.
class UnexportableKeyServiceProxyImpl : public mojom::UnexportableKeyService {
 public:
  // Constructs a proxy. `unexportable_key_service` must be a non-null pointer
  // to an UnexportableKeyService that outlives this
  // UnexportableKeyServiceProxyImpl.
  explicit UnexportableKeyServiceProxyImpl(
      unexportable_keys::UnexportableKeyService* unexportable_key_service,
      mojo::PendingReceiver<mojom::UnexportableKeyService> receiver);
  UnexportableKeyServiceProxyImpl(const UnexportableKeyServiceProxyImpl&) =
      delete;
  UnexportableKeyServiceProxyImpl& operator=(
      const UnexportableKeyServiceProxyImpl&) = delete;
  ~UnexportableKeyServiceProxyImpl() override;

  void GenerateSigningKey(
      const std::vector<crypto::SignatureVerifier::SignatureAlgorithm>&
          acceptable_algorithms,
      BackgroundTaskPriority priority,
      GenerateSigningKeyCallback callback) override;

  void FromWrappedSigningKey(const std::vector<uint8_t>& wrapped_key,
                             BackgroundTaskPriority priority,
                             FromWrappedSigningKeyCallback callback) override;

  void Sign(const UnexportableKeyId& token,
            const std::vector<uint8_t>& data,
            BackgroundTaskPriority priority,
            SignCallback callback) override;

  void GetAllSigningKeysForGarbageCollection(
      BackgroundTaskPriority priorty,
      GetAllSigningKeysForGarbageCollectionCallback callback) override;

  void DeleteKey(const UnexportableKeyId& key_id,
                 BackgroundTaskPriority priority,
                 DeleteKeyCallback result) override;

  void DeleteAllKeys(BackgroundTaskPriority priority,
                     DeleteAllKeysCallback result) override;

 private:
  mojo::Receiver<mojom::UnexportableKeyService> receiver_{this};
  // The underlying UnexportableKeyService instance. Not owned.
  // This pointer must remain valid for the entire lifetime of the
  // UnexportableKeyServiceProxyImpl object.
  const raw_ref<unexportable_keys::UnexportableKeyService>
      unexportable_key_service_;
};

}  // namespace unexportable_keys
#endif  // COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEY_SERVICE_PROXY_IMPL_H_
