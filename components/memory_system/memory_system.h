// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_SYSTEM_MEMORY_SYSTEM_H_
#define COMPONENTS_MEMORY_SYSTEM_MEMORY_SYSTEM_H_

#include <memory>
#include <optional>

namespace memory_system {

struct DispatcherParameters;
struct GwpAsanParameters;
struct ProfilingClientParameters;

// The memory system. The memory system represents multiple memory components,
// i.e. GWP-ASan or Profiling Client, and takes care of proper initialization.
class MemorySystem {
 public:
  MemorySystem();
  ~MemorySystem();

  // Initialize the memory system with the given parameters. If an empty
  // optional is given for a component, it will not be initialized.
  void Initialize(
      const std::optional<GwpAsanParameters>& gwp_asan_parameters,
      const std::optional<ProfilingClientParameters>&
          profiling_client_parameters,
      const std::optional<DispatcherParameters>& dispatcher_parameters);

 private:
  struct Impl;
  std::unique_ptr<Impl> const impl_;
};

}  // namespace memory_system
#endif  // COMPONENTS_MEMORY_SYSTEM_MEMORY_SYSTEM_H_