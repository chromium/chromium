// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_type_configurer.h"

namespace syncer {

ModelTypeConfigurer::ConfigureParams::ConfigureParams()
    : reason(CONFIGURE_REASON_UNKNOWN), is_sync_feature_enabled(false) {}
ModelTypeConfigurer::ConfigureParams::ConfigureParams(ConfigureParams&& other) =
    default;
ModelTypeConfigurer::ConfigureParams::~ConfigureParams() = default;
ModelTypeConfigurer::ConfigureParams&
ModelTypeConfigurer::ConfigureParams::operator=(ConfigureParams&& other) =
    default;

ModelTypeConfigurer::ModelTypeConfigurer() = default;
ModelTypeConfigurer::~ModelTypeConfigurer() = default;

}  // namespace syncer
