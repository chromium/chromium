// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_session_manager.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_crd_manager.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_notification_handler.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_service.h"

namespace ash::boca {

SpotlightSessionManager::SpotlightSessionManager(
    std::unique_ptr<SpotlightCrdManager> spotlight_crd_manager)
    : notification_handler_(std::make_unique<SpotlightNotificationHandler>()),
      spotlight_service_(std::make_unique<SpotlightService>()),
      spotlight_crd_manager_(std::move(spotlight_crd_manager)) {}
SpotlightSessionManager::SpotlightSessionManager(
    std::unique_ptr<SpotlightNotificationHandler> notification_handler,
    std::unique_ptr<SpotlightCrdManager> spotlight_crd_manager,
    std::unique_ptr<SpotlightService> spotlight_service)
    : notification_handler_(std::move(notification_handler)),
      spotlight_service_(std::move(spotlight_service)),
      spotlight_crd_manager_(std::move(spotlight_crd_manager)) {
  CHECK_IS_TEST();
}
SpotlightSessionManager::~SpotlightSessionManager() = default;

void SpotlightSessionManager::OnSessionStarted(
    const std::string& session_id,
    const ::boca::UserIdentity& producer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  in_session_ = true;
  teacher_email_ = producer.email();
  teacher_name_ = producer.full_name();
}

void SpotlightSessionManager::OnSessionEnded(const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  in_session_ = false;
  request_in_progress_ = false;
  spotlight_crd_manager_->OnSessionEnded();
  teacher_email_ = "";
  teacher_name_ = "";
  notification_handler_->StopSpotlightCountdown();
}

void SpotlightSessionManager::OnConsumerActivityUpdated(
    const std::map<std::string, ::boca::StudentStatus>& activities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ash::features::IsBocaSpotlightEnabled() || !in_session_) {
    return;
  }
  auto student_status = activities.find(BocaAppClient::Get()
                                            ->GetSessionManager()
                                            ->account_id()
                                            .GetGaiaId()
                                            .ToString());
  if (student_status == activities.end()) {
    return;
  }
  auto device = student_status->second.devices().find(
      BocaAppClient::Get()->GetDeviceId());
  if (device == student_status->second.devices().end()) {
    return;
  }

  if (device->second.has_view_screen_config()) {
    switch (device->second.view_screen_config().view_screen_state()) {
      case ::boca::ViewScreenConfig::REQUESTED: {
        if (request_in_progress_) {
          return;
        }
        request_in_progress_ = true;
        std::string requester_email = teacher_email_;
        // ignore the experiment, the correct experiment is not a part of this
        // chain
        if (ash::features::IsBocaSpotlightRobotRequesterEnabled() &&
            device->second.view_screen_config().has_view_screen_requester()) {
          requester_email = device->second.view_screen_config()
                                .view_screen_requester()
                                .service_account()
                                .email();
        }
        spotlight_crd_manager_->InitiateSpotlightSession(
            base::BindOnce(&SpotlightSessionManager::OnConnectionCodeReceived,
                           weak_ptr_factory_.GetWeakPtr()),
            requester_email);
        break;
      }
      case ::boca::ViewScreenConfig::INACTIVE:
        request_in_progress_ = false;
        notification_handler_->StopSpotlightCountdown();
        spotlight_crd_manager_->HidePersistentNotification();
        break;
      default:
        break;
    }
  }
}

void SpotlightSessionManager::OnConnectionCodeReceived(
    const std::string& connection_code) {
  CHECK(spotlight_service_);
  notification_handler_->StartSpotlightCountdownNotification(
      base::BindOnce(&SpotlightSessionManager::OnCountdownEnded,
                     weak_ptr_factory_.GetWeakPtr()));

  spotlight_service_->RegisterScreen(
      connection_code, BocaAppClient::Get()->GetSchoolToolsServerBaseUrl(),
      base::BindOnce(&SpotlightSessionManager::OnRegisterScreenRequestSent,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SpotlightSessionManager::OnRegisterScreenRequestSent(
    base::expected<bool, google_apis::ApiErrorCode> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    boca::RecordOnRegisterScreenRequestSentErrorCode(result.error());
    LOG(WARNING)
        << "[Boca]Failed to send Spotlight connection code with error code: "
        << result.error();
  }
  request_in_progress_ = false;

  // Trigger a session update request so that the view screen state is
  // immediately updated locally.
  BocaAppClient::Get()->GetSessionManager()->LoadCurrentSession(
      /*from_polling=*/false);
}

void SpotlightSessionManager::OnCountdownEnded() {
  spotlight_crd_manager_->ShowPersistentNotification(teacher_name_);
}

}  // namespace ash::boca
