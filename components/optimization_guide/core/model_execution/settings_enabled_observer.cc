// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"

namespace optimization_guide {

SettingsEnabledObserver::SettingsEnabledObserver(
    proto::ModelExecutionFeature feature)
    : feature_(feature) {}

SettingsEnabledObserver::~SettingsEnabledObserver() = default;

}  // namespace optimization_guide
