// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"

#include <memory>
#include <optional>
#include <string>

#include "base/byte_count.h"
#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"
#include "components/optimization_guide/core/model_execution/test/fake_component_update_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

TestManifestAssetManagerComponentState::InstallTarget::InstallTarget() =
    default;

TestManifestAssetManagerComponentState::InstallTarget::InstallTarget(
    const std::string& public_key_hex,
    const base::Version& version)
    : public_key_hex(public_key_hex), version(version) {}

TestManifestAssetManagerComponentState::InstallTarget::~InstallTarget() =
    default;

TestManifestAssetManagerComponentState::InstallTarget::InstallTarget(
    const InstallTarget&) = default;

TestManifestAssetManagerComponentState::InstallTarget&
TestManifestAssetManagerComponentState::InstallTarget::operator=(
    const InstallTarget&) = default;

class TestManifestAssetManagerComponentState::DelegateImpl final
    : public ManifestAssetManager::Delegate {
 public:
  explicit DelegateImpl(
      base::WeakPtr<TestManifestAssetManagerComponentState> state)
      : state_(state) {}
  ~DelegateImpl() override = default;

  base::CallbackListSubscription ListenForManifestReady(
      base::RepeatingCallback<void(base::FilePath)> on_ready) override {
    if (state_) {
      if (state_->manifest_path_) {
        on_ready.Run(*state_->manifest_path_);
      }
      return state_->manifest_ready_callbacks_.Add(std::move(on_ready));
    }
    return base::CallbackListSubscription();
  }

  void RegisterOnDemandComponent(
      const std::string& public_key_hex,
      const std::string& target_version,
      const std::string& component_name,
      base::WeakPtr<ManifestAssetManager> manager) override {
    if (!state_) {
      return;
    }
    VLOG(2) << "Register " << public_key_hex << " V:" << target_version;
    Registration& registration = state_->registrations_[public_key_hex];
    CHECK(!registration.pending_registration);
    CHECK(!registration.pending_uninstall);
    registration.manager = manager;
    registration.target = {public_key_hex, base::Version(target_version)};
    registration.pending_registration = true;

    if (!state_->defer_registration_callbacks_) {
      state_->RunPendingRegistrations(registration);
    }
  }

  void Uninstall(const std::string& public_key_hex,
                 base::WeakPtr<ManifestAssetManager> manager) override {
    if (!state_) {
      return;
    }
    VLOG(2) << "Uninstall " << public_key_hex;
    Registration& registration = state_->registrations_[public_key_hex];
    CHECK(!registration.pending_registration);
    CHECK(!registration.pending_uninstall);
    registration.manager = manager;
    registration.target.public_key_hex = public_key_hex;
    registration.target.version = std::nullopt;
    registration.pending_uninstall = true;

    if (!state_->defer_registration_callbacks_) {
      state_->RunPendingRegistrations(registration);
    }
  }

  void RequestUpdate(const std::string& public_key_hex,
                     bool is_background) override {
    if (!state_) {
      return;
    }
    VLOG(2) << "RequestUpdate " << public_key_hex;
    Registration& registration = state_->registrations_[public_key_hex];
    if (is_background) {
      registration.has_background_update_requested = true;
    } else {
      registration.has_foreground_update_requested = true;
    }
    state_->MaybeCompleteDownload(public_key_hex);
  }

  void GetFreeDiskSpace(base::OnceCallback<void(std::optional<base::ByteCount>)>
                            callback) const override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       state_ ? state_->free_disk_space_ : base::ByteCount(0)));
  }

 private:
  base::WeakPtr<TestManifestAssetManagerComponentState> state_;
};

TestManifestAssetManagerComponentState::InstallableComponent::
    InstallableComponent() = default;

TestManifestAssetManagerComponentState::InstallableComponent::
    InstallableComponent(const InstallTarget& target,
                         const base::FilePath& install_dir)
    : target(target), install_dir(install_dir) {}

TestManifestAssetManagerComponentState::InstallableComponent::
    ~InstallableComponent() = default;

