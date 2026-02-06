// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"

#include "components/update_client/crx_update_item.h"

namespace optimization_guide {

namespace features {
BASE_FEATURE(kAIModelUnloadableProgress, base::FEATURE_ENABLED_BY_DEFAULT);

// The number of bytes that won't load when reporting downloadprogress until
// creation is completed.
//
// Calculated to occupy 10% of the loading bar when the model (currently
// 3556255776 bytes) isn't downloaded.
const base::FeatureParam<int> kAIModelUnloadableProgressBytes{
    &kAIModelUnloadableProgress, "ai_model_unloadable_progress_bytes",
    3556255776 / 9};
}  // namespace features

int64_t NormalizeModelDownloadProgress(int64_t bytes_so_far,
                                       int64_t total_bytes) {
  // If `bytes_so_far` is zero, we should have downloaded zero bytes
  // out of zero meaning we're at 100%. So set it to
  // `kNormalizedDownloadProgressMax` to avoid dividing by zero.
  if (total_bytes == 0) {
    CHECK_EQ(bytes_so_far, 0);
    return kNormalizedDownloadProgressMax;
  }

  double raw_progress_fraction =
      bytes_so_far / static_cast<double>(total_bytes);

  return raw_progress_fraction * kNormalizedDownloadProgressMax;
}

namespace {

bool IsDownloadEvent(const component_updater::CrxUpdateItem& item) {
  // See class comment: components/update_client/component.h
  switch (item.state) {
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDecompressing:
    case update_client::ComponentState::kPatching:
    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpToDate:
      return item.downloaded_bytes >= 0 && item.total_bytes >= 0;
    case update_client::ComponentState::kNew:
    case update_client::ComponentState::kChecking:
    case update_client::ComponentState::kCanUpdate:
    case update_client::ComponentState::kUpdated:
    case update_client::ComponentState::kUpdateError:
    case update_client::ComponentState::kRun:
      return false;
  }
}

bool IsAlreadyInstalled(const component_updater::CrxUpdateItem& item) {
  // See class comment: components/update_client/component.h
  switch (item.state) {
    case update_client::ComponentState::kUpdated:
    case update_client::ComponentState::kUpToDate:
      return true;
    case update_client::ComponentState::kNew:
    case update_client::ComponentState::kChecking:
    case update_client::ComponentState::kCanUpdate:
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDecompressing:
    case update_client::ComponentState::kPatching:
    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpdateError:
    case update_client::ComponentState::kRun:
      return false;
  }
}

}  // namespace

OnDeviceModelDownloadProgressManager::OnDeviceModelDownloadProgressManager(
    component_updater::ComponentUpdateService* component_update_service,
    base::flat_set<std::string> component_ids,
    bool enable_unloadable_progress)
    : component_update_service_(*component_update_service) {
  for (const auto& component_id : component_ids) {
    components_progress_.emplace(component_id, DownloadProgressInfo());
  }

  if (enable_unloadable_progress &&
      base::FeatureList::IsEnabled(features::kAIModelUnloadableProgress)) {
    never_load_component_bytes_ =
        features::kAIModelUnloadableProgressBytes.Get();
  }
}

OnDeviceModelDownloadProgressManager::~OnDeviceModelDownloadProgressManager() {
  component_updater_observation_.Reset();
}

AddDownloadProgressObserverCallback
OnDeviceModelDownloadProgressManager::GetAddObserverCallback() {
  return base::BindRepeating(&OnDeviceModelDownloadProgressManager::AddObserver,
                             weak_ptr_factory_.GetWeakPtr());
}

void OnDeviceModelDownloadProgressManager::AddObserver(
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver>
        observer_remote) {
  auto reporter = std::make_unique<Reporter>(*this, std::move(observer_remote));

  if (!components_total_bytes_.has_value()) {
    components_total_bytes_ = CalculateTotalBytes();
  }

  if (components_total_bytes_.has_value()) {
    int64_t components_leftover_bytes = CalculateLeftoverBytes();
    // If |components_leftover_bytes| is equal to |never_load_component_bytes_|,
    // all components are already downloaded. Will send a 0 progress update as
    // ComponentUpdateService won't send any updates for already installed
    // components.
    bool is_all_components_downloaded =
        components_leftover_bytes == never_load_component_bytes_;

    reporter->SetTotalBytes(components_leftover_bytes);
    if (is_all_components_downloaded) {
      reporter->UpdateProgress(0);
    }
  }

  reporters_.emplace(std::move(reporter));
  if (GetNumberOfReporters() == 1) {
    StartObserver();
  }
}

void OnDeviceModelDownloadProgressManager::RemoveReporter(Reporter* reporter) {
  CHECK(reporter);
  reporters_.erase(reporter);
  if (GetNumberOfReporters() == 0) {
    component_updater_observation_.Reset();

    components_total_bytes_ = std::nullopt;
    for (auto& [component_id, progress] : components_progress_) {
      progress = DownloadProgressInfo();
    }
  }
}

int OnDeviceModelDownloadProgressManager::GetNumberOfReporters() const {
  return reporters_.size();
}

void OnDeviceModelDownloadProgressManager::StartObserver() {
  size_t already_installed_count = 0;
  for (auto& [component_id, progress] : components_progress_) {
    component_updater::CrxUpdateItem item;
    bool success =
        component_update_service_->GetComponentDetails(component_id, &item);
    if (success && IsAlreadyInstalled(item)) {
      SetDownloadProgress(progress, 0, 0);
      already_installed_count += 1;
    }
  }

  if (already_installed_count == components_progress_.size()) {
    return;
  }
  component_updater_observation_.Observe(&component_update_service_.get());
}

void OnDeviceModelDownloadProgressManager::OnEvent(
    const component_updater::CrxUpdateItem& item) {
  if (!IsDownloadEvent(item)) {
    return;
  }

  auto it = components_progress_.find(item.id);
  if (it == components_progress_.end()) {
    return;
  }
  SetDownloadProgress(it->second,
                      std::min(item.downloaded_bytes, item.total_bytes),
                      item.total_bytes);
}

void OnDeviceModelDownloadProgressManager::SetDownloadProgress(
    DownloadProgressInfo& progress,
    int64_t downloaded_bytes,
    int64_t total_bytes) {
  int64_t downloaded_bytes_delta =
      downloaded_bytes - progress.downloaded_bytes.value_or(0);
  if (downloaded_bytes_delta < 0) {
    return;
  }

  progress.downloaded_bytes = downloaded_bytes;
  progress.total_bytes = total_bytes;

  // If total component bytes is not determined yet, try to calculate it.
  if (!components_total_bytes_.has_value()) {
    components_total_bytes_ = CalculateTotalBytes();
    if (!components_total_bytes_.has_value()) {
      return;
    }

    // Don't include already downloaded bytes in progress calculation.
    int64_t components_leftover_bytes = CalculateLeftoverBytes();
    downloaded_bytes_delta = 0;
    for (const auto& reporter : reporters_) {
      reporter->SetTotalBytes(components_leftover_bytes);
    }
  }

  for (const auto& reporter : reporters_) {
    reporter->UpdateProgress(downloaded_bytes_delta);
  }
}

std::optional<int64_t>
OnDeviceModelDownloadProgressManager::CalculateTotalBytes() {
  int64_t total_bytes = 0;
  for (const auto& [component_id, progress] : components_progress_) {
    if (!progress.total_bytes.has_value()) {
      return std::nullopt;
    }
    total_bytes += progress.total_bytes.value();
  }
  return total_bytes;
}

int64_t OnDeviceModelDownloadProgressManager::GetDownloadedBytes() const {
  int64_t total_downloaded_bytes = 0;
  for (const auto& [component_id, progress] : components_progress_) {
    total_downloaded_bytes += progress.downloaded_bytes.value_or(0);
  }
  return total_downloaded_bytes;
}

int64_t OnDeviceModelDownloadProgressManager::CalculateLeftoverBytes() const {
  CHECK(components_total_bytes_.has_value());

  int64_t downloaded_bytes = GetDownloadedBytes();
  int64_t components_total_bytes = components_total_bytes_.value();
  CHECK_LE(downloaded_bytes, components_total_bytes);

  components_total_bytes -= downloaded_bytes;
  // Add never load component bytes.
  components_total_bytes += never_load_component_bytes_;
  return components_total_bytes;
}

OnDeviceModelDownloadProgressManager::Reporter::Reporter(
    OnDeviceModelDownloadProgressManager& manager,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver>
        observer_remote)
    : manager_(manager), observer_remote_(std::move(observer_remote)) {
  observer_remote_.set_disconnect_handler(base::BindOnce(
      &Reporter::OnRemoteDisconnect, weak_ptr_factory_.GetWeakPtr()));
}

OnDeviceModelDownloadProgressManager::Reporter::~Reporter() = default;

void OnDeviceModelDownloadProgressManager::Reporter::OnRemoteDisconnect() {
  manager_->RemoveReporter(this);
}

void OnDeviceModelDownloadProgressManager::Reporter::SetTotalBytes(
    int64_t total_bytes) {
  CHECK_GE(total_bytes, 0);
  if (total_bytes_.has_value()) {
    CHECK_EQ(total_bytes_.value(), total_bytes);
    return;
  }

  total_bytes_ = total_bytes;
}

void OnDeviceModelDownloadProgressManager::Reporter::UpdateProgress(
    int64_t downloaded_bytes_delta) {
  if (!total_bytes_.has_value()) {
    return;
  }

  if (!has_fired_zero_progress_) {
    has_fired_zero_progress_ = true;
    last_reported_progress_ = 0;
    last_progress_time_ = base::TimeTicks::Now();
    observer_remote_->OnDownloadProgressUpdate(0,
                                               kNormalizedDownloadProgressMax);
  }

  int64_t total_bytes = total_bytes_.value();
  downloaded_bytes_ += downloaded_bytes_delta;

  CHECK_GE(downloaded_bytes_, 0);
  CHECK_LE(downloaded_bytes_, total_bytes);

  // Only report this event if we're at 100% or if more than 50ms has passed
  // since the last time we reported a progress event.
  if (downloaded_bytes_ != total_bytes) {
    base::TimeTicks current_time = base::TimeTicks::Now();
    if (current_time - last_progress_time_ <= base::Milliseconds(50)) {
      return;
    }
    last_progress_time_ = current_time;
  }

  int64_t normalized_progress =
      NormalizeModelDownloadProgress(downloaded_bytes_, total_bytes);
  // Don't report progress events we've already sent.
  if (normalized_progress == last_reported_progress_) {
    return;
  }

  CHECK_GT(normalized_progress, last_reported_progress_);
  CHECK_LE(normalized_progress, kNormalizedDownloadProgressMax);
  last_reported_progress_ = normalized_progress;

  // Send the progress event to the observer.
  observer_remote_->OnDownloadProgressUpdate(normalized_progress,
                                             kNormalizedDownloadProgressMax);
}

}  // namespace optimization_guide
