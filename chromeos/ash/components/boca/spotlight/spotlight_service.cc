// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_service.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/spotlight/register_screen_request.h"
#include "chromeos/ash/components/boca/spotlight/update_view_screen_state_request.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"

namespace ash::boca {

SpotlightService::SpotlightService() : sender_(CreateRequestSender()) {}
SpotlightService::SpotlightService(
    std::unique_ptr<google_apis::RequestSender> sender)
    : sender_(std::move(sender)) {}

SpotlightService::~SpotlightService() = default;

std::unique_ptr<google_apis::RequestSender>
SpotlightService::CreateRequestSender() {
  auto url_loader_factory = BocaAppClient::Get()->GetURLLoaderFactory();
  auto* identity_manager = BocaAppClient::Get()->GetIdentityManager();

  if (!identity_manager) {
    return nullptr;
  }
  auto auth_service = std::make_unique<google_apis::AuthService>(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      url_loader_factory,
      signin::OAuthConsumerId::kChromeOsBocaSchoolToolsAuth);

  return std::make_unique<google_apis::RequestSender>(
      std::move(auth_service), url_loader_factory,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           /* `USER_VISIBLE` is because the requested/returned data is visible
               to the user on Web UI surfaces. */
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      /*custom_user_agent=*/"", kTrafficAnnotation);
}

void SpotlightService::ViewScreen(std::string student_gaia_id,
                                  std::string url_base,
                                  ViewScreenRequestCallback callback) {
  auto* const current_session =
      BocaAppClient::Get()->GetSessionManager()->GetCurrentSession();
  if (!current_session) {
    std::move(callback).Run(
        base::unexpected(google_apis::ApiErrorCode::CANCELLED));
    return;
  }
  auto student_device_id =
      ValidateStudentAndGetDeviceId(current_session, student_gaia_id);
  if (!student_device_id.has_value()) {
    std::move(callback).Run(
        base::unexpected(google_apis::ApiErrorCode::CANCELLED));
    return;
  }

  // TODO: crbug.com/386420367 - Support multiple devices. Currently only view
  // screen from first device in student device,
  std::optional<std::string> device_robot_email =
      ash::features::IsBocaSpotlightRobotRequesterEnabled()
          ? std::optional(BocaAppClient::Get()
                              ->GetSessionManager()
                              ->GetDeviceRobotEmail())
          : std::nullopt;
  ViewScreenParam view_screen_param{
      current_session->teacher().gaia_id(), BocaAppClient::Get()->GetDeviceId(),
      std::move(device_robot_email), std::move(student_gaia_id),
      std::move(student_device_id.value())};
  auto view_screen_request = std::make_unique<ViewScreenRequest>(
      sender_.get(), current_session->session_id(),
      std::move(view_screen_param), std::move(url_base), std::move(callback));
  sender_->StartRequestWithAuthRetry(std::move(view_screen_request));
}

void SpotlightService::RegisterScreen(const std::string& connection_code,
                                      std::string url_base,
                                      RegisterScreenRequestCallback callback) {
  auto* const current_session =
      BocaAppClient::Get()->GetSessionManager()->GetCurrentSession();
  if (!current_session) {
    std::move(callback).Run(
        base::unexpected(google_apis::ApiErrorCode::CANCELLED));
    return;
  }

  RegisterScreenParam register_screen_param(
      connection_code,
      BocaAppClient::Get()
          ->GetSessionManager()
          ->account_id()
          .GetGaiaId()
          .ToString(),
      BocaAppClient::Get()->GetDeviceId());
  auto register_screen_request = std::make_unique<RegisterScreenRequest>(
      sender_.get(), current_session->session_id(),
      std::move(register_screen_param), std::move(url_base),
      std::move(callback));
  sender_->StartRequestWithAuthRetry(std::move(register_screen_request));
}

void SpotlightService::UpdateViewScreenState(
    std::string student_gaia_id,
    ::boca::ViewScreenConfig::ViewScreenState view_screen_state,
    std::string url_base,
    UpdateViewScreenStateRequestCallback callback) {
  auto* const current_session =
      BocaAppClient::Get()->GetSessionManager()->GetCurrentSession();
  if (!current_session) {
    std::move(callback).Run(
        base::unexpected(google_apis::ApiErrorCode::CANCELLED));
    return;
  }
  auto student_device_id =
      ValidateStudentAndGetDeviceId(current_session, student_gaia_id);
  if (!student_device_id.has_value()) {
    std::move(callback).Run(
        base::unexpected(google_apis::ApiErrorCode::CANCELLED));
    return;
  }

  // TODO: crbug.com/386420367 - Support multiple devices. Currently only view
  // screen from first device in student device,
  UpdateViewScreenStateParam update_view_screen_param{
      current_session->teacher().gaia_id(), BocaAppClient::Get()->GetDeviceId(),
      std::move(student_gaia_id), std::move(student_device_id.value()),
      std::move(view_screen_state)};
  auto update_view_screen_request =
      std::make_unique<UpdateViewScreenStateRequest>(
          sender_.get(), current_session->session_id(),
          std::move(update_view_screen_param), std::move(url_base),
          std::move(callback));
  sender_->StartRequestWithAuthRetry(std::move(update_view_screen_request));
}

std::optional<std::string> SpotlightService::ValidateStudentAndGetDeviceId(
    ::boca::Session* current_session,
    const std::string& student_gaia_id) {
  if (!current_session->student_statuses().contains(student_gaia_id)) {
    return std::nullopt;
  }
  auto student_devices =
      current_session->student_statuses().at(student_gaia_id).devices();
  if (student_devices.empty()) {
    return std::nullopt;
  }
  return student_devices.begin()->first;
}
}  // namespace ash::boca