TestManifestAssetManagerComponentState::InstallableComponent::
    InstallableComponent(const InstallableComponent&) = default;

TestManifestAssetManagerComponentState::InstallableComponent&
TestManifestAssetManagerComponentState::InstallableComponent::operator=(
    const InstallableComponent&) = default;

TestManifestAssetManagerComponentState::Registration::Registration() = default;

TestManifestAssetManagerComponentState::Registration::~Registration() = default;

TestManifestAssetManagerComponentState::Registration::Registration(
    const Registration&) = default;

TestManifestAssetManagerComponentState::Registration&
TestManifestAssetManagerComponentState::Registration::operator=(
    const Registration&) = default;

TestManifestAssetManagerComponentState::
    TestManifestAssetManagerComponentState() = default;

TestManifestAssetManagerComponentState::
    ~TestManifestAssetManagerComponentState() = default;

std::unique_ptr<ManifestAssetManager::Delegate>
TestManifestAssetManagerComponentState::CreateDelegate() {
  return std::make_unique<DelegateImpl>(weak_ptr_factory_.GetWeakPtr());
}

void TestManifestAssetManagerComponentState::RunPendingRegistrations(
    const std::string& public_key) {
  auto it = registrations_.find(public_key);
  if (it != registrations_.end()) {
    RunPendingRegistrations(it->second);
  }
}

void TestManifestAssetManagerComponentState::RunPendingRegistrations(
    Registration& registration) {
  if (registration.pending_registration) {
    registration.pending_registration = false;
    bool is_already_installed = false;
    auto it = installed_components_.find(registration.target.public_key_hex);
    if (it != installed_components_.end()) {
      is_already_installed =
          (it->second.target.version == registration.target.version);
    }
    if (registration.manager) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ManifestAssetManager::InstallerRegistered,
                                    registration.manager,
                                    registration.target.public_key_hex,
                                    registration.target.version->GetString(),
                                    is_already_installed));
    }
    MaybeCompleteDownload(registration.target.public_key_hex);
  }
  if (registration.pending_uninstall) {
    registration.pending_uninstall = false;
    installed_components_.erase(registration.target.public_key_hex);
    if (registration.manager) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ManifestAssetManager::OnAssetUninstalled,
                                    registration.manager,
                                    registration.target.public_key_hex));
    }
  }
}

void TestManifestAssetManagerComponentState::RunPendingRegistrations() {
  for (auto& [public_key, registration] : registrations_) {
    RunPendingRegistrations(registration);
  }
}

void TestManifestAssetManagerComponentState::SetDownloadScenario(
    DownloadScenario scenario) {
  download_scenario_ = scenario;
  if (download_scenario_ != DownloadScenario::kOffline) {
    for (const auto& [public_key, component] : installable_components_) {
      MaybeCompleteDownload(component.target.public_key_hex);
    }
  }
}

void TestManifestAssetManagerComponentState::MaybeCompleteDownload(
    const std::string& public_key) {
  VLOG(2) << "MaybeCompleteDownload: " << public_key;
  if (download_scenario_ == DownloadScenario::kOffline) {
    return;
  }

  auto reg_it = registrations_.find(public_key);
  if (reg_it == registrations_.end() || !reg_it->second.manager) {
    return;
  }
  Registration& registration = reg_it->second;
  if (registration.pending_registration || registration.pending_uninstall) {
    return;  // Wait for callback
  }

  if (download_scenario_ == DownloadScenario::kThrottled &&
      !registration.has_foreground_update_requested) {
    return;
  }

  auto inst_it = installable_components_.find(registration.target);
  if (inst_it == installable_components_.end()) {
    return;
  }

  installed_components_[public_key] = inst_it->second;
  registration.has_foreground_update_requested = false;
  registration.has_background_update_requested = false;

  VLOG(2) << "Posted OnAssetReady: " << public_key;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ManifestAssetManager::OnAssetReady, registration.manager,
                     inst_it->second.target.public_key_hex,
                     *inst_it->second.target.version,
                     inst_it->second.install_dir));
}

