// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEST_AI_MODE_BUTTON_SERVICE_H_
#define COMPONENTS_SEARCH_ENGINES_TEST_AI_MODE_BUTTON_SERVICE_H_

#include "components/search_engines/ai_mode_button_service.h"

class TestAiModeButtonService : public AiModeButtonService {
 public:
  using AiModeButtonService::AiModeButtonService;
  ~TestAiModeButtonService() override = default;

  using AiModeButtonService::IsValidConfig;

  using AiModeButtonService::current_ui_config_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEST_AI_MODE_BUTTON_SERVICE_H_
