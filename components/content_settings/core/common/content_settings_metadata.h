// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_METADATA_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_METADATA_H_

#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_constraints.h"

namespace content_settings {

// Holds metadata for a ContentSetting rule.
struct RuleMetaData {
  // Last Modified data as specified by some UserModifiableProvider
  // implementations. May be null.
  base::Time last_modified;
  // Last visited data as specified by some UserModifiableProvider
  // implementations. Only populated when
  // ContentSettingsConstraint::track_last_visit_for_autoexpiration is enabled.
  base::Time last_visited;
  // Expiration date if defined through a ContentSettingsConstraint.
  base::Time expiration;
  // SessionModel as defined through a ContentSettingsConstraint.
  SessionModel session_model = SessionModel::Durable;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_METADATA_H_
