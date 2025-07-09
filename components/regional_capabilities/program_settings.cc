// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/program_settings.h"

namespace regional_capabilities {

const ProgramSettings kWaffleSettings{
    .search_engine_list_type = SearchEngineListType::kShuffled,
};

const ProgramSettings kDefaultSettings{
    .search_engine_list_type = SearchEngineListType::kTopFive,
};

}  // namespace regional_capabilities
