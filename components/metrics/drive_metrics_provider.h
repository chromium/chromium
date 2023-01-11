// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DRIVE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_DRIVE_METRICS_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace base {
class FilePath;
}

namespace metrics {

// Provides metrics about the local drives on a user's computer. Currently only
// checks to see if they incur a seek-time penalty (e.g. if they're SSDs).
class DriveMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit DriveMetricsProvider(int local_state_path_key);

  DriveMetricsProvider(const DriveMetricsProvider&) = delete;
  DriveMetricsProvider& operator=(const DriveMetricsProvider&) = delete;

  ~DriveMetricsProvider() override;

  // metrics::MetricsProvider:
  void AsyncInit(base::OnceClosure done_callback) override;
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DriveMetricsProviderTest, HasSeekPenalty);

  // A response to querying a drive as to whether it incurs a seek penalty.
  // |has_seek_penalty| is set if |success| is true.
  struct SeekPenaltyResponse {
    SeekPenaltyResponse();
    bool success;
    bool has_seek_penalty;
  };

  struct DriveMetrics {
    SeekPenaltyResponse app_drive;
    SeekPenaltyResponse user_data_drive;
  };

  // Determine whether the device that services |path| has a seek penalty.
  // Returns false if it couldn't be determined (e.g., |path| doesn't exist).
  static bool HasSeekPenalty(const base::FilePath& path,
                             bool* has_seek_penalty);

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
  void FillDriveMetrics(const SeekPenaltyResponse& response,
                        metrics::SystemProfileProto::Hardware::Drive* drive);

  // The key to give to base::PathService to obtain the path to local state
  // (supplied by the embedder).
  int local_state_path_key_;

  // Information gathered about various important drives.
  DriveMetrics metrics_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DriveMetricsProvider> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DRIVE_METRICS_PROVIDER_H_
