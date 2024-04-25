// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LAUNCHER_SEARCH_SYSTEM_INFO_SYSTEM_INFO_KEYWORD_INPUT_H_
#define CHROMEOS_ASH_COMPONENTS_LAUNCHER_SEARCH_SYSTEM_INFO_SYSTEM_INFO_KEYWORD_INPUT_H_

#include <string>

#include "base/component_export.h"

namespace launcher_search {

// This enum represents which type of System Info will be displayed.
enum class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LAUNCHER_SEARCH)
    SystemInfoInputType {
      kCPU,
      kVersion,
      kMemory,
      kBattery,
      kStorage
    };

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LAUNCHER_SEARCH)
    SystemInfoKeywordInput {
  SystemInfoKeywordInput() = default;
  SystemInfoKeywordInput(SystemInfoInputType input_type,
                         std::u16string keyword);

  ~SystemInfoKeywordInput() = default;

  SystemInfoInputType GetInputType();
  std::u16string GetKeyword();

 private:
  SystemInfoInputType input_type_;
  std::u16string keyword_;
};

}  // namespace launcher_search

#endif  // CHROMEOS_ASH_COMPONENTS_LAUNCHER_SEARCH_SYSTEM_INFO_SYSTEM_INFO_KEYWORD_INPUT_H_
