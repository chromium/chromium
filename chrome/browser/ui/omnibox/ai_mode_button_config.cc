// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/ai_mode_button_config.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/grit/branded_strings.h"
#include "components/search_engines/search_engine_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ai_mode_button_config {

namespace {
const AiModeButtonConfig* g_test_config = nullptr;
}  // namespace

const AiModeButtonConfig& GetCurrentAiModeButtonConfig() {
  if (g_test_config) {
    return *g_test_config;
  }
  static const base::NoDestructor<AiModeButtonConfig>
      kDefaultAiModeButtonConfig{
          {SearchEngineType::SEARCH_ENGINE_GOOGLE,
           l10n_util::GetStringUTF16(IDS_AI_MODE_ENTRYPOINT_LABEL),
           l10n_util::GetStringUTF16(
               IDS_STARTER_PACK_AI_MODE_ACTION_SUGGESTION_CONTENTS),
           "", "", "",
           l10n_util::GetStringUTF16(IDS_ACC_AI_MODE_BUTTON_FOCUSED),
#if BUILDFLAG(IS_ANDROID)
           u"",  // Android does not have an omnibox context menu.
#else
           l10n_util::GetStringUTF16(
               IDS_CONTEXT_MENU_SHOW_AI_MODE_OMNIBOX_BUTTON),
#endif
           l10n_util::GetStringUTF16(IDS_ACC_AI_MODE_PLACEHOLDER_TEXT)}};
  return *kDefaultAiModeButtonConfig;
}

void SetCurrentAiModeButtonConfigForTesting(const AiModeButtonConfig* config) {
  g_test_config = config;
}

}  // namespace ai_mode_button_config
