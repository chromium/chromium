// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/cros_state_sender.h"
#include "ash/constants/ash_features.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/message_sender.h"
#include "chromeos/ash/components/phonehub/phone_model.h"
#include "chromeos/ash/components/phonehub/public/cpp/attestation_certificate_generator.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {
namespace phonehub {

namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

// The minimum time to wait before checking whether the phone has responded to
// status messages sent by CrosStateSender, and re-sending the status messages
// if there was no response (no phone status model exists).
constexpr base::TimeDelta kMinimumRetryDelay = base::Seconds(15u);

// The amount the previous delay is multiplied by to determine the new amount
// of time to wait before determining whether CrosStateSender should resend the
// CrOS State. Follows a doubling sequence, e.g 2 sec, 4 sec, 8 sec... etc.
constexpr int kRetryDelayMultiplier = 2;

}  // namespace

CrosStateSender::CrosStateSender(
    MessageSender* message_sender,
    secure_channel::ConnectionManager* connection_manager,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    PhoneModel* phone_model,
    std::unique_ptr<AttestationCertificateGenerator>
        attestation_certificate_generator)
    : CrosStateSender(message_sender,
                      connection_manager,
                      multidevice_setup_client,
                      phone_model,
                      std::make_unique<base::OneShotTimer>(),
                      std::move(attestation_certificate_generator)) {}

CrosStateSender::CrosStateSender(
    MessageSender* message_sender,
    secure_channel::ConnectionManager* connection_manager,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    PhoneModel* phone_model,
    std::unique_ptr<base::OneShotTimer> timer,
    std::unique_ptr<AttestationCertificateGenerator>
        attestation_certificate_generator)
    : message_sender_(message_sender),
      connection_manager_(connection_manager),
      multidevice_setup_client_(multidevice_setup_client),
      phone_model_(phone_model),
      retry_timer_(std::move(timer)),
      retry_delay_(kMinimumRetryDelay),
      attestation_certificate_generator_(
          std::move(attestation_certificate_generator)) {
  DCHECK(message_sender_);
  DCHECK(connection_manager_);
  DCHECK(multidevice_setup_client_);
  DCHECK(phone_model_);
  DCHECK(retry_timer_);

  connection_manager_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);
  if (attestation_certificate_generator_) {
    attestation_certificate_generator_->AddObserver(this);
  }
}

CrosStateSender::~CrosStateSender() {
  connection_manager_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
  if (attestation_certificate_generator_) {
    attestation_certificate_generator_->RemoveObserver(this);
  }
}

void CrosStateSender::AttemptUpdateCrosState() {
  // Stop and cancel old timer if it is running, and reset the |retry_delay_| to
  // |kMinimumRetryDelay|.
  retry_timer_->Stop();
  retry_delay_ = kMinimumRetryDelay;

  // Wait for connection to be established.
  if (connection_manager_->GetStatus() !=
      secure_channel::ConnectionManager::Status::kConnected) {
    PA_LOG(VERBOSE) << "Could not start AttemptUpdateCrosState() because "
                    << "connection manager status is: "
                    << connection_manager_->GetStatus();
    is_certificate_requested_ = false;
    return;
  }

  PerformUpdateCrosState();
}

void CrosStateSender::PerformUpdateCrosState() {
  // If Eche isn't enabled, or there's no way to generate an attestation
  // certificate, send the CrosState message without attestation.
  if (!features::IsEcheSWAEnabled() ||
      attestation_certificate_generator_ == nullptr) {
    SendCrosStateMessage(nullptr);
    return;
  }

  attestation_generating_start_time_ = base::Time::Now();
  is_certificate_requested_ = true;
  attestation_certificate_generator_->RetrieveCertificate();
}

void CrosStateSender::RecordResultMetrics(
    bool is_attestation_certificate_valid) {
  base::UmaHistogramLongTimes(
      is_attestation_certificate_valid
          ? "PhoneHub.Attestation.GeneratingTime"
          : "PhoneHub.Attestation.GeneratingTime.Invalid",
      base::Time::NowFromSystemTime() - attestation_generating_start_time_);
  base::UmaHistogramBoolean("PhoneHub.Attestation.Result",
                            is_attestation_certificate_valid);
}

void CrosStateSender::SendCrosStateMessage(
    const std::vector<std::string>* attestation_certs) {
  bool are_notifications_enabled =
      multidevice_setup_client_->GetFeatureState(
          Feature::kPhoneHubNotifications) == FeatureState::kEnabledByUser;
  bool is_camera_roll_enabled =
      multidevice_setup_client_->GetFeatureState(
          Feature::kPhoneHubCameraRoll) == FeatureState::kEnabledByUser;

  PA_LOG(INFO) << "Attempting to send cros state with notifications enabled "
               << "state as: " << are_notifications_enabled
               << " and camera roll enabled state as: "
               << is_camera_roll_enabled;
  message_sender_->SendCrosState(are_notifications_enabled,
                                 is_camera_roll_enabled, attestation_certs);

  retry_timer_->Start(FROM_HERE, retry_delay_,
                      base::BindOnce(&CrosStateSender::OnRetryTimerFired,
                                     base::Unretained(this)));
}

void CrosStateSender::OnRetryTimerFired() {
  // If the phone status model is non-null, implying that the phone has
  // responded to the previous PerformUpdateCrosState(), or if the
  // connection status is no longer in the connected state, do not
  // retry sending the cros state.
  if (phone_model_->phone_status_model().has_value() ||
      connection_manager_->GetStatus() !=
          secure_channel::ConnectionManager::Status::kConnected) {
    return;
  }

  retry_delay_ *= kRetryDelayMultiplier;
  PerformUpdateCrosState();
}

void CrosStateSender::OnConnectionStatusChanged() {
  AttemptUpdateCrosState();
}

void CrosStateSender::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  AttemptUpdateCrosState();
}

void CrosStateSender::OnCertificateGenerated(
    const std::vector<std::string>& attestation_certs,
    bool is_valid) {
  if (connection_manager_->GetStatus() !=
      secure_channel::ConnectionManager::Status::kConnected) {
    return;
  }

  // Skip sending CrosState update messages when we don't have a valid
  // certificate, unless this is the initial connection or there has been a
  // feature status change.
  if (!is_valid && !is_certificate_requested_) {
    return;
  }

  if (is_certificate_requested_) {
    is_certificate_requested_ = false;
    RecordResultMetrics(is_valid);
  }

  SendCrosStateMessage(&attestation_certs);
}

}  // namespace phonehub
}  // namespace ash
