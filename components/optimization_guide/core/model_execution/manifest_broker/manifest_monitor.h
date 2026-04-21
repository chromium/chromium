// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_MONITOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_MONITOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/byte_count.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/public/mojom/model_broker_debug.mojom.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace optimization_guide {

// Selects the Manifests of Model components this device should use.
// Notifies an observer when the manifest changes.
class ManifestMonitor {
 public:
  // Delegate to avoid depending on the InstallPolicy directly.
  class Delegate {
   public:
    // Returns the base install directory for on-demand models.
    virtual base::CallbackListSubscription ListenForManifestReady(
        base::RepeatingCallback<void(base::FilePath)> on_ready) = 0;

    // Gets the available free disk space in the install directory on a
    // background thread.
    virtual void GetFreeDiskSpace(
        base::OnceCallback<void(std::optional<base::ByteCount>)> callback)
        const = 0;
  };

  ManifestMonitor(PrefService& local_state,
                  PerformanceClassifier& performance_classifier,
                  Delegate& delegate);
  ~ManifestMonitor();

  // Sets the callback to be called when the manifest changes.
  // This is called once the ManifestBrokerState is fully constructed.
  void SetCallback(base::RepeatingClosure on_manifest_changed);

  // Returns the current manifest, once selected.
  const std::optional<Manifest>& manifest() const { return manifest_; }
  // Returns the amount of free disk space found at initialization.
  std::optional<base::ByteCount> free_space() const { return free_space_; }
  // Returns the base install directory for on-demand models.
  std::optional<base::FilePath> manifest_dir() const { return manifest_dir_; }

  // Returns a list of properties for the broker state info.
  std::vector<mojom::BrokerPropertyInfoPtr> GetBrokerProperties() const;

 private:
  // This should be called once during initialization.
  void OnDiskSpaceEvaluated(std::optional<base::ByteCount> free_space);

  // Called when the manifest component is ready / updated.
  void OnManifestReady(base::FilePath manifest_dir);

  // Called when any input is changed, to determine if the manifest should be
  // changed.
  void OnInputsChanged();

  // Uses the default manifest, which has uninstalls all assets.
  void UseUninstallManifest(Manifest::UninstallReason reason);

  // Called when the manifest is loaded.
  void OnManifestLoaded(
      base::expected<Manifest, Manifest::ParseError> manifest);

  // Inputs:
  const raw_ref<PerformanceClassifier> performance_classifier_;
  const raw_ref<PrefService> local_state_;
  std::optional<base::ByteCount> free_space_;
  std::optional<base::FilePath> manifest_dir_;

  // Outputs:
  std::optional<Manifest> manifest_;
  base::RepeatingClosure on_manifest_changed_;

  // Subscriptions:
  PrefChangeRegistrar pref_change_registrar_;
  base::CallbackListSubscription manifest_ready_subscription_;
  base::WeakPtrFactory<ManifestMonitor> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_MONITOR_H_
