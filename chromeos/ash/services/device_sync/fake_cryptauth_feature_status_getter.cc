// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_cryptauth_feature_status_getter.h"

namespace ash {

namespace device_sync {

FakeCryptAuthFeatureStatusGetter::FakeCryptAuthFeatureStatusGetter() = default;

FakeCryptAuthFeatureStatusGetter::~FakeCryptAuthFeatureStatusGetter() = default;

void FakeCryptAuthFeatureStatusGetter::FinishAttempt(
    const IdToDeviceSoftwareFeatureInfoMap&
        id_to_device_software_feature_info_map,
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK(request_context_);
  DCHECK(device_ids_);

  OnAttemptFinished(id_to_device_software_feature_info_map,
                    device_sync_result_code);
}

void FakeCryptAuthFeatureStatusGetter::OnAttemptStarted(
    const cryptauthv2::RequestContext& request_context,
    const base::flat_set<std::string>& device_ids) {
  request_context_ = request_context;
  device_ids_ = device_ids;
}

FakeCryptAuthFeatureStatusGetterFactory::
    FakeCryptAuthFeatureStatusGetterFactory() = default;

FakeCryptAuthFeatureStatusGetterFactory::
    ~FakeCryptAuthFeatureStatusGetterFactory() = default;

std::unique_ptr<CryptAuthFeatureStatusGetter>
FakeCryptAuthFeatureStatusGetterFactory::CreateInstance(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  last_client_factory_ = client_factory;

  auto instance = std::make_unique<FakeCryptAuthFeatureStatusGetter>();
  instances_.push_back(instance.get());

  return instance;
}

}  // namespace device_sync

}  // namespace ash
