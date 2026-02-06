// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_DOWNLOAD_PROGRESS_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_DOWNLOAD_PROGRESS_MANAGER_H_

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/scoped_observation.h"
#include "components/component_updater/component_updater_service.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"

namespace optimization_guide {

namespace features {

// When enabled some amount of bytes will not be loadable when creating one of
// the built-in AI APIs.
BASE_DECLARE_FEATURE(kAIModelUnloadableProgress);
extern const base::FeatureParam<int> kAIModelUnloadableProgressBytes;

}  // namespace features

static constexpr int kNormalizedDownloadProgressMax = 0x10000;

// Normalizes the model download progress by scaling `bytes_so_far` from
// having `total_bytes` its max to having a `kNormalizedDownloadProgressMax`
// as its max.
int64_t NormalizeModelDownloadProgress(int64_t bytes_so_far,
                                       int64_t total_bytes);

class OnDeviceModelDownloadProgressManager
    : public component_updater::ServiceObserver {
 public:
  OnDeviceModelDownloadProgressManager(
      component_updater::ComponentUpdateService* component_update_service,
      base::flat_set<std::string> component_ids,
      bool enable_unloadable_progress = true);
  ~OnDeviceModelDownloadProgressManager() override;

  // Not copyable or movable.
  OnDeviceModelDownloadProgressManager(
      const OnDeviceModelDownloadProgressManager&) = delete;
  OnDeviceModelDownloadProgressManager& operator=(
      const OnDeviceModelDownloadProgressManager&) = delete;

  // Adds a `DownloadObserver` to send progress updates. Each time there is a
  // new `PendingRemote<DownloadObserver>` added, a new Reporter will be created
  // to observe the progress updates from all components and report to the added
  // observer.
  void AddObserver(mojo::PendingRemote<on_device_model::mojom::DownloadObserver>
                       observer_remote);

  AddDownloadProgressObserverCallback GetAddObserverCallback();

 private:
  // Observes progress updates from `components`, filters and processes them,
  // and reports the result to `observer_remote`. Each Reporter may have
  // different |total_bytes_| based on when it is created.
  class Reporter {
   public:
    Reporter(OnDeviceModelDownloadProgressManager& manager,
             mojo::PendingRemote<on_device_model::mojom::DownloadObserver>
                 observer_remote);
    ~Reporter();

    // Not copyable or movable.
    Reporter(const Reporter&) = delete;
    Reporter& operator=(const Reporter&) = delete;

    void SetTotalBytes(int64_t total_bytes);
    void UpdateProgress(int64_t downloaded_bytes_delta);

   private:
    void OnRemoteDisconnect();

    // `manager_` owns `this`.
    base::raw_ref<OnDeviceModelDownloadProgressManager> manager_;

    mojo::Remote<on_device_model::mojom::DownloadObserver> observer_remote_;

    int64_t downloaded_bytes_ = 0;
    std::optional<int64_t> total_bytes_;

    bool has_fired_zero_progress_ = false;
    int64_t last_reported_progress_ = 0;
    base::TimeTicks last_progress_time_;

    base::WeakPtrFactory<Reporter> weak_ptr_factory_{this};
  };

  struct DownloadProgressInfo {
    std::optional<int64_t> downloaded_bytes;
    std::optional<int64_t> total_bytes;
  };

  FRIEND_TEST_ALL_PREFIXES(OnDeviceModelDownloadProgressManagerTest,
                           ReporterIsDestroyedWhenRemoteIsDisconnected);
  FRIEND_TEST_ALL_PREFIXES(OnDeviceModelDownloadProgressManagerTest,
                           AddNewObserverAfterRemoveObserver);

  int GetNumberOfReporters() const;

  void StartObserver();
  // component_updater::ServiceObserver:
  void OnEvent(const component_updater::CrxUpdateItem& item) override;

  void SetDownloadProgress(DownloadProgressInfo& progress,
                           int64_t downloaded_bytes,
                           int64_t total_bytes);

  void RemoveReporter(Reporter* reporter);

  std::optional<int64_t> CalculateTotalBytes();
  int64_t GetDownloadedBytes() const;
  int64_t CalculateLeftoverBytes() const;

  std::optional<int64_t> components_total_bytes_;
  int64_t never_load_component_bytes_ = 0;

  raw_ref<component_updater::ComponentUpdateService> component_update_service_;
  base::flat_set<std::unique_ptr<Reporter>, base::UniquePtrComparator>
      reporters_;
  std::unordered_map<std::string, DownloadProgressInfo> components_progress_;

  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ServiceObserver>
      component_updater_observation_{this};

  base::WeakPtrFactory<OnDeviceModelDownloadProgressManager> weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_DOWNLOAD_PROGRESS_MANAGER_H_
