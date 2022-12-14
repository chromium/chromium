// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/group_configurations.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/group_constants.h"

namespace feature_engagement {

absl::optional<GroupConfig> GetClientSideGroupConfig(
    const base::Feature* group) {
  if (kIPHDummyGroup.name == group->name) {
    // Only used for tests. Various magic tricks are used below to ensure this
    // config is invalid and unusable.
    absl::optional<GroupConfig> config = GroupConfig();
    config->valid = false;
    config->session_rate = Comparator(LESS_THAN, 0);
    config->trigger =
        EventConfig("dummy_group_iph_trigger", Comparator(LESS_THAN, 0), 1, 1);
    return config;
  }

  return absl::nullopt;
}

}  // namespace feature_engagement
