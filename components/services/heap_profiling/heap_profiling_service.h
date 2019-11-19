// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_HEAP_PROFILING_SERVICE_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_HEAP_PROFILING_SERVICE_H_

#include "components/services/heap_profiling/public/mojom/heap_profiling_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace heap_profiling {

// Returns a PendingRemote which can be bound to control a new ProfilingService
// instance running on a dedicated thread. The service will run until this
// remote is disconnected.
//
// |profiler_receiver| must be valid and will receive heap profiling requests.
// |helper| must be valid and will be used by the service to retrieve memory
// maps.
mojo::PendingRemote<mojom::ProfilingService> LaunchService(
    mojo::PendingReceiver<memory_instrumentation::mojom::HeapProfiler>
        profiler_receiver,
    mojo::PendingRemote<memory_instrumentation::mojom::HeapProfilerHelper>
        helper);

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_HEAP_PROFILING_SERVICE_H_
