// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_MEMORY_DATA_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_MEMORY_DATA_H_

#include <string>

#include "base/component_export.h"

namespace system_info {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO) MemoryData {
 public:
  MemoryData() = default;

  ~MemoryData() = default;

  void SetAvailableMemory(std::u16string available_memory_gb) {
    available_memory_gb_ = available_memory_gb;
  }
  void SetTotalMemory(std::u16string total_memory_gb) {
    total_memory_gb_ = total_memory_gb;
  }

  std::u16string GetAvailableMemory() const { return available_memory_gb_; }
  std::u16string GetTotalMemory() const { return total_memory_gb_; }

 private:
  std::u16string available_memory_gb_ = u"";
  std::u16string total_memory_gb_ = u"";
};

}  // namespace system_info

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_MEMORY_DATA_H_
