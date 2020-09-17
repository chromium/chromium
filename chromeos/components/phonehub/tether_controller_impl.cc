// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/tether_controller_impl.h"

#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {
namespace phonehub {

TetherControllerImpl::TetherControllerImpl(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : multidevice_setup_client_(multidevice_setup_client) {
  multidevice_setup_client_->AddObserver(this);
}

TetherControllerImpl::~TetherControllerImpl() {
  multidevice_setup_client_->RemoveObserver(this);
}

TetherController::Status TetherControllerImpl::GetStatus() const {
  return status_;
}

void TetherControllerImpl::ScanForAvailableConnection() {
  if (status_ != Status::kConnectionUnavailable) {
    PA_LOG(WARNING) << "Received request to scan for available connection, but "
                    << "a scan cannot be performed because the current status "
                    << "is " << status_;
    return;
  }

  PA_LOG(INFO) << "Scanning for available connection.";
  // TODO(khorimoto): Actually scan for an available connection.
}

void TetherControllerImpl::AttemptConnection() {
  if (status_ != Status::kConnectionUnavailable &&
      status_ != Status::kConnectionAvailable) {
    PA_LOG(WARNING) << "Received request to attempt a connection, but a "
                    << "connection cannot be attempted because the current "
                    << "status is " << status_;
    return;
  }

  PA_LOG(INFO) << "Attempting connection; current status is " << status_;
  // TODO(khorimoto): Actually attempt a connection.
}

void TetherControllerImpl::Disconnect() {
  if (status_ != Status::kConnecting && status_ != Status::kConnected) {
    PA_LOG(WARNING) << "Received request to disconnect, but no connection or "
                    << "connection attempt is in progress. Current status is "
                    << status_;
    return;
  }

  PA_LOG(INFO) << "Attempting disconnection; current status is " << status_;
  // TODO(khorimoto): Actually attempt a connection.
}

}  // namespace phonehub
}  // namespace chromeos
