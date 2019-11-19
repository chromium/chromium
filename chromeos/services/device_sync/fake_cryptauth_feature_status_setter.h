// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_SETTER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_SETTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/services/device_sync/cryptauth_feature_status_setter.h"
#include "chromeos/services/device_sync/cryptauth_feature_status_setter_impl.h"
#include "chromeos/services/device_sync/network_request_error.h"

namespace chromeos {

namespace device_sync {

class ClientAppMetadataProvider;
class CryptAuthClientFactory;
class CryptAuthGCMManager;

class FakeCryptAuthFeatureStatusSetter : public CryptAuthFeatureStatusSetter {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnSetFeatureStatusCalled() {}
  };

  struct Request {
    Request(const std::string& device_id,
            multidevice::SoftwareFeature feature,
            FeatureStatusChange status_change,
            base::OnceClosure success_callback,
            base::OnceCallback<void(NetworkRequestError)> error_callback);

    Request(Request&& request);

    ~Request();

    const std::string device_id;
    const multidevice::SoftwareFeature feature;
    const FeatureStatusChange status_change;
    base::OnceClosure success_callback;
    base::OnceCallback<void(NetworkRequestError)> error_callback;
  };

  FakeCryptAuthFeatureStatusSetter();
  ~FakeCryptAuthFeatureStatusSetter() override;

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  std::vector<Request>& requests() { return requests_; }

 private:
  // CryptAuthFeatureStatusSetter:
  void SetFeatureStatus(
      const std::string& device_id,
      multidevice::SoftwareFeature feature,
      FeatureStatusChange status_change,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) override;

  Delegate* delegate_ = nullptr;
  std::vector<Request> requests_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthFeatureStatusSetter);
};

class FakeCryptAuthFeatureStatusSetterFactory
    : public CryptAuthFeatureStatusSetterImpl::Factory {
 public:
  FakeCryptAuthFeatureStatusSetterFactory();
  ~FakeCryptAuthFeatureStatusSetterFactory() override;

  const std::vector<FakeCryptAuthFeatureStatusSetter*>& instances() const {
    return instances_;
  }

  const ClientAppMetadataProvider* last_client_app_metadata_provider() const {
    return last_client_app_metadata_provider_;
  }

  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

  const CryptAuthGCMManager* last_gcm_manager() const {
    return last_gcm_manager_;
  }

 private:
  // CryptAuthFeatureStatusSetterImpl::Factory:
  std::unique_ptr<CryptAuthFeatureStatusSetter> BuildInstance(
      ClientAppMetadataProvider* client_app_metadata_provider,
      CryptAuthClientFactory* client_factory,
      CryptAuthGCMManager* gcm_manager,
      std::unique_ptr<base::OneShotTimer> timer = nullptr) override;

  std::vector<FakeCryptAuthFeatureStatusSetter*> instances_;
  ClientAppMetadataProvider* last_client_app_metadata_provider_ = nullptr;
  CryptAuthClientFactory* last_client_factory_ = nullptr;
  CryptAuthGCMManager* last_gcm_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthFeatureStatusSetterFactory);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_FEATURE_STATUS_SETTER_H_
