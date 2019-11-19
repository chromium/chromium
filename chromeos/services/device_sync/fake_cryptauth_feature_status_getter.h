// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_GETTER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_GETTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/services/device_sync/cryptauth_feature_status_getter.h"
#include "chromeos/services/device_sync/cryptauth_feature_status_getter_impl.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"

namespace chromeos {

namespace device_sync {

class CryptAuthClientFactory;

class FakeCryptAuthFeatureStatusGetter : public CryptAuthFeatureStatusGetter {
 public:
  FakeCryptAuthFeatureStatusGetter();
  ~FakeCryptAuthFeatureStatusGetter() override;

  // The RequestContext passed to GetFeatureStatuses(). Returns null if
  // GetFeatureStatuses() has not been called yet.
  const base::Optional<cryptauthv2::RequestContext>& request_context() const {
    return request_context_;
  }

  // The device IDs passed to GetFeatureStatuses(). Returns null if
  // GetFeatureStatuses() has not been called yet.
  const base::Optional<base::flat_set<std::string>>& device_ids() const {
    return device_ids_;
  }

  // Calls OnAttemptFinished() with the same input parameters.
  void FinishAttempt(
      const IdToDeviceSoftwareFeatureInfoMap&
          id_to_device_software_feature_info_map,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code);

 private:
  // CryptAuthFeatureStatusGetter:
  void OnAttemptStarted(const cryptauthv2::RequestContext& request_context,
                        const base::flat_set<std::string>& device_ids) override;

  base::Optional<cryptauthv2::RequestContext> request_context_;
  base::Optional<base::flat_set<std::string>> device_ids_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthFeatureStatusGetter);
};

class FakeCryptAuthFeatureStatusGetterFactory
    : public CryptAuthFeatureStatusGetterImpl::Factory {
 public:
  FakeCryptAuthFeatureStatusGetterFactory();
  ~FakeCryptAuthFeatureStatusGetterFactory() override;

  // Returns a vector of all FakeCryptAuthFeatureStatusGetter instances created
  // by BuildInstance().
  const std::vector<FakeCryptAuthFeatureStatusGetter*>& instances() const {
    return instances_;
  }

  // Returns the most recent CryptAuthClientFactory input into BuildInstance().
  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

 private:
  // CryptAuthFeatureStatusGetterImpl::Factory:
  std::unique_ptr<CryptAuthFeatureStatusGetter> BuildInstance(
      CryptAuthClientFactory* client_factory,
      std::unique_ptr<base::OneShotTimer> timer = nullptr) override;

  std::vector<FakeCryptAuthFeatureStatusGetter*> instances_;
  CryptAuthClientFactory* last_client_factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthFeatureStatusGetterFactory);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_GETTER_H_
