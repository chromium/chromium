// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PREDICTION_MODEL_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PREDICTION_MODEL_COMPONENT_INSTALLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {
class PredictionModelComponentConfig;
class PredictionModelComponentUpdateListener;
}  // namespace optimization_guide

namespace component_updater {

class ComponentInstallerPolicy;
class ComponentUpdateService;

// Factory method to create the installer policy.
std::unique_ptr<ComponentInstallerPolicy>
CreatePredictionModelComponentInstallerPolicy(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::PredictionModelComponentConfig& config,
    base::WeakPtr<optimization_guide::PredictionModelComponentUpdateListener>
        update_listener);

// Registers a prediction model component for the given target if it is
// supported.
void RegisterPredictionModelComponent(
    ComponentUpdateService* cus,
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::WeakPtr<optimization_guide::PredictionModelComponentUpdateListener>
        update_listener);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PREDICTION_MODEL_COMPONENT_INSTALLER_H_
