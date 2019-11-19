// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_NOTIFIER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_NOTIFIER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/services/device_sync/cryptauth_device_notifier.h"
#include "chromeos/services/device_sync/cryptauth_device_notifier_impl.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace device_sync {

class ClientAppMetadataProvider;
class CryptAuthClientFactory;
class CryptAuthGCMManager;

class FakeCryptAuthDeviceNotifier : public CryptAuthDeviceNotifier {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnNotifyDevicesCalled() {}
  };

  struct Request {
    Request(const base::flat_set<std::string>& device_ids,
            cryptauthv2::TargetService target_service,
            CryptAuthFeatureType feature_type,
            base::OnceClosure success_callback,
            base::OnceCallback<void(NetworkRequestError)> error_callback);

    Request(Request&& request);

    ~Request();

    base::flat_set<std::string> device_ids;
    cryptauthv2::TargetService target_service;
    CryptAuthFeatureType feature_type;
    base::OnceClosure success_callback;
    base::OnceCallback<void(NetworkRequestError)> error_callback;
  };

  FakeCryptAuthDeviceNotifier();
  ~FakeCryptAuthDeviceNotifier() override;

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  std::vector<Request>& requests() { return requests_; }

 private:
  // CryptAuthDeviceNotifier:
  void NotifyDevices(
      const base::flat_set<std::string>& device_ids,
      cryptauthv2::TargetService target_service,
      CryptAuthFeatureType feature_type,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) override;

  Delegate* delegate_ = nullptr;
  std::vector<Request> requests_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthDeviceNotifier);
};

class FakeCryptAuthDeviceNotifierFactory
    : public CryptAuthDeviceNotifierImpl::Factory {
 public:
  FakeCryptAuthDeviceNotifierFactory();
  ~FakeCryptAuthDeviceNotifierFactory() override;

  const std::vector<FakeCryptAuthDeviceNotifier*>& instances() const {
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
  // CryptAuthDeviceNotifierImpl::Factory:
  std::unique_ptr<CryptAuthDeviceNotifier> BuildInstance(
      ClientAppMetadataProvider* client_app_metadata_provider,
      CryptAuthClientFactory* client_factory,
      CryptAuthGCMManager* gcm_manager,
      std::unique_ptr<base::OneShotTimer> timer = nullptr) override;

  std::vector<FakeCryptAuthDeviceNotifier*> instances_;
  ClientAppMetadataProvider* last_client_app_metadata_provider_ = nullptr;
  CryptAuthClientFactory* last_client_factory_ = nullptr;
  CryptAuthGCMManager* last_gcm_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthDeviceNotifierFactory);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_NOTIFIER_H_
