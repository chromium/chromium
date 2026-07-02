// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/prediction_model_component_installer.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/delivery/prediction_model_component_configs.h"
#include "components/optimization_guide/core/delivery/prediction_model_component_update_listener.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace component_updater {

namespace {

class PredictionModelComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  PredictionModelComponentInstallerPolicy(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::PredictionModelComponentConfig& config,
      base::WeakPtr<optimization_guide::PredictionModelComponentUpdateListener>
          update_listener)
      : optimization_target_(optimization_target),
        config_(config),
        update_listener_(update_listener),
        ui_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  PredictionModelComponentInstallerPolicy(
      const PredictionModelComponentInstallerPolicy&) = delete;
  PredictionModelComponentInstallerPolicy& operator=(
      const PredictionModelComponentInstallerPolicy&) = delete;

  ~PredictionModelComponentInstallerPolicy() override = default;

 private:
  // ComponentInstallerPolicy implementation:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override {
    return true;
  }

  bool RequiresNetworkEncryption() const override { return false; }

  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override {
    return update_client::CrxInstaller::Result(0);  // Nothing custom here.
  }

  void OnCustomUninstall() override {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &optimization_guide::PredictionModelComponentUpdateListener::
                OnModelUninstalled,
            update_listener_, optimization_target_));
  }

  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override {
    DCHECK(!install_dir.empty());
    if (update_listener_) {
      update_listener_->MaybeUpdateModel(optimization_target_, version,
                                         install_dir);
    }
  }

  // Called during startup and installation before ComponentReady().
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override {
    return base::PathExists(install_dir.Append(
               optimization_guide::GetBaseFileNameForModels())) &&
           base::PathExists(install_dir.Append(
               optimization_guide::GetBaseFileNameForModelInfo()));
  }

  base::FilePath GetRelativeInstallDir() const override {
    return base::FilePath(FILE_PATH_LITERAL("OptGuidePredictionModels"))
        .AppendASCII(optimization_guide::GetStringNameForOptimizationTarget(
            optimization_target_));
  }

  void GetHash(std::vector<uint8_t>* hash) const override {
    if (!hash) {
      return;
    }
    *hash = config_.public_key_sha256();
  }

  std::string GetName() const override { return config_.component_name(); }

  update_client::InstallerAttributes GetInstallerAttributes() const override {
    return update_client::InstallerAttributes();
  }

  const optimization_guide::proto::OptimizationTarget optimization_target_;
  const optimization_guide::PredictionModelComponentConfig config_;
  base::WeakPtr<optimization_guide::PredictionModelComponentUpdateListener>
      update_listener_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
};

}  // namespace

std::unique_ptr<ComponentInstallerPolicy>
CreatePredictionModelComponentInstallerPolicy(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::PredictionModelComponentConfig& config,
    base::WeakPtr<optimization_guide::PredictionModelComponentUpdateListener>
        update_listener) {
  return std::make_unique<PredictionModelComponentInstallerPolicy>(
      optimization_target, config, update_listener);
}

void RegisterPredictionModelComponent(
    ComponentUpdateService* cus,
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::WeakPtr<optimization_guide::PredictionModelComponentUpdateListener>
        update_listener) {
  auto config = optimization_guide::GetPredictionModelComponentConfig(
      optimization_target);
  if (!config) {
    return;
  }

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      CreatePredictionModelComponentInstallerPolicy(optimization_target,
                                                    *config, update_listener));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
