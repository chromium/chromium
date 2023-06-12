// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_constraints.h"

namespace content_settings {

ContentSettingConstraints::ContentSettingConstraints() = default;

ContentSettingConstraints::ContentSettingConstraints(
    ContentSettingConstraints&& other) = default;
ContentSettingConstraints::ContentSettingConstraints(
    const ContentSettingConstraints& other) = default;
ContentSettingConstraints& ContentSettingConstraints::operator=(
    ContentSettingConstraints&& other) = default;
ContentSettingConstraints& ContentSettingConstraints::operator=(
    const ContentSettingConstraints& other) = default;

ContentSettingConstraints::~ContentSettingConstraints() = default;

}  // namespace content_settings
