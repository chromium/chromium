// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A RendererDnsPrefetch instance is maintained for each RenderThread.
// URL strings are typically added to the embedded queue during rendering.
// The first addition to the queue (transitioning from empty to having
// some names) causes a processing task to be added to the Renderer Thread.
// The processing task gathers all buffered names, and send them via IPC
// to the browser, so that DNS lookups can be performed before the user attempts
// to traverse a link.
// This class removed some duplicates, and discards numeric IP addresss
// (which wouldn't looked up in DNS anyway).
// To limit the time during the processing task (and avoid stalling the Render
// thread), several limits are placed on how much of the queue to process.
// If the processing task is not able to completely empty the queue, it
// schedules a future continuation of the task, and keeps the map of already
// sent names.  If the entire queue is processed, then the list of "sent names"
// is cleared so that future gatherings might again pass along the same names.

#ifndef COMPONENTS_NETWORK_HINTS_RENDERER_RENDERER_DNS_PREFETCH_H_
#define COMPONENTS_NETWORK_HINTS_RENDERER_RENDERER_DNS_PREFETCH_H_

#include <stddef.h>

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/network_hints/renderer/dns_prefetch_queue.h"

namespace network_hints {

// An internal interface to the network_hints component for efficiently sending
// DNS prefetch requests to the net stack.
class RendererDnsPrefetch {
 public:
  typedef base::RepeatingCallback<void(const std::vector<std::string>&)>
      BatchHandler;

  // The specified |batch_handler| will be notified periodically and
  // asynchronously on the same sequence when there are hostnames that should
  // be resolved. Individual |Resolve| calls are batched up to help minimize
  // IPC to the process where DNS requests are made. The |batch_handler| is
  // expected to outlive instances of this class. It is safe to delete
  // instances of this class when |batch_handler| is called.
  explicit RendererDnsPrefetch(BatchHandler batch_handler);
  ~RendererDnsPrefetch();

  // Push a name into the queue to be resolved.
  void Resolve(const char* name, size_t length);

  // SubmitHosts processes the buffered names, and submits them for DNS
  // prefetching.
  // Note that browser process may decide which names should be looked up (to
  // pre-warm the cache) based on what has been (or not been) looked up
  // recently.
  // If sending for DNS lookup is incomplete (queue is not empty, or not all
  // names in map are sent, or ...) then a task to continue processing is
  // sent to our thread loop.
  void SubmitHostnames();

  // The following is private, but exposed for testing purposes only.
  static bool is_numeric_ip(const char* name, size_t length);

 private:
  // ExtractBufferedNames pulls names from queue into the map, reducing or
  // eliminating a waiting queue.
  // The size_goal argument can be used to reduce the amount of
  // processing done in this method, and can leave some data
  // in the buffer under some circumstances.
  // If size_goal is zero, then extraction proceeds until
  // the queue is empty.  If size goal is positive, then
  // extraction continues until the domain_map_ contains
  // at least the specified number of names, or the buffer is empty.
  void ExtractBufferedNames(size_t size_goal = 0);

  // GetNamesToPrefetch does not check the buffer, and just takes names that
  // are already collected in the domain_map_ for DNS lookup.  If max_count is
  // zero, then all available names are appended to |names|; and if positive,
  // then at most max_count names will be appended.
  void GetNamesToPrefetch(size_t max_count, std::vector<std::string>* names);

  // Reset() restores initial state provided after construction.
  // This discards ALL queue entries, and map entries.
  void Reset();

  SEQUENCE_CHECKER(sequence_checker_);

  // This callback is run periodically to send a set of recommended hostnames
  // to resolve.
  BatchHandler batch_handler_;

  // We use c_string_queue_ to hold lists of names supplied typically) by the
  // renderer.  It queues the names, at minimal cost to the renderer's thread,
  // and allows this class to process them when time permits (in a later task).
  DnsQueue c_string_queue_;

  // domain_map_ contains (for each domain) one of the next two constants,
  // depending on whether we have asked the browser process to do the actual
  // DNS lookup.
  static const int kLookupRequested = 0x1;
  static const int kPending = 0x0;
  typedef std::map<std::string, int> DomainUseMap;
  DomainUseMap domain_map_;

  // Cache a tally of the count of names that haven't yet been sent
  // for DNS pre-fetching.  Note that we *could* recalculate this
  // count by iterating over domain_map_, looking for even values.
  size_t new_name_count_;

  // We have some metrics to examine performance.  We might use
  // these metrics to modify buffer counts etc. some day.
  int buffer_full_discard_count_;
  int numeric_ip_discard_count_;

  base::WeakPtrFactory<RendererDnsPrefetch> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RendererDnsPrefetch);
};  // class RendererDnsPrefetch

}  // namespace network_hints

#endif  // COMPONENTS_NETWORK_HINTS_RENDERER_RENDERER_DNS_PREFETCH_H_
