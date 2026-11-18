// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/feature_list_creator/feature_list_creator_client.h"

namespace variations {

FeatureListCreatorClient::~FeatureListCreatorClient() = default;

crash_keys::SwitchFilterFunction FeatureListCreatorClient::GetSwitchFilter() {
  return &crash_keys::IsDefaultBoringSwitch;
}

std::vector<base::FeatureList::FeatureOverrideInfo>
FeatureListCreatorClient::GetExtraFeatureOverrides(
    const base::CommandLine& command_line) {
  return {};
}

}  // namespace variations
