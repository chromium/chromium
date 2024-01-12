// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_NOTIFIER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_NOTIFIER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_notifier.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_notifier_impl.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"

namespace ash {

namespace device_sync {

class CryptAuthClientFactory;

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

  FakeCryptAuthDeviceNotifier(const FakeCryptAuthDeviceNotifier&) = delete;
  FakeCryptAuthDeviceNotifier& operator=(const FakeCryptAuthDeviceNotifier&) =
      delete;

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

  raw_ptr<Delegate> delegate_ = nullptr;
  std::vector<Request> requests_;
};

class FakeCryptAuthDeviceNotifierFactory
    : public CryptAuthDeviceNotifierImpl::Factory {
 public:
  FakeCryptAuthDeviceNotifierFactory();

  FakeCryptAuthDeviceNotifierFactory(
      const FakeCryptAuthDeviceNotifierFactory&) = delete;
  FakeCryptAuthDeviceNotifierFactory& operator=(
      const FakeCryptAuthDeviceNotifierFactory&) = delete;

  ~FakeCryptAuthDeviceNotifierFactory() override;

  const std::vector<raw_ptr<FakeCryptAuthDeviceNotifier, VectorExperimental>>&
  instances() const {
    return instances_;
  }

  const std::string& last_instance_id() const { return last_instance_id_; }

  const std::string& last_instance_id_token() const {
    return last_instance_id_token_;
  }

  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

 private:
  // CryptAuthDeviceNotifierImpl::Factory:
  std::unique_ptr<CryptAuthDeviceNotifier> CreateInstance(
      const std::string& instance_id,
      const std::string& instance_id_token,
      CryptAuthClientFactory* client_factory,
      std::unique_ptr<base::OneShotTimer> timer) override;

  std::vector<raw_ptr<FakeCryptAuthDeviceNotifier, VectorExperimental>>
      instances_;
  std::string last_instance_id_;
  std::string last_instance_id_token_;
  raw_ptr<CryptAuthClientFactory, DanglingUntriaged> last_client_factory_ =
      nullptr;
};

}  // namespace device_sync

}  // namespace ash

#endif  //  CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_NOTIFIER_H_
