// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_DISTRIBUTION_IWA_KEY_DISTRIBUTION_INFO_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_DISTRIBUTION_IWA_KEY_DISTRIBUTION_INFO_PROVIDER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/one_shot_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_histograms.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"

namespace base {
class FilePath;
}  // namespace base

namespace web_app {

class IwaInternalsHandler;

// Enables the key distribution dev mode UI on chrome://web-app-internals.
BASE_DECLARE_FEATURE(kIwaKeyDistributionDevMode);

// This class is a singleton responsible for processing the IWA Key Distribution
// Component data.
class IwaKeyDistributionInfoProvider {
 public:
  struct KeyRotationInfo {
    using PublicKeyData = std::vector<uint8_t>;

    explicit KeyRotationInfo(std::optional<PublicKeyData> public_key);
    ~KeyRotationInfo();
    KeyRotationInfo(const KeyRotationInfo&);

    base::Value AsDebugValue() const;

    std::optional<PublicKeyData> public_key;
  };

  using KeyRotations = base::flat_map<std::string, KeyRotationInfo>;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnComponentUpdateSuccess(const base::Version& version,
                                          bool is_preloaded) {}
    virtual void OnComponentUpdateError(const base::Version& version,
                                        IwaComponentUpdateError error) {}
  };

  static IwaKeyDistributionInfoProvider* GetInstance();
  static void DestroyInstanceForTesting();

  ~IwaKeyDistributionInfoProvider();

  IwaKeyDistributionInfoProvider(const IwaKeyDistributionInfoProvider&) =
      delete;
  IwaKeyDistributionInfoProvider& operator=(
      const IwaKeyDistributionInfoProvider&) = delete;

  const KeyRotationInfo* GetKeyRotationInfo(
      const std::string& web_bundle_id) const;

  // Asynchronously loads new component data and replaces the current `data_`
  // upon success and if `component_version` is greater than the stored one, and
  // informs observers about the operation result.
  void LoadKeyDistributionData(const base::Version& component_version,
                               const base::FilePath& file_path,
                               bool is_preloaded);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets a custom key rotation outside of the component updater flow and
  // triggers an `OnComponentUpdateSuccess()` event. The usage of this function
  // is intentionally limited to chrome://web-app-internals.
  void RotateKeyForDevMode(
      base::PassKey<IwaInternalsHandler>,
      const std::string& web_bundle_id,
      const std::optional<std::vector<uint8_t>>& rotated_key);

  // Dumps the entire component data to web-app-internals.
  base::Value AsDebugValue() const;

  // Writes component metadata (version and whether it's preloaded) to `log`.
  void WriteComponentMetadata(base::Value::Dict& log) const;

  // Use this to post IWA-related tasks if they rely on the key distribution
  // component. The event will be signalled
  //  * Immediately if the kIwaKeyDistributionComponent flag is disabled
  //  * Upon component loading if the available component is downloaded and not
  //    preloaded
  //  * In 15 seconds after the first call to this function if the available
  //  component is preloaded and the component updater is unable to fetch a
  //  newer version.
  base::OneShotEvent& OnMaybeDownloadedComponentDataReady();

  std::optional<bool> IsPreloadedForTesting() const;

 private:
  struct ComponentData {
    ComponentData(base::Version version,
                  KeyRotations key_rotations,
                  bool is_preloaded);
    ~ComponentData();
    ComponentData(const ComponentData&);

    base::Version version;
    KeyRotations key_rotations;

    bool is_preloaded = false;
  };

  IwaKeyDistributionInfoProvider();

  // Posts `MaybeQueueComponentUpdate()` onto `any_data_ready_` once.
  void PostMaybeQueueComponentUpdateOnceOnDataReady();

  // Queues a component update request with a fallback.
  // By the time of this call, `any_data_ready_` is already signalled.
  //  * If the request succeeds, will signal `maybe_downloaded_data_ready_` via
  //    OnKeyDistributionDataLoaded();
  //  * If not, will signal `maybe_downloaded_data_ready_` in 15 seconds after
  //    the call. Note that the fallback preloaded version is guaranteed to be
  //    loaded in this case.
  void MaybeQueueComponentUpdate();

  void OnKeyDistributionDataLoaded(
      const base::Version& version,
      bool is_preloaded,
      base::expected<KeyRotations, IwaComponentUpdateError>);

  void DispatchComponentUpdateSuccess(const base::Version& version,
                                      bool is_preloaded) const;

  void DispatchComponentUpdateError(const base::Version& version,
                                    IwaComponentUpdateError error) const;

  void SignalOnDataReady(bool is_preloaded);

  // Will be signalled once any component version (regardless of whether
  // preloaded or downloaded) is loaded.
  base::OneShotEvent any_data_ready_;

  // Will be signalled either if a downloaded component version is loaded or in
  // 15 seconds after the preloaded version has been loaded. See
  // OnMaybeDownloadedComponentDataReady() for details.
  base::OneShotEvent maybe_downloaded_data_ready_;

  bool maybe_queue_component_update_posted_ = false;

  std::optional<ComponentData> data_;
  base::ObserverList<Observer> observers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_DISTRIBUTION_IWA_KEY_DISTRIBUTION_INFO_PROVIDER_H_
