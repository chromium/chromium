// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"

#include <string_view>

namespace chromeos::editor_menu {

PresetTextQuery::PresetTextQuery(std::string_view text_query_id,
                                 std::u16string_view name,
                                 PresetQueryCategory category)
    : text_query_id(text_query_id), name(name), category(category) {}

}  // namespace chromeos::editor_menu
