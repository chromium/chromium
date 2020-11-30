// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_LIFETIME_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_LIFETIME_H_

#include <memory>

#include "base/callback_forward.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/public/performance_manager.h"

namespace performance_manager {

class Graph;

using GraphCreatedCallback = base::OnceCallback<void(Graph*)>;

enum class Decorators {
  // Installs no additional graph features. Mostly intended for unittests,
  // where tests pick and choose exactly which decorators they want.
  kNone,
  // Installs the minimal set of graph features required for content_shell to
  // work.
  kMinimal,
  // Installs the default set of graph features shipped with a full-featured
  // Chromium browser.
  kDefault
};

// A helper class that manages the lifetime of PerformanceManager
// and PerformanceManagerRegistry.
class PerformanceManagerLifetime {
 public:
  PerformanceManagerLifetime(Decorators, GraphCreatedCallback);
  ~PerformanceManagerLifetime();

  // Allows specifying an additional callback that will be invoked in tests.
  static void SetAdditionalGraphCreatedCallbackForTesting(
      GraphCreatedCallback graph_created_callback);

  // Allows setting an override for the initialization mode that will be
  // used in tests. This needs to be set before PerformanceManagerLifetime is
  // created. In browser tests this occurs as part of Chrome browser main parts.
  static void SetDecoratorsOverrideForTesting(
      base::Optional<Decorators> decorators_override);

 private:
  std::unique_ptr<PerformanceManager> performance_manager_;
  std::unique_ptr<PerformanceManagerRegistry> performance_manager_registry_;
};

// Creates a PerformanceManager with default decorators.
// |graph_created_callback| is invoked on the PM sequence once the Graph is
// created.
std::unique_ptr<PerformanceManager>
CreatePerformanceManagerWithDefaultDecorators(
    GraphCreatedCallback graph_created_callback);

// Unregisters |instance| and arranges for its deletion on its sequence.
void DestroyPerformanceManager(std::unique_ptr<PerformanceManager> instance);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_LIFETIME_H_
