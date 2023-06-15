// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_metadata.h"

#include <stddef.h>

#include <tuple>

#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_constraints.h"

namespace content_settings {

RuleMetaData::RuleMetaData() = default;

void RuleMetaData::SetFromConstraints(
    const ContentSettingConstraints& constraints) {
  expiration_ = constraints.expiration();
  session_model_ = constraints.session_model();
}

bool RuleMetaData::operator==(const RuleMetaData& other) const {
  return std::tie(last_modified_, last_visited_, expiration_, session_model_) ==
         std::tie(other.last_modified_, other.last_visited_, other.expiration_,
                  other.session_model_);
}

}  // namespace content_settings
