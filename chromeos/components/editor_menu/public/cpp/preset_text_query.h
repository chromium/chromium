// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_PRESET_TEXT_QUERY_H_
#define CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_PRESET_TEXT_QUERY_H_

#include <string>
#include <vector>

#include "base/component_export.h"

namespace chromeos::editor_menu {

enum class COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) PresetQueryCategory {
  kUnknown = 0,
  kShorten,
  kElaborate,
  kRephrase,
  kFormalize,
  kEmojify,
  kProofread,
};

struct COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) PresetTextQuery {
  PresetTextQuery(std::string_view text_query_id,
                  std::u16string_view name,
                  PresetQueryCategory category);

  std::string text_query_id;
  std::u16string name;
  PresetQueryCategory category;
};

using PresetTextQueries = std::vector<PresetTextQuery>;

}  // namespace chromeos::editor_menu

#endif  // CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_PRESET_TEXT_QUERY_H_
