// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/blocked_iph_features.h"

#include <vector>

#include "base/base_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace feature_engagement {

const char BlockedIphFeatures::kPropagateIPHForTestingSwitch[] =
    "propagate-iph-for-testing";

BlockedIphFeatures::BlockedIphFeatures() = default;
BlockedIphFeatures::~BlockedIphFeatures() = default;

void BlockedIphFeatures::IncrementGlobalBlockCount() {
  ++global_block_count_;
}

void BlockedIphFeatures::DecrementGlobalBlockCount() {
  DCHECK_GT(global_block_count_, 0U);
  --global_block_count_;
}

void BlockedIphFeatures::IncrementFeatureAllowedCount(
    const std::string& feature_name) {
  DCHECK(!feature_name.empty());
  ++allow_feature_counts_[feature_name];
}

void BlockedIphFeatures::DecrementFeatureAllowedCount(
    const std::string& feature_name) {
  DCHECK_GT(allow_feature_counts_[feature_name], 0U);
  --allow_feature_counts_[feature_name];
}

bool BlockedIphFeatures::IsFeatureBlocked(
    const std::string& feature_name) const {
  // If nobody has requested blocks, then nothing is blocked.
  if (global_block_count_ == 0U) {
    return false;
  }

  // All features are blocked unless explicitly allowed.
  const auto it = allow_feature_counts_.find(feature_name);
  return it == allow_feature_counts_.end() || it->second == 0U;
}

void BlockedIphFeatures::MaybeWriteToCommandLine(
    base::CommandLine& command_line) const {
  if (global_block_count_ == 0U) {
    return;
  }

  std::vector<std::string> features;
  for (const auto& [name, count] : allow_feature_counts_) {
    if (count > 0U) {
      features.push_back(name);
    }
  }

  std::string value_string = base::JoinString(features, ",");
  command_line.AppendSwitchASCII(kPropagateIPHForTestingSwitch, value_string);
  if (!features.empty()) {
    if (command_line.HasSwitch(switches::kEnableFeatures)) {
      const std::string old_value =
          command_line.GetSwitchValueASCII(switches::kEnableFeatures);
      if (!old_value.empty()) {
        value_string = base::StrCat({old_value, ",", value_string});
      }
      command_line.RemoveSwitch(switches::kEnableFeatures);
    }
    command_line.AppendSwitchASCII(switches::kEnableFeatures, value_string);
  }
}

void BlockedIphFeatures::MaybeReadFromCommandLine() {
  if (read_from_command_line_) {
    return;
  }
  read_from_command_line_ = true;

  const base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kPropagateIPHForTestingSwitch)) {
    IncrementGlobalBlockCount();
    const std::string value =
        command_line->GetSwitchValueASCII(kPropagateIPHForTestingSwitch);
    if (!value.empty()) {
      auto features = base::FeatureList::SplitFeatureListString(value);
      for (auto& feature_name : features) {
        if (!feature_name.empty()) {
          IncrementFeatureAllowedCount(std::string(feature_name));
        }
      }
    }
  }
}

// static
BlockedIphFeatures* BlockedIphFeatures::GetInstance() {
  static base::NoDestructor<BlockedIphFeatures> instance;
  auto* const test_features = instance.get();
  {
    base::AutoLock lock(test_features->GetLock());
    test_features->MaybeReadFromCommandLine();
  }
  return test_features;
}

}  // namespace feature_engagement
