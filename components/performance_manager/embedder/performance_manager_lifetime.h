// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_LIFETIME_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_LIFETIME_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/public/performance_manager.h"

namespace performance_manager {

class Graph;
class GraphFeatures;

using GraphCreatedCallback = base::OnceCallback<void(Graph*)>;

// A helper class that manages the lifetime of PerformanceManager
// and PerformanceManagerRegistry.
class PerformanceManagerLifetime {
 public:
  PerformanceManagerLifetime(const GraphFeatures&, GraphCreatedCallback);
  ~PerformanceManagerLifetime();

  // Allows specifying an additional callback that will be invoked in tests.
  static void SetAdditionalGraphCreatedCallbackForTesting(
      GraphCreatedCallback graph_created_callback);

  // Sets an override for the features enabled in testing. These will be used
  // instead of the features passed to the PerformanceManagerLifetime
  // constructor in tests. Individual tests can enable more features by
  // creating another GraphFeatures object and calling its ConfigureGraph
  // method.
  //
  // This needs to be set before any PerformanceManagerLifetime is created. In
  // browser tests this occurs as part of Chrome browser main parts.
  static void SetGraphFeaturesOverrideForTesting(const GraphFeatures&);

 private:
  std::unique_ptr<PerformanceManager> performance_manager_;
  std::unique_ptr<PerformanceManagerRegistry> performance_manager_registry_;
};

// Unregisters |instance| and arranges for its deletion on its sequence.
void DestroyPerformanceManager(std::unique_ptr<PerformanceManager> instance);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_LIFETIME_H_
