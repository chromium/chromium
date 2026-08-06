// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_COMPONENT_DOWNLOAD_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_COMPONENT_DOWNLOAD_OBSERVER_H_

#include <optional>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/component_updater/component_updater_service.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"

namespace optimization_guide {

// Returns the download progress for a given crx_id as a pair of
// (downloaded_bytes, total_bytes). Returns nullopt if the progress is unknown.
// (0, 0) download progress indicates that the component is already installed
// and up-to-date.
std::optional<std::pair<int64_t, int64_t>> GetDownloadProgress(
    component_updater::ComponentUpdateService* component_update_service,
    const std::string& crx_id);

// Observes the download progress of a single component and broadcasts it to
// any registered DownloadObservers.
// Similar to OnDeviceModelDownloadProgressManager but without the
// anti-fingerprinting math and undownloadable padding.
class ComponentDownloadObserver : public component_updater::ServiceObserver {
 public:
  ComponentDownloadObserver(
      component_updater::ComponentUpdateService* component_update_service,
      const std::string& crx_id);
  ~ComponentDownloadObserver() override;

  ComponentDownloadObserver(const ComponentDownloadObserver&) = delete;
  ComponentDownloadObserver& operator=(const ComponentDownloadObserver&) =
      delete;

  void AddObserver(
      mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer);

  bool IsEmpty() const { return observers_.empty(); }

 private:
  // component_updater::ServiceObserver:
  void OnEvent(const component_updater::CrxUpdateItem& item) override;

  void UpdateProgress(int64_t downloaded_bytes, int64_t total_bytes);

  void OnObserverDisconnected(mojo::RemoteSetElementId id);

  std::string crx_id_;
  raw_ptr<component_updater::ComponentUpdateService> component_update_service_;
  mojo::RemoteSet<on_device_model::mojom::DownloadObserver> observers_;
  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ServiceObserver>
      component_updater_observation_{this};

  std::optional<int64_t> last_downloaded_bytes_;
  std::optional<int64_t> last_total_bytes_;
  base::TimeTicks last_progress_time_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_COMPONENT_DOWNLOAD_OBSERVER_H_
