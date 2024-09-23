// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/data_type_configurer.h"

namespace syncer {

DataTypeConfigurer::ConfigureParams::ConfigureParams() = default;
DataTypeConfigurer::ConfigureParams::ConfigureParams(ConfigureParams&& other) =
    default;
DataTypeConfigurer::ConfigureParams::~ConfigureParams() = default;
DataTypeConfigurer::ConfigureParams&
DataTypeConfigurer::ConfigureParams::operator=(ConfigureParams&& other) =
    default;

DataTypeConfigurer::DataTypeConfigurer() = default;
DataTypeConfigurer::~DataTypeConfigurer() = default;

}  // namespace syncer
