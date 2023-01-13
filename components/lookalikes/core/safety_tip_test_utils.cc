// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/safety_tip_test_utils.h"

#include "components/lookalikes/core/safety_tips_config.h"

namespace lookalikes {

std::unique_ptr<reputation::SafetyTipsConfig> GetOrCreateSafetyTipsConfig() {
  auto* old = GetSafetyTipsRemoteConfigProto();
  if (old) {
    return std::make_unique<reputation::SafetyTipsConfig>(*old);
  }

  auto conf = std::make_unique<reputation::SafetyTipsConfig>();
  // Any version ID will do.
  conf->set_version_id(4);
  return conf;
}

void InitializeSafetyTipConfig() {
  SetSafetyTipsRemoteConfigProto(GetOrCreateSafetyTipsConfig());
}

void SetSafetyTipAllowlistPatterns(std::vector<std::string> patterns,
                                   std::vector<std::string> target_patterns,
                                   std::vector<std::string> common_words) {
  auto config_proto = GetOrCreateSafetyTipsConfig();
  config_proto->clear_allowed_pattern();
  config_proto->clear_allowed_target_pattern();
  config_proto->clear_common_word();

  std::sort(patterns.begin(), patterns.end());
  std::sort(target_patterns.begin(), target_patterns.end());
  std::sort(common_words.begin(), common_words.end());

  for (const auto& pattern : patterns) {
    reputation::UrlPattern* page = config_proto->add_allowed_pattern();
    page->set_pattern(pattern);
  }
  for (const auto& pattern : target_patterns) {
    reputation::HostPattern* page = config_proto->add_allowed_target_pattern();
    page->set_regex(pattern);
  }
  for (const auto& word : common_words) {
    config_proto->add_common_word(word);
  }
  SetSafetyTipsRemoteConfigProto(std::move(config_proto));
}

void InitializeBlankLookalikeAllowlistForTesting() {
  SetSafetyTipAllowlistPatterns({}, {}, {});
}

void AddSafetyTipHeuristicLaunchConfigForTesting(
    reputation::HeuristicLaunchConfig::Heuristic heuristic,
    int launch_percentage) {
  auto config_proto = GetOrCreateSafetyTipsConfig();
  reputation::HeuristicLaunchConfig* launch_config =
      config_proto->add_launch_config();
  launch_config->set_heuristic(heuristic);
  launch_config->set_launch_percentage(launch_percentage);
  SetSafetyTipsRemoteConfigProto(std::move(config_proto));
}

}  // namespace lookalikes
