// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/origin_trials_settings_manager.h"

namespace {
// TODO(https://crbug.com/1216609): Keep the limit the same as the existing CLI
// solution. When this bug is resolved, this limit will be increased.
const int kOriginMaxDisabledTrialTokens = 11;
}  // namespace

namespace embedder_support {
OriginTrialsSettingsManager::OriginTrialsSettingsManager() {
  settings_ = blink::mojom::OriginTrialsSettings();
}
OriginTrialsSettingsManager::~OriginTrialsSettingsManager() = default;

void OriginTrialsSettingsManager::PopulateFromOriginTrialsConfig(
    const base::Value::List& disabled_tokens_list) {
  SetDisabledTokens(std::move(disabled_tokens_list));
}
void OriginTrialsSettingsManager::SetDisabledTokens(
    const base::Value::List& disabled_tokens_list) {
  // TODO: Do not silently ignore the config if it exceeds the limit. For more
  // information: https://crrev.com/c/4374245/comment/6d15e085_b16be69c
  if (disabled_tokens_list.size() > kOriginMaxDisabledTrialTokens) {
    return;
  }

  std::vector<std::string> disabled_tokens;
  disabled_tokens.reserve(disabled_tokens_list.size());
  for (const auto& item : disabled_tokens_list) {
    if (item.is_string()) {
      // TODO(crbug.com/1431177): Investigate storing the decoded strings.
      disabled_tokens.push_back(item.GetString());
    }
  }
  settings_.disabled_tokens = std::move(disabled_tokens);
}
blink::mojom::OriginTrialsSettingsPtr OriginTrialsSettingsManager::GetSettings()
    const {
  return settings_.Clone();
}
}  // namespace embedder_support