void TestManifestAssetManagerComponentState::UpdateManifest(
    std::unique_ptr<ManifestComponentDirectory> manifest_dir) {
  // Technically, the installed manifest shouldn't update until after it's
  // registered. However, we usually call this Update when "shutdown" to
  // simulate starting up with a new manifest after getting the new manifest but
  // not applying it yet.
  manifest_path_ = manifest_dir->path();
  manifest_ready_callbacks_.Notify(*manifest_path_);
  manifest_assets_.push_back(std::move(manifest_dir));
}

void TestManifestAssetManagerComponentState::UpdateBaseModel(
    const std::string& public_key,
    std::unique_ptr<FakeBaseModelAsset> asset) {
  InstallTarget target{public_key, base::Version(asset->version())};
  installable_components_[target] = {target, asset->path()};
  base_model_assets_.push_back(std::move(asset));
  MaybeCompleteDownload(public_key);
}

void TestManifestAssetManagerComponentState::UpdateModelAdaptation(
    const std::string& public_key,
    std::unique_ptr<FakeAdaptationAsset> asset) {
  InstallTarget target{public_key,
                       base::Version(base::NumberToString(asset->version()))};
  installable_components_[target] = {target, asset->dir()};
  adaptation_assets_.push_back(std::move(asset));
  MaybeCompleteDownload(public_key);
}

void TestManifestAssetManagerComponentState::UpdateSafetyModel(
    const std::string& public_key,
    std::unique_ptr<FakeSafetyModelAsset> asset) {
  InstallTarget target{
      public_key,
      base::Version(base::NumberToString(asset->model_info().GetVersion()))};
  installable_components_[target] = {
      target, asset->model_info().GetModelFilePath().DirName()};
  safety_model_assets_.push_back(std::move(asset));
  MaybeCompleteDownload(public_key);
}

void TestManifestAssetManagerComponentState::UpdateLanguageDetectionModel(
    const std::string& public_key,
    std::unique_ptr<FakeLanguageModelAsset> asset) {
  InstallTarget target{
      public_key,
      base::Version(base::NumberToString(asset->model_info().GetVersion()))};
  installable_components_[target] = {
      target, asset->model_info().GetModelFilePath().DirName()};
  language_model_assets_.push_back(std::move(asset));
  MaybeCompleteDownload(public_key);
}

void TestManifestAssetManagerComponentState::SimulateRestart() {
  VLOG(2) << "SimulateRestart";
  registrations_.clear();
}

bool TestManifestAssetManagerComponentState::IsRegistered(
    const std::string& public_key) const {
  auto it = registrations_.find(public_key);
  return it != registrations_.end();
}

bool TestManifestAssetManagerComponentState::IsRegistered(
    const InstallTarget& target) const {
  auto it = registrations_.find(target.public_key_hex);
  if (it == registrations_.end()) {
    return false;
  }
  return target.version == it->second.target.version;
}

bool TestManifestAssetManagerComponentState::WasUninstallRequested(
    const std::string& public_key) const {
  auto it = registrations_.find(public_key);
  if (it == registrations_.end()) {
    return false;
  }
  return !it->second.target.version;
}

bool TestManifestAssetManagerComponentState::WasOnDemandUpdateRequested(
    const std::string& public_key) const {
  auto it = registrations_.find(public_key);
  return it != registrations_.end() &&
         it->second.has_foreground_update_requested;
}

bool TestManifestAssetManagerComponentState::WasBackgroundUpdateRequested(
    const std::string& public_key) const {
  auto it = registrations_.find(public_key);
  return it != registrations_.end() &&
         it->second.has_background_update_requested;
}

bool TestManifestAssetManagerComponentState::IsInstalled(
    const InstallTarget& target) const {
  auto it = installed_components_.find(target.public_key_hex);
  return it != installed_components_.end() &&
         it->second.target.version == target.version;
}

bool TestManifestAssetManagerComponentState::IsUninstalled(
    const std::string& public_key) const {
  return !installed_components_.contains(public_key);
}

}  // namespace optimization_guide
