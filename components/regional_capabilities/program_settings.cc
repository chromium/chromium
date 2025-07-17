// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/program_settings.h"

namespace regional_capabilities {

const ProgramSettings kWaffleSettings{
    .search_engine_list_type = SearchEngineListType::kShuffled,
    .can_show_search_engine_choice_screen = true,
};

const ProgramSettings kTaiyakiSettings{
    .search_engine_list_type = SearchEngineListType::kShuffled,
    .can_show_search_engine_choice_screen = true,
};

const ProgramSettings kDefaultSettings{
    .search_engine_list_type = SearchEngineListType::kTopFive,
    .can_show_search_engine_choice_screen = false,
};

}  // namespace regional_capabilities
