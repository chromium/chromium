// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_params.h"

namespace performance_manager::resource_attribution::internal {

QueryParams::QueryParams() = default;

QueryParams::~QueryParams() = default;

QueryParams::QueryParams(const QueryParams& other) = default;

QueryParams& QueryParams::operator=(const QueryParams& other) = default;

}  // namespace performance_manager::resource_attribution::internal
