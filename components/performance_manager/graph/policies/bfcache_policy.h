// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_POLICIES_BFCACHE_POLICY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_POLICIES_BFCACHE_POLICY_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace performance_manager::policies {

// Policies that automatically flush the BFCache of pages when the system is
// under memory pressure.
class BFCachePolicy : public GraphOwned, public base::MemoryPressureListener {
 public:
  BFCachePolicy();
  BFCachePolicy(const BFCachePolicy&) = delete;
  BFCachePolicy(BFCachePolicy&&) = delete;
  BFCachePolicy& operator=(const BFCachePolicy&) = delete;
  BFCachePolicy& operator=(BFCachePolicy&&) = delete;
  ~BFCachePolicy() override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 protected:
  // Try to flush the BFCache associated with |page_node|. This will be a no-op
  // if there's a pending navigation.
  virtual void MaybeFlushBFCache(
      const PageNode* page_node,
      base::MemoryPressureLevel memory_pressure_level);

 private:
  void OnMemoryPressure(base::MemoryPressureLevel new_level) override;

  base::MemoryPressureListenerRegistration
      memory_pressure_listener_registration_;
};

}  // namespace performance_manager::policies

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_POLICIES_BFCACHE_POLICY_H_
