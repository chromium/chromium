// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_monitor.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

namespace {

DeviceCategory GetDeviceCategory(const PerformanceClassifier& classifier) {
  if (!classifier.IsDeviceGPUCapable()) {
    return DeviceCategory::kCpu;
  }
  if (classifier.IsLowTierDevice()) {
    return DeviceCategory::kGpuLowTier;
  }
  return DeviceCategory::kGpuHighTier;
}

bool IsAllowedByPolicy(const PrefService& local_state) {
  return features::IsOnDeviceExecutionEnabled() &&
         GetGenAILocalFoundationalModelEnterprisePolicySettings(&local_state) ==
             model_execution::prefs::
                 GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed;
}

bool IsAllowedByUserSetting(const PrefService& local_state) {
  return local_state.GetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled);
}

}  // namespace

ManifestMonitor::ManifestMonitor(PrefService& local_state,
                                 PerformanceClassifier& performance_classifier,
                                 Delegate& delegate)
    : performance_classifier_(performance_classifier),
      local_state_(local_state) {
  TRACE_EVENT("optimization_guide", "ManifestMonitor::ManifestMonitor",
              perfetto::Flow::FromPointer(this));
  pref_change_registrar_.Init(&local_state_.get());
  pref_change_registrar_.Add(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      base::BindRepeating(&ManifestMonitor::OnInputsChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      base::BindRepeating(&ManifestMonitor::OnInputsChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  manifest_ready_subscription_ =
      delegate.ListenForManifestReady(base::BindRepeating(
          &ManifestMonitor::OnManifestReady, weak_ptr_factory_.GetWeakPtr()));

  performance_classifier_->ListenForPerformanceClassAvailable(base::BindOnce(
      &ManifestMonitor::OnInputsChanged, weak_ptr_factory_.GetWeakPtr()));

  delegate.GetFreeDiskSpace(base::BindOnce(
      &ManifestMonitor::OnDiskSpaceEvaluated, weak_ptr_factory_.GetWeakPtr()));
}
ManifestMonitor::~ManifestMonitor() {
  TRACE_EVENT("optimization_guide", "ManifestMonitor::~ManifestMonitor",
              perfetto::TerminatingFlow::FromPointer(this));
}

void ManifestMonitor::SetCallback(base::RepeatingClosure on_manifest_changed) {
  on_manifest_changed_ = std::move(on_manifest_changed);
  OnInputsChanged();
}

void ManifestMonitor::OnDiskSpaceEvaluated(
    std::optional<base::ByteCount> free_space) {
  TRACE_EVENT("optimization_guide", "ManifestMonitor::OnDiskSpaceEvaluated",
              perfetto::Flow::FromPointer(this));
  free_space_ = free_space;
  OnInputsChanged();
}

void ManifestMonitor::OnManifestReady(base::FilePath manifest_dir) {
  TRACE_EVENT("optimization_guide", "ManifestMonitor::OnManifestReady",
              perfetto::Flow::FromPointer(this));
  manifest_dir_ = manifest_dir;
  OnInputsChanged();
}

void ManifestMonitor::OnInputsChanged() {
  TRACE_EVENT("optimization_guide", "ManifestMonitor::OnInputsChanged",
              perfetto::Flow::FromPointer(this));
  if (!on_manifest_changed_) {
    return;
  }
  if (!IsAllowedByPolicy(*local_state_)) {
    UseUninstallManifest(Manifest::UninstallReason::kDisallowedByPolicy);
    return;
  }
  if (!IsAllowedByUserSetting(*local_state_)) {
    UseUninstallManifest(Manifest::UninstallReason::kDisallowedByUser);
    return;
  }
  if (performance_classifier_->IsPerformanceClassAvailable() &&
      !performance_classifier_->IsDeviceCapable()) {
    UseUninstallManifest(Manifest::UninstallReason::kDeviceNotCapable);
    return;
  }
  if (free_space_.has_value() &&
      features::IsFreeDiskSpaceTooLowForOnDeviceModelInstall(*free_space_)) {
    UseUninstallManifest(Manifest::UninstallReason::kInsufficientDisk);
    return;
  }
  if (!manifest_dir_ ||
      !performance_classifier_->IsPerformanceClassAvailable() ||
      !free_space_.has_value()) {
    // We are not ready to load the manifest yet.
    return;
  }
  Manifest::Load(*manifest_dir_, GetDeviceCategory(*performance_classifier_),
                 base::BindOnce(&ManifestMonitor::OnManifestLoaded,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ManifestMonitor::UseUninstallManifest(Manifest::UninstallReason reason) {
  TRACE_EVENT("optimization_guide", "ManifestMonitor::UseUninstallManifest",
              perfetto::Flow::FromPointer(this));
  if (!manifest_ || manifest_->HasAssets()) {
    manifest_.emplace(reason);
    on_manifest_changed_.Run();
  }
}

void ManifestMonitor::OnManifestLoaded(
    base::expected<Manifest, Manifest::ParseError> manifest) {
  TRACE_EVENT("optimization_guide", "ManifestMonitor::OnManifestLoaded");
  if (!manifest.has_value()) {
    UseUninstallManifest(Manifest::UninstallReason::kParseError);
    return;
  }
  if (!IsAllowedByPolicy(*local_state_)) {
    UseUninstallManifest(Manifest::UninstallReason::kDisallowedByPolicy);
    return;
  }
  if (!IsAllowedByUserSetting(*local_state_)) {
    UseUninstallManifest(Manifest::UninstallReason::kDisallowedByUser);
    return;
  }
  manifest_.emplace(std::move(manifest.value()));
  on_manifest_changed_.Run();
}

std::vector<mojom::BrokerPropertyInfoPtr> ManifestMonitor::GetBrokerProperties()
    const {
  std::vector<mojom::BrokerPropertyInfoPtr> properties;
  properties.push_back(mojom::BrokerPropertyInfo::New(
      "Manifest Path",
      manifest_dir_.has_value() ? manifest_dir_->AsUTF8Unsafe() : "N/A"));
  properties.push_back(mojom::BrokerPropertyInfo::New(
      "Free Disk Space", free_space_.has_value()
                             ? base::ToString(free_space_->InMiB()) + " MiB"
                             : "N/A"));
  properties.push_back(mojom::BrokerPropertyInfo::New(
      "Manifest Loaded", manifest_.has_value() ? "true" : "false"));
  return properties;
}

}  // namespace optimization_guide
