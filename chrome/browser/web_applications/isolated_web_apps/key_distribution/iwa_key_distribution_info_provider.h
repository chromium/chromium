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

class IsolatedWebAppPolicyManager;
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

  // Attempts to fetch the latest version of the Iwa Key Distribution component
  // from Omaha on session startup if the currently available version is
  // preloaded. Returns `true` if the fetch task has been queued, and `false`
  // otherwise.
  // Note that `true` can only be returned once.
  bool MaybeQueueComponentUpdateOnce(
      base::PassKey<IsolatedWebAppPolicyManager>);

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

  base::Value AsDebugValue() const;

  bool Ready() const;

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

  void OnKeyDistributionDataLoaded(
      const base::Version& version,
      bool is_preloaded,
      base::expected<KeyRotations, IwaComponentUpdateError>);

  void DispatchComponentUpdateSuccess(const base::Version& version,
                                      bool is_preloaded) const;

  void DispatchComponentUpdateError(const base::Version& version,
                                    IwaComponentUpdateError error) const;

  // Component data protobuf parsing tasks are posted to a sequenced runner
  // instead of a thread pool to prevent possible version races.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::optional<ComponentData> data_;
  base::ObserverList<Observer> observers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_DISTRIBUTION_IWA_KEY_DISTRIBUTION_INFO_PROVIDER_H_
