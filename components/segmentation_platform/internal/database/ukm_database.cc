// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_database.h"

#include <string_view>

namespace segmentation_platform {

UkmDatabase::CustomSqlQuery::CustomSqlQuery() = default;

UkmDatabase::CustomSqlQuery::CustomSqlQuery(CustomSqlQuery&&) = default;

UkmDatabase::CustomSqlQuery::CustomSqlQuery(
    std::string_view query,
    const std::vector<processing::ProcessedValue>& bind_values)
    : query(query), bind_values(bind_values) {}

UkmDatabase::CustomSqlQuery::~CustomSqlQuery() = default;

}  // namespace segmentation_platform
