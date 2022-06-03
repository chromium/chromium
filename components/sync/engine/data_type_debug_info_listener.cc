// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/data_type_debug_info_listener.h"

namespace syncer {

DataTypeConfigurationStats::DataTypeConfigurationStats()
    : model_type(UNSPECIFIED) {}

DataTypeConfigurationStats::DataTypeConfigurationStats(
    const DataTypeConfigurationStats& other) = default;

DataTypeConfigurationStats::~DataTypeConfigurationStats() = default;

}  // namespace syncer
