// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/services/device_sync/device_sync_base.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_driver.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kDummyAppName[] = "DeviceSyncDummyApp";

class DummyGCMAppHandler : public gcm::GCMAppHandler {
 public:
  explicit DummyGCMAppHandler(base::OnceClosure shutdown_callback)
      : shutdown_callback_(std::move(shutdown_callback)) {}
  ~DummyGCMAppHandler() override = default;

  // gcm::GCMAppHandler:
  void ShutdownHandler() override { std::move(shutdown_callback_).Run(); }

  void OnStoreReset() override {}
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override {}
  void OnMessagesDeleted(const std::string& app_id) override {}
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override {}
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override {}

 private:
  base::OnceClosure shutdown_callback_;
};

}  // namespace

DeviceSyncBase::DeviceSyncBase(gcm::GCMDriver* gcm_driver)
    : gcm_app_handler_(std::make_unique<DummyGCMAppHandler>(
          base::BindOnce(&DeviceSyncBase::Shutdown, base::Unretained(this)))) {
  if (gcm_driver)
    gcm_driver->AddAppHandler(kDummyAppName, gcm_app_handler_.get());
  bindings_.set_connection_error_handler(base::BindRepeating(
      &DeviceSyncBase::OnDisconnection, base::Unretained(this)));
}

DeviceSyncBase::~DeviceSyncBase() = default;

void DeviceSyncBase::AddObserver(mojom::DeviceSyncObserverPtr observer,
                                 AddObserverCallback callback) {
  observers_.AddPtr(std::move(observer));
  std::move(callback).Run();
}

void DeviceSyncBase::BindRequest(mojom::DeviceSyncRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void DeviceSyncBase::NotifyOnEnrollmentFinished() {
  observers_.ForAllPtrs(
      [](auto* observer) { observer->OnEnrollmentFinished(); });
}

void DeviceSyncBase::NotifyOnNewDevicesSynced() {
  observers_.ForAllPtrs([](auto* observer) { observer->OnNewDevicesSynced(); });
}

void DeviceSyncBase::OnDisconnection() {
  // If all clients have disconnected, shut down.
  if (bindings_.empty())
    Shutdown();
}

}  // namespace device_sync

}  // namespace chromeos
