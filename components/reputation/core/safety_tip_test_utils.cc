// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reputation/core/safety_tip_test_utils.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "components/reputation/core/safety_tips_config.h"

namespace reputation {

std::unique_ptr<SafetyTipsConfig> GetOrCreateSafetyTipsConfig() {
  auto* old = GetSafetyTipsRemoteConfigProto();
  if (old) {
    return std::make_unique<SafetyTipsConfig>(*old);
  }

  auto conf = std::make_unique<SafetyTipsConfig>();
  // Any version ID will do.
  conf->set_version_id(4);
  return conf;
}

void InitializeSafetyTipConfig() {
  SetSafetyTipsRemoteConfigProto(GetOrCreateSafetyTipsConfig());
}

void SetSafetyTipPatternsWithFlagType(std::vector<std::string> patterns,
                                      FlaggedPage::FlagType type) {
  auto config_proto = GetOrCreateSafetyTipsConfig();
  config_proto->clear_flagged_page();

  std::sort(patterns.begin(), patterns.end());
  for (const auto& pattern : patterns) {
    FlaggedPage* page = config_proto->add_flagged_page();
    page->set_pattern(pattern);
    page->set_type(type);
  }

  SetSafetyTipsRemoteConfigProto(std::move(config_proto));
}

void SetSafetyTipBadRepPatterns(std::vector<std::string> patterns) {
  SetSafetyTipPatternsWithFlagType(patterns, FlaggedPage::BAD_REP);
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
    UrlPattern* page = config_proto->add_allowed_pattern();
    page->set_pattern(pattern);
  }
  for (const auto& pattern : target_patterns) {
    HostPattern* page = config_proto->add_allowed_target_pattern();
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

}  // namespace reputation
