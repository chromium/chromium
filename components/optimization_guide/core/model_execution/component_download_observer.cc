// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/component_download_observer.h"

#include <algorithm>

#include "components/update_client/crx_update_item.h"

namespace optimization_guide {

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

std::optional<std::pair<int64_t, int64_t>> ToDownloadProgress(
    const component_updater::CrxUpdateItem& item) {
  if (IsAlreadyInstalled(item)) {
    return std::make_pair(0, 0);
  }
  if (IsDownloadEvent(item)) {
    return std::make_pair(std::min(item.downloaded_bytes, item.total_bytes),
                          item.total_bytes);
  }
  return std::nullopt;
}

}  // namespace

std::optional<std::pair<int64_t, int64_t>> GetDownloadProgress(
    component_updater::ComponentUpdateService* component_update_service,
    const std::string& crx_id) {
  if (!component_update_service) {
    return std::nullopt;
  }
  component_updater::CrxUpdateItem item;
  if (!component_update_service->GetComponentDetails(crx_id, &item)) {
    return std::nullopt;
  }
  return ToDownloadProgress(item);
}

ComponentDownloadObserver::ComponentDownloadObserver(
    component_updater::ComponentUpdateService* component_update_service,
    const std::string& crx_id)
    : crx_id_(crx_id), component_update_service_(component_update_service) {
  observers_.set_disconnect_handler(
      base::BindRepeating(&ComponentDownloadObserver::OnObserverDisconnected,
                          base::Unretained(this)));
  if (component_update_service_) {
    component_updater_observation_.Observe(component_update_service_);
  }
}

ComponentDownloadObserver::~ComponentDownloadObserver() = default;

void ComponentDownloadObserver::OnObserverDisconnected(
    mojo::RemoteSetElementId id) {
  if (observers_.empty()) {
    component_updater_observation_.Reset();
    last_downloaded_bytes_ = std::nullopt;
    last_total_bytes_ = std::nullopt;
  }
}

void ComponentDownloadObserver::AddObserver(
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer) {
  observers_.Add(std::move(observer));

  if (!component_updater_observation_.IsObserving() &&
      component_update_service_) {
    component_updater_observation_.Observe(component_update_service_);
  }
}

void ComponentDownloadObserver::OnEvent(
    const component_updater::CrxUpdateItem& item) {
  if (item.id != crx_id_) {
    return;
  }

  if (auto progress = ToDownloadProgress(item)) {
    UpdateProgress(progress->first, progress->second);
  }
}

void ComponentDownloadObserver::UpdateProgress(int64_t downloaded_bytes,
                                               int64_t total_bytes) {
  // Only report this event if we're at 100% or if more than 50ms has passed
  // since the last time we reported a progress event.
  if (downloaded_bytes != total_bytes) {
    base::TimeTicks current_time = base::TimeTicks::Now();
    if (current_time - last_progress_time_ <= base::Milliseconds(50)) {
      return;
    }
    last_progress_time_ = current_time;
  }

  // Don't report progress events we've already sent.
  if (last_downloaded_bytes_ == downloaded_bytes &&
      last_total_bytes_ == total_bytes) {
    return;
  }

  last_downloaded_bytes_ = downloaded_bytes;
  last_total_bytes_ = total_bytes;

  for (auto& observer : observers_) {
    observer->OnDownloadProgressUpdate(downloaded_bytes, total_bytes);
  }
}

}  // namespace optimization_guide
