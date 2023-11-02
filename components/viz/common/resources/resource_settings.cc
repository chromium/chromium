// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/resource_settings.h"

namespace viz {

ResourceSettings::ResourceSettings() = default;

ResourceSettings::ResourceSettings(const ResourceSettings& other) = default;

ResourceSettings::~ResourceSettings() = default;

ResourceSettings& ResourceSettings::operator=(const ResourceSettings& other) =
    default;

}  // namespace viz
