// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_CLIENT_CALLBACK_WRAPPER_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_CLIENT_CALLBACK_WRAPPER_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"

namespace storage {

struct BucketLocator;

// Stopgap for QuotaClients in systems with an unclear ownership graph.
//
// Implements the QuotaClient interface by proxying to a "real" implementation.
// Proxying wraps mojo callbacks so that they are always called. In other words,
// if the "real" implementation drops the callback it receives (instead of
// Run()ning it) then the underlying mojo callback is called with default
// values.
//
// This is useful for mojofying QuotaClient implementations in systems with
// unclear object ownerships. More concretely, QuotaClients usually perform
// asynchronous work, which involves posting tasks across sequences. Tasks that
// use WeakPtrs may get dropped (which in turn drops the QuotaClient callbacks)
// if the WeakPtr objects get destroyed before the QuotaClient mojo connection.
//
// The situation above can be broken down into multiple subproblems, whose
// solutions can be landed separately.
// 1) Migrate the storage::QuotaClient implementation to a
//    storage::mojom::QuotaClient implementation and wrap it with a
//    storage::QuotaClientCallbackWrapper. Fix the issues introduced by
//    mojofication.
// 2) Simplify the system's threading model, taking advantage of the fact that
//    storage::mojom::QuotaClient implementations are not restricted to the
//    browser process' IO thread.
// 3) Remove the storage::QuotaClientCallbackWrapper. Fix shutdown issues,
//    potentially by further simplifying the system's threading model.
class COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC) QuotaClientCallbackWrapper
    : public mojom::QuotaClient {
 public:
  // `wrapped_client` must outlive this instance.
  explicit QuotaClientCallbackWrapper(mojom::QuotaClient* wrapped_client);

  QuotaClientCallbackWrapper(const QuotaClientCallbackWrapper&) = delete;
  QuotaClientCallbackWrapper& operator=(const QuotaClientCallbackWrapper&) =
      delete;

  ~QuotaClientCallbackWrapper() override;

  // mojom::QuotaClient.
  void GetBucketUsage(const BucketLocator& bucket,
                      GetBucketUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void DeleteBucketData(const BucketLocator& bucket,
                        DeleteBucketDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<mojom::QuotaClient> wrapped_client_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_QUOTA_CLIENT_CALLBACK_WRAPPER_H_
