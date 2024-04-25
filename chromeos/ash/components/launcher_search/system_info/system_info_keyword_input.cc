// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/launcher_search/system_info/system_info_keyword_input.h"

namespace launcher_search {

SystemInfoKeywordInput::SystemInfoKeywordInput(SystemInfoInputType input_type,
                                               std::u16string keyword)
    : input_type_(input_type), keyword_(keyword) {}

SystemInfoInputType SystemInfoKeywordInput::GetInputType() {
  return input_type_;
}
std::u16string SystemInfoKeywordInput::GetKeyword() {
  return keyword_;
}

}  // namespace launcher_search
