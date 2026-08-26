The heap profiling service provides:
  * Storage for recording metadata about heap allocations and deallocations,
  for any number of clients.
  * A lightweight, synchronous API for clients to send metadata about heap
  allocations and deallocations.

There are two main entities:
  * Heap Profiling Clients send heap allocation data to the Heap Profiling
    Service. Clients require no special privileges. There should be at most 1
    Client per process, since heap hooks are instantiated on a per-process
    basis.
  * The Heap Profiling Service runs in an unprivileged, sandboxed process. It's
    sole purpose is to record allocations and serve them in heap dumps.

Heap dumps are emitted in traces with the "memory-infra" category enabled. This
is coordinated by the (memory_instrumentation
service)[https://cs.chromium.org/chromium/src/services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom].
See the HeapProfiler and HeapProfilerHelper interfaces.
