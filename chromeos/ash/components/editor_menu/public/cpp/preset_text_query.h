// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_PRESET_TEXT_QUERY_H_
#define CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_PRESET_TEXT_QUERY_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"

namespace chromeos::editor_menu {

inline constexpr std::string_view kLobsterPresetId = "LOBSTER";

enum class COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) PresetQueryCategory {
  kUnknown = 0,
  kShorten,
  kElaborate,
  kRephrase,
  kFormalize,
  kEmojify,
  kProofread,
  kLobster,
};

struct COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) PresetTextQuery {
  PresetTextQuery(std::string_view text_query_id,
                  std::u16string_view name,
                  PresetQueryCategory category);

  bool operator==(const PresetTextQuery&) const = default;

  std::string text_query_id;
  std::u16string name;
  PresetQueryCategory category;
};

using PresetTextQueries = std::vector<PresetTextQuery>;

}  // namespace chromeos::editor_menu

#endif  // CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_PRESET_TEXT_QUERY_H_
