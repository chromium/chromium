// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_RESOURCES_MEMORY_RESOURCE_IMPL_H_
#define COMPONENTS_REPORTING_RESOURCES_MEMORY_RESOURCE_IMPL_H_

#include <atomic>
#include <cstdint>

#include "components/reporting/resources/resource_interface.h"

namespace reporting {

// Interface to resources management by Storage module.
// Must be implemented by the caller base on the platform limitations.
// All APIs are non-blocking.
class MemoryResourceImpl : public ResourceInterface {
 public:
  explicit MemoryResourceImpl(uint64_t total_size);

  // Implementation of ResourceInterface methods.
  bool Reserve(uint64_t size) override;
  void Discard(uint64_t size) override;
  uint64_t GetTotal() const override;
  uint64_t GetUsed() const override;
  void Test_SetTotal(uint64_t test_total) override;

 private:
  ~MemoryResourceImpl() override;

  uint64_t total_;
  std::atomic<uint64_t> used_{0};
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_RESOURCES_MEMORY_RESOURCE_IMPL_H_
