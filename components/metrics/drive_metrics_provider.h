// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DRIVE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_DRIVE_METRICS_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/system_profile.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// Provides metrics about the local drives on a user's computer. Currently only
// checks to see if they incur a seek-time penalty (e.g. if they're SSDs).
class DriveMetricsProvider : public metrics::MetricsProvider {
 public:
  DriveMetricsProvider(int local_state_path_key, PrefService* local_state);

  DriveMetricsProvider(const DriveMetricsProvider&) = delete;
  DriveMetricsProvider& operator=(const DriveMetricsProvider&) = delete;

  ~DriveMetricsProvider() override;

  // metrics::MetricsProvider:
  void AsyncInit(base::OnceClosure done_callback) override;
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // A response to querying a drive as to whether it incurs a seek penalty.
  // |has_seek_penalty| is set if |success| is true.
  struct SeekPenaltyResponse {
    SeekPenaltyResponse();
    std::optional<bool> has_seek_penalty;
    std::optional<bool> is_removable;
    std::optional<bool> is_usb;
  };

  struct DriveMetrics {
    SeekPenaltyResponse app_drive;
    SeekPenaltyResponse user_data_drive;
  };

  // Gather metrics about various drives. Should be run on a background thread.
  static DriveMetrics GetDriveMetricsOnBackgroundThread(
      int local_state_path_key);

  // Tries to determine whether there is a penalty for seeking on the drive that
  // hosts |path_service_key| (for example: the drive that holds "Local State").
  static void QuerySeekPenalty(int path_service_key,
                               SeekPenaltyResponse* response);

  // Called when metrics are done being gathered asynchronously.
  // |done_callback| is the callback that should be called once all metrics are
  // gathered.
  void GotDriveMetrics(base::OnceClosure done_callback,
                       const DriveMetrics& metrics);

  // Fills |drive| with information from successful |response|s.
  // |pref_name| is used to cache/retrieve the information in/from
  // |local_state_|.
  void FillDriveMetrics(const SeekPenaltyResponse& response,
                        metrics::SystemProfileProto::Hardware::Drive* drive,
                        const char* pref_name);

  // The key to give to base::PathService to obtain the path to local state
  // (supplied by the embedder).
  int local_state_path_key_;

  raw_ptr<PrefService> local_state_;

  // Information gathered about various important drives.
  DriveMetrics metrics_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DriveMetricsProvider> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(DriveMetricsProviderTest,
                           HasSeekPenalty_FallbackToLocalState);
  FRIEND_TEST_ALL_PREFIXES(DriveMetricsProviderTest,
                           HasSeekPenalty_WritesToLocalState);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DRIVE_METRICS_PROVIDER_H_
