// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_feature_status_getter.h"

#include <utility>

namespace ash {

namespace device_sync {

CryptAuthFeatureStatusGetter::DeviceSoftwareFeatureInfo::
    DeviceSoftwareFeatureInfo() = default;

CryptAuthFeatureStatusGetter::DeviceSoftwareFeatureInfo::
    DeviceSoftwareFeatureInfo(const SoftwareFeatureStateMap& feature_state_map,
                              base::Time last_modified_time)
    : feature_state_map(feature_state_map),
      last_modified_time(last_modified_time) {}

CryptAuthFeatureStatusGetter::DeviceSoftwareFeatureInfo::
    DeviceSoftwareFeatureInfo(const DeviceSoftwareFeatureInfo&) = default;

CryptAuthFeatureStatusGetter::DeviceSoftwareFeatureInfo::
    ~DeviceSoftwareFeatureInfo() = default;

CryptAuthFeatureStatusGetter::CryptAuthFeatureStatusGetter() = default;

CryptAuthFeatureStatusGetter::~CryptAuthFeatureStatusGetter() = default;

void CryptAuthFeatureStatusGetter::GetFeatureStatuses(
    const cryptauthv2::RequestContext& request_context,
    const base::flat_set<std::string>& device_ids,
    GetFeatureStatusesAttemptFinishedCallback callback) {
  // Enforce that GetFeatureStatuses() can only be called once.
  DCHECK(!was_get_feature_statuses_called_);
  was_get_feature_statuses_called_ = true;

  callback_ = std::move(callback);

  OnAttemptStarted(request_context, device_ids);
}

void CryptAuthFeatureStatusGetter::OnAttemptFinished(
    const IdToDeviceSoftwareFeatureInfoMap&
        id_to_device_software_feature_info_map,
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK(callback_);
  std::move(callback_).Run(id_to_device_software_feature_info_map,
                           device_sync_result_code);
}

}  // namespace device_sync

}  // namespace ash
