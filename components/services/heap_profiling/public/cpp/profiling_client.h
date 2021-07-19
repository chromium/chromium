// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_PROFILING_CLIENT_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_PROFILING_CLIENT_H_

#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace heap_profiling {

// The Client listens on the interface for a StartProfiling message. On
// receiving the message, it begins profiling the current process.
//
// The owner of this object is responsible for binding it to the BinderRegistry.
class ProfilingClient : public mojom::ProfilingClient {
 public:
  ProfilingClient();

  // mojom::ProfilingClient overrides:
  void StartProfiling(mojom::ProfilingParamsPtr params,
                      StartProfilingCallback callback) override;
  void RetrieveHeapProfile(RetrieveHeapProfileCallback callback) override;

  void BindToInterface(mojo::PendingReceiver<mojom::ProfilingClient> receiver);

 private:
  ~ProfilingClient() override;

  void StartProfilingInternal(mojom::ProfilingParamsPtr params,
                              StartProfilingCallback callback);

  // Ideally, this would be a mojo::Receiver that would only keep alive one
  // client receiver. However, the service that makes the client requests
  // [content_browser] is different from the service that dedupes the client
  // requests [profiling service]. This means that there may be a brief
  // intervals where there are two active bindings, until the profiling service
  // has a chance to figure out which one to keep.
  mojo::ReceiverSet<mojom::ProfilingClient> receivers_;

  bool started_profiling_{false};
};

// Initializes the TLS slot globally. This will be called early in Chrome's
// lifecycle to prevent re-entrancy from occurring while trying to set up the
// TLS slot, which is the entity that's supposed to prevent re-entrancy.
void InitTLSSlot();

// Exists for testing only.
// A return value of |true| means that the allocator shim was already
// initialized and |callback| will never be called. Otherwise, |callback| will
// be called on |task_runner| after the allocator shim is initialized.
bool SetOnInitAllocatorShimCallbackForTesting(
    base::OnceClosure callback,
    scoped_refptr<base::TaskRunner> task_runner);

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_PROFILING_CLIENT_H_
