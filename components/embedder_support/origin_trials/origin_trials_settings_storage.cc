// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"

namespace {
const int kMaxDisabledTokens = 1024;
}  // namespace

namespace embedder_support {
OriginTrialsSettingsStorage::OriginTrialsSettingsStorage() {
  settings_ = blink::mojom::OriginTrialsSettings();
}
OriginTrialsSettingsStorage::~OriginTrialsSettingsStorage() = default;

void OriginTrialsSettingsStorage::PopulateSettings(
    const base::Value::List& disabled_tokens_list) {
  SetDisabledTokens(std::move(disabled_tokens_list));
}
void OriginTrialsSettingsStorage::SetDisabledTokens(
    const base::Value::List& disabled_tokens_list) {
  if (disabled_tokens_list.size() > kMaxDisabledTokens) {
    LOG(WARNING) << "Input has " << disabled_tokens_list.size()
                 << " disabled tokens, which exceeds max of "
                 << kMaxDisabledTokens << " and will not be stored";
    return;
  }

  std::vector<std::string> disabled_tokens;
  disabled_tokens.reserve(disabled_tokens_list.size());
  for (const auto& item : disabled_tokens_list) {
    if (item.is_string()) {
      // TODO(crbug.com/40263412): Investigate storing the decoded strings.
      disabled_tokens.push_back(item.GetString());
    }
  }
  settings_.disabled_tokens = std::move(disabled_tokens);
}
blink::mojom::OriginTrialsSettingsPtr OriginTrialsSettingsStorage::GetSettings()
    const {
  return settings_.Clone();
}
}  // namespace embedder_support
