// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_GETTER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_GETTER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_status_getter.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_status_getter_impl.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"

namespace ash {

namespace device_sync {

class CryptAuthClientFactory;

class FakeCryptAuthFeatureStatusGetter : public CryptAuthFeatureStatusGetter {
 public:
  FakeCryptAuthFeatureStatusGetter();

  FakeCryptAuthFeatureStatusGetter(const FakeCryptAuthFeatureStatusGetter&) =
      delete;
  FakeCryptAuthFeatureStatusGetter& operator=(
      const FakeCryptAuthFeatureStatusGetter&) = delete;

  ~FakeCryptAuthFeatureStatusGetter() override;

  // The RequestContext passed to GetFeatureStatuses(). Returns null if
  // GetFeatureStatuses() has not been called yet.
  const std::optional<cryptauthv2::RequestContext>& request_context() const {
    return request_context_;
  }

  // The device IDs passed to GetFeatureStatuses(). Returns null if
  // GetFeatureStatuses() has not been called yet.
  const std::optional<base::flat_set<std::string>>& device_ids() const {
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

  std::optional<cryptauthv2::RequestContext> request_context_;
  std::optional<base::flat_set<std::string>> device_ids_;
};

class FakeCryptAuthFeatureStatusGetterFactory
    : public CryptAuthFeatureStatusGetterImpl::Factory {
 public:
  FakeCryptAuthFeatureStatusGetterFactory();

  FakeCryptAuthFeatureStatusGetterFactory(
      const FakeCryptAuthFeatureStatusGetterFactory&) = delete;
  FakeCryptAuthFeatureStatusGetterFactory& operator=(
      const FakeCryptAuthFeatureStatusGetterFactory&) = delete;

  ~FakeCryptAuthFeatureStatusGetterFactory() override;

  // Returns a vector of all FakeCryptAuthFeatureStatusGetter instances created
  // by CreateInstance().
  const std::vector<
      raw_ptr<FakeCryptAuthFeatureStatusGetter, VectorExperimental>>&
  instances() const {
    return instances_;
  }

  // Returns the most recent CryptAuthClientFactory input into CreateInstance().
  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

 private:
  // CryptAuthFeatureStatusGetterImpl::Factory:
  std::unique_ptr<CryptAuthFeatureStatusGetter> CreateInstance(
      CryptAuthClientFactory* client_factory,
      std::unique_ptr<base::OneShotTimer> timer) override;

  std::vector<raw_ptr<FakeCryptAuthFeatureStatusGetter, VectorExperimental>>
      instances_;
  raw_ptr<CryptAuthClientFactory> last_client_factory_ = nullptr;
};

}  // namespace device_sync

}  // namespace ash

#endif  //  CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_GETTER_H_
