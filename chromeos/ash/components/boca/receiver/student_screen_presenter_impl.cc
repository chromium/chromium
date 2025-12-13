// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/student_screen_presenter_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/receiver/get_kiosk_receiver_request.h"
#include "chromeos/ash/components/boca/receiver/start_kiosk_receiver_request.h"
#include "chromeos/ash/components/boca/receiver/update_kiosk_receiver_state_request.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::boca {
namespace {

std::unique_ptr<google_apis::RequestSender> CreateSender(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
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
      /*custom_user_agent=*/"", traffic_annotation);
}

}  // namespace

StudentScreenPresenterImpl::StudentScreenPresenterImpl(
    std::string_view session_id,
    const ::boca::UserIdentity& teacher_identity,
    std::string_view teacher_device_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : session_id_(session_id),
      teacher_identity_(teacher_identity),
      teacher_device_id_(teacher_device_id),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {}

StudentScreenPresenterImpl::~StudentScreenPresenterImpl() {
  if (start_success_cb_) {
    std::move(start_success_cb_).Run(false);
  }
  NotifyStopSuccess(/*success=*/false);
}

void StudentScreenPresenterImpl::Start(
    std::string_view receiver_id,
    const ::boca::UserIdentity& student_identity,
    std::string_view student_device_id,
    base::OnceCallback<void(bool)> success_cb,
    base::OnceClosure disconnected_cb) {
  if (IsPresenting(/*student_id=*/std::nullopt)) {
    LOG(ERROR) << "[Boca] Trying to present more than one student screen";
    RecordPresentStudentScreenResult(/* failure */ false);
    RecordPresentStudentScreenFailureReason(
        BocaPresentStudentScreenFailureReason::kStudentScreenShareActive);
    std::move(success_cb).Run(false);
    return;
  }
  start_success_cb_ = std::move(success_cb);
  receiver_id_ = receiver_id;
  student_id_ = student_identity.gaia_id();
  disconnected_cb_ = std::move(disconnected_cb);
  start_connection_request_sender_ = CreateSender(
      url_loader_factory_, identity_manager_,
      boca_receiver::StartKioskReceiverRequest::kTrafficAnnotation);
  auto start_request_delegate =
      std::make_unique<boca_receiver::StartKioskReceiverRequest>(
          receiver_id_.value(), teacher_identity_, student_identity,
          teacher_device_id_, std::string(student_device_id),
          /*connection_code=*/std::nullopt, session_id_,
          base::BindOnce(&StudentScreenPresenterImpl::OnStartResponse,
                         weak_ptr_factory_.GetWeakPtr()));
  auto start_request =
      std::make_unique<BocaRequest>(start_connection_request_sender_.get(),
                                    std::move(start_request_delegate));
  start_connection_request_sender_->StartRequestWithAuthRetry(
      std::move(start_request));
}

void StudentScreenPresenterImpl::CheckConnection() {
  if (!connection_id_.has_value() || stop_request_in_progress_) {
    return;
  }
  CHECK(receiver_id_.has_value());
  get_receiver_request_sender_ =
      CreateSender(url_loader_factory_, identity_manager_,
                   boca_receiver::GetKioskReceiverRequest::kTrafficAnnotation);
  auto get_receiver_request_delegate =
      std::make_unique<boca_receiver::GetKioskReceiverRequest>(
          *receiver_id_, *connection_id_,
          base::BindOnce(&StudentScreenPresenterImpl::OnCheckConnectionResponse,
                         weak_ptr_factory_.GetWeakPtr()));
  auto get_receiver_request =
      std::make_unique<BocaRequest>(get_receiver_request_sender_.get(),
                                    std::move(get_receiver_request_delegate));
  get_receiver_request_sender_->StartRequestWithAuthRetry(
      std::move(get_receiver_request));
}

void StudentScreenPresenterImpl::Stop(
    base::OnceCallback<void(bool)> success_cb) {
  if (!IsPresenting(/*student_id=*/std::nullopt)) {
    std::move(success_cb).Run(true);
    return;
  }
  if (!connection_id_.has_value()) {
    // Start presentation is requested but not fulfilled yet.
    std::move(success_cb).Run(false);
    return;
  }
  CHECK(receiver_id_.has_value());

  stop_success_callbacks_.push(std::move(success_cb));
  if (stop_request_in_progress_) {
    return;
  }
  // Ignore any connection check in progress, connection state will be checked
  // on response.
  get_receiver_request_sender_.reset();
  stop_request_in_progress_ = true;
  update_connection_request_sender_ = CreateSender(
      url_loader_factory_, identity_manager_,
      boca_receiver::UpdateKioskReceiverStateRequest::kTrafficAnnotation);
  auto stop_request_delegate =
      std::make_unique<boca_receiver::UpdateKioskReceiverStateRequest>(
          receiver_id_.value(), connection_id_.value(),
          ::boca::ReceiverConnectionState::STOP_REQUESTED,
          base::BindOnce(&StudentScreenPresenterImpl::OnStopResponse,
                         weak_ptr_factory_.GetWeakPtr()));
  auto stop_request =
      std::make_unique<BocaRequest>(update_connection_request_sender_.get(),
                                    std::move(stop_request_delegate));
  update_connection_request_sender_->StartRequestWithAuthRetry(
      std::move(stop_request));
}

bool StudentScreenPresenterImpl::IsPresenting(
    std::optional<std::string_view> student_id) {
  return student_id_.has_value() &&
         (!student_id.has_value() || student_id.value() == student_id_.value());
}

void StudentScreenPresenterImpl::OnStartResponse(
    std::optional<std::string> connection_id) {
  if (!connection_id.has_value()) {
    RecordPresentStudentScreenResult(/* failure */ false);
    RecordPresentStudentScreenFailureReason(
        BocaPresentStudentScreenFailureReason::
            kStartKioskConnectionRequestFailed);
    std::move(start_success_cb_).Run(false);
    Reset();
    return;
  }
  connection_id_ = connection_id;
  RecordPresentStudentScreenResult(/* success */ true);
  std::move(start_success_cb_).Run(true);
}

void StudentScreenPresenterImpl::OnCheckConnectionResponse(
    std::optional<::boca::KioskReceiver> receiver) {
  if (!receiver.has_value() ||
      (receiver->state() != ::boca::ReceiverConnectionState::DISCONNECTED &&
       receiver->state() != ::boca::ReceiverConnectionState::ERROR)) {
    return;
  }
  if (!stop_success_callbacks_.empty()) {
    NotifyStopSuccess(true);
  } else {
    std::move(disconnected_cb_).Run();
  }
  Reset();
}

void StudentScreenPresenterImpl::OnStopResponse(
    std::optional<::boca::ReceiverConnectionState> connection_state) {
  stop_request_in_progress_ = false;
  NotifyStopSuccess(/*success=*/connection_state.has_value());
  if (connection_state.has_value()) {
    Reset();
  }
}

void StudentScreenPresenterImpl::Reset() {
  receiver_id_.reset();
  connection_id_.reset();
  student_id_.reset();
  disconnected_cb_.Reset();
  stop_request_in_progress_ = false;
}

void StudentScreenPresenterImpl::NotifyStopSuccess(bool success) {
  while (!stop_success_callbacks_.empty()) {
    std::move(stop_success_callbacks_.front()).Run(success);
    stop_success_callbacks_.pop();
  }
}

}  // namespace ash::boca
