// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/local_data_description.h"

namespace syncer {

LocalDataDescription::LocalDataDescription() = default;

LocalDataDescription::LocalDataDescription(
    ModelType type,
    size_t item_count,
    const std::vector<std::string>& item_preview)
    : type(type), item_count(item_count), item_preview(item_preview) {}

LocalDataDescription::LocalDataDescription(const LocalDataDescription&) =
    default;

LocalDataDescription& LocalDataDescription::operator=(
    const LocalDataDescription&) = default;

LocalDataDescription::LocalDataDescription(LocalDataDescription&&) = default;

LocalDataDescription& LocalDataDescription::operator=(LocalDataDescription&&) =
    default;

LocalDataDescription::~LocalDataDescription() = default;

}  // namespace syncer
