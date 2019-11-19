// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_STATUS_GETTER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_STATUS_GETTER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"

namespace cryptauthv2 {
class RequestContext;
}  // namespace cryptauthv2

namespace chromeos {

namespace device_sync {

// Handles the BatchGetFeatureStatuses portion of the CryptAuth v2 DeviceSync
// protocol. Returns the feature statuses for each input device ID as a map from
// multidevice::SoftwareFeature to multidevice::SoftwareFeatureState along with
// the last time any feature was modified.
//
// A CryptAuthFeatureStatusGetter object is designed to be used for only one
// GetFeatureStatuses() call. For a new attempt, a new object should be created.
class CryptAuthFeatureStatusGetter {
 public:
  using SoftwareFeatureStateMap =
      std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>;

  struct DeviceSoftwareFeatureInfo {
    DeviceSoftwareFeatureInfo();
    DeviceSoftwareFeatureInfo(const SoftwareFeatureStateMap& feature_state_map,
                              base::Time last_modified_time);
    DeviceSoftwareFeatureInfo(const DeviceSoftwareFeatureInfo&);
    ~DeviceSoftwareFeatureInfo();

    // A map from SoftwareFeature to SoftwareFeatureState.
    SoftwareFeatureStateMap feature_state_map;

    // The last time any feature state was modified in CryptAuth.
    base::Time last_modified_time;
  };

  using IdToDeviceSoftwareFeatureInfoMap =
      base::flat_map<std::string, DeviceSoftwareFeatureInfo>;
  using GetFeatureStatusesAttemptFinishedCallback =
      base::OnceCallback<void(const IdToDeviceSoftwareFeatureInfoMap&,
                              CryptAuthDeviceSyncResult::ResultCode)>;

  virtual ~CryptAuthFeatureStatusGetter();

  // Starts the BatchGetFeatureStatuses portion of the CryptAuth v2 DeviceSync
  // flow, retrieving feature status for |device_ids|.
  void GetFeatureStatuses(const cryptauthv2::RequestContext& request_context,
                          const base::flat_set<std::string>& device_ids,
                          GetFeatureStatusesAttemptFinishedCallback callback);

 protected:
  CryptAuthFeatureStatusGetter();

  // Implementations should retrieve feature statuses for devices with IDs
  // |device_ids|, using CryptAuth's BatchGetFeatureStatuses API, and call
  // OnAttemptFinished() with the results.
  virtual void OnAttemptStarted(
      const cryptauthv2::RequestContext& request_context,
      const base::flat_set<std::string>& device_ids) = 0;

  void OnAttemptFinished(
      const IdToDeviceSoftwareFeatureInfoMap&
          id_to_device_software_feature_info_map,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code);

 private:
  GetFeatureStatusesAttemptFinishedCallback callback_;
  bool was_get_feature_statuses_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthFeatureStatusGetter);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_STATUS_GETTER_H_
