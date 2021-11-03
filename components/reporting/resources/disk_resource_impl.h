// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_RESOURCES_DISK_RESOURCE_IMPL_H_
#define COMPONENTS_REPORTING_RESOURCES_DISK_RESOURCE_IMPL_H_

#include <atomic>
#include <cstdint>

#include "components/reporting/resources/resource_interface.h"

namespace reporting {

// Interface to resources management by Storage module.
// Must be implemented by the caller base on the platform limitations.
// All APIs are non-blocking.
class DiskResourceImpl : public ResourceInterface {
 public:
  DiskResourceImpl();
  ~DiskResourceImpl() override;

  // Implementation of ResourceInterface methods.
  bool Reserve(uint64_t size) override;
  void Discard(uint64_t size) override;
  uint64_t GetTotal() override;
  uint64_t GetUsed() override;
  void Test_SetTotal(uint64_t test_total) override;

 private:
  uint64_t total_;
  std::atomic<uint64_t> used_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_RESOURCES_DISK_RESOURCE_IMPL_H_
