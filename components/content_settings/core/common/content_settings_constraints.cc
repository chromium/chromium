// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_constraints.h"

namespace content_settings {

ContentSettingConstraints::ContentSettingConstraints()
    : ContentSettingConstraints(base::Time::Now()) {}

ContentSettingConstraints::ContentSettingConstraints(base::Time now)
    : created_at_(now) {}

ContentSettingConstraints::ContentSettingConstraints(
    ContentSettingConstraints&& other) = default;

ContentSettingConstraints& ContentSettingConstraints::operator=(
    ContentSettingConstraints&& other) = default;

ContentSettingConstraints::~ContentSettingConstraints() = default;

ContentSettingConstraints ContentSettingConstraints::Clone() const {
  ContentSettingConstraints clone;
  clone.created_at_ = created_at_;
  clone.lifetime_ = lifetime_;
  clone.session_model_ = session_model_;
  clone.track_last_visit_for_autoexpiration_ =
      track_last_visit_for_autoexpiration_;
  clone.decided_by_related_website_sets_ = decided_by_related_website_sets_;
  clone.options_ = options_.Clone();
  return clone;
}

}  // namespace content_settings
