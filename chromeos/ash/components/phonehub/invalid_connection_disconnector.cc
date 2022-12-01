// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/invalid_connection_disconnector.h"

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/phone_model.h"

namespace ash {
namespace phonehub {
namespace {

// The grace period time for the phone status model to remain non-empty when
// the phone is connected. If the phone status model is still empty after this
// period of time, ConnectionManager should call Disconnect().
constexpr base::TimeDelta kEmptyPhoneStatusModelGracePeriodTimeDelta =
    base::Seconds(45);

}  // namespace

InvalidConnectionDisconnector::InvalidConnectionDisconnector(
    secure_channel::ConnectionManager* connection_manager,
    PhoneModel* phone_model)
    : InvalidConnectionDisconnector(connection_manager,
                                    phone_model,
                                    std::make_unique<base::OneShotTimer>()) {}

InvalidConnectionDisconnector::InvalidConnectionDisconnector(
    secure_channel::ConnectionManager* connection_manager,
    PhoneModel* phone_model,
    std::unique_ptr<base::OneShotTimer> timer)
    : connection_manager_(connection_manager),
      phone_model_(phone_model),
      timer_(std::move(timer)) {
  connection_manager_->AddObserver(this);
}

InvalidConnectionDisconnector::~InvalidConnectionDisconnector() {
  connection_manager_->RemoveObserver(this);
}

void InvalidConnectionDisconnector::OnConnectionStatusChanged() {
  timer_->AbandonAndStop();

  if (IsPhoneConnected() && !DoesPhoneStatusModelExist()) {
    timer_->Start(FROM_HERE, kEmptyPhoneStatusModelGracePeriodTimeDelta,
                  base::BindOnce(&InvalidConnectionDisconnector::OnTimerFired,
                                 base::Unretained(this)));
  }
}

void InvalidConnectionDisconnector::OnTimerFired() {
  if (!IsPhoneConnected() || DoesPhoneStatusModelExist())
    return;

  PA_LOG(INFO) << "Grace period ended for an empty PhoneStatusModel while in "
                  "the connected state; disconnecting from phone";

  connection_manager_->Disconnect();
}

bool InvalidConnectionDisconnector::IsPhoneConnected() const {
  return connection_manager_->GetStatus() ==
         secure_channel::ConnectionManager::Status::kConnected;
}

bool InvalidConnectionDisconnector::DoesPhoneStatusModelExist() const {
  return phone_model_->phone_status_model().has_value();
}

}  // namespace phonehub
}  // namespace ash
