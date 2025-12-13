// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/teacher_screen_presenter_impl.h"

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
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/shared_crd_session_wrapper.h"
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

TeacherScreenPresenterImpl::TeacherScreenPresenterImpl(
    std::string_view teacher_device_id,
    std::unique_ptr<SharedCrdSessionWrapper> shared_crd_session,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : teacher_device_id_(teacher_device_id),
      shared_crd_session_(std::move(shared_crd_session)),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {}

TeacherScreenPresenterImpl::~TeacherScreenPresenterImpl() {
  if (start_success_cb_) {
    std::move(start_success_cb_).Run(false);
  }
}

void TeacherScreenPresenterImpl::Start(
    std::string_view receiver_id,
    std::string_view receiver_name,
    ::boca::UserIdentity teacher_identity,
    const bool is_session_active,
    base::OnceCallback<void(bool)> success_cb,
    base::OnceClosure disconnected_cb) {
  if (IsPresenting()) {
    LOG(ERROR) << "[Boca] Trying to start more than one screen sharing";
    RecordPresentOwnScreenResult(/* failure */ false, is_session_active);
    RecordPresentOwnScreenFailureReason(
        BocaPresentOwnScreenFailureReason::kTeacherScreenShareActive,
        is_session_active);
    std::move(success_cb).Run(false);
    return;
  }
  receiver_id_ = receiver_id;
  receiver_name_ = receiver_name;
  is_session_active_ = is_session_active;
  start_success_cb_ = std::move(success_cb);
  disconnected_cb_ = std::move(disconnected_cb);

  get_receiver_request_sender_ =
      CreateSender(url_loader_factory_, identity_manager_,
                   boca_receiver::GetKioskReceiverRequest::kTrafficAnnotation);
  auto get_receiver_request_delegate =
      std::make_unique<boca_receiver::GetKioskReceiverRequest>(
          *receiver_id_, /*connection_id=*/std::nullopt,
          base::BindOnce(&TeacherScreenPresenterImpl::OnGetReceiverResponse,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(teacher_identity)));
  auto get_receiver_request =
      std::make_unique<BocaRequest>(get_receiver_request_sender_.get(),
                                    std::move(get_receiver_request_delegate));
  get_receiver_request_sender_->StartRequestWithAuthRetry(
      std::move(get_receiver_request));
}

void TeacherScreenPresenterImpl::Stop(
    base::OnceCallback<void(bool)> success_cb) {
  if (start_success_cb_) {
    LOG(ERROR) << "[Boca] Cannot stop while start is still in progress";
    std::move(success_cb).Run(false);
    return;
  }
  notification_handler_.HandleScreenShareEndedNotification(
      message_center::MessageCenter::Get());

  if (!IsPresenting()) {
    std::move(success_cb).Run(true);
    return;
  }
  // No need to update server, receiver will do automatically on CRD session
  // termination.
  shared_crd_session_->TerminateSession();
  std::move(success_cb).Run(true);
  Reset();
}

bool TeacherScreenPresenterImpl::IsPresenting() {
  return receiver_id_.has_value();
}

void TeacherScreenPresenterImpl::OnGetReceiverResponse(
    ::boca::UserIdentity teacher_identity,
    std::optional<::boca::KioskReceiver> receiver) {
  if (!receiver) {
    RecordPresentOwnScreenResult(/* failure */ false, is_session_active_);
    RecordPresentOwnScreenFailureReason(
        BocaPresentOwnScreenFailureReason::kGetReceiverRequestFailed,
        is_session_active_);
    std::move(start_success_cb_).Run(false);
    Reset();
    return;
  }
  shared_crd_session_->StartCrdHost(
      receiver->robot_email(),
      base::BindOnce(&TeacherScreenPresenterImpl::OnGetCrdConnectionCode,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(teacher_identity)),
      base::BindOnce(&TeacherScreenPresenterImpl::OnCrdSessionFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&TeacherScreenPresenterImpl::OnCrdSessionFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TeacherScreenPresenterImpl::OnGetCrdConnectionCode(
    ::boca::UserIdentity teacher_identity,
    const std::string& connection_code) {
  start_connection_request_sender_ = CreateSender(
      url_loader_factory_, identity_manager_,
      boca_receiver::StartKioskReceiverRequest::kTrafficAnnotation);
  auto start_request_delegate =
      std::make_unique<boca_receiver::StartKioskReceiverRequest>(
          receiver_id_.value(), teacher_identity, teacher_identity,
          teacher_device_id_, teacher_device_id_, connection_code,
          /*session_id_=*/std::nullopt,
          base::BindOnce(&TeacherScreenPresenterImpl::OnStartReceiverResponse,
                         weak_ptr_factory_.GetWeakPtr()));
  auto start_request =
      std::make_unique<BocaRequest>(start_connection_request_sender_.get(),
                                    std::move(start_request_delegate));
  start_connection_request_sender_->StartRequestWithAuthRetry(
      std::move(start_request));
}

void TeacherScreenPresenterImpl::OnStartReceiverResponse(
    std::optional<std::string> connection_id) {
  if (!start_success_cb_) {
    return;
  }
  if (!connection_id) {
    RecordPresentOwnScreenResult(/* failure */ false, is_session_active_);
    RecordPresentOwnScreenFailureReason(
        BocaPresentOwnScreenFailureReason::kStartKioskConnectionRequestFailed,
        is_session_active_);
    std::move(start_success_cb_).Run(false);
    Reset();
    return;
  }
  RecordPresentOwnScreenResult(/* success */ true, is_session_active_);
  notification_handler_.HandleScreenShareStartedNotification(
      message_center::MessageCenter::Get(), receiver_name_.value());
  std::move(start_success_cb_).Run(true);
}

void TeacherScreenPresenterImpl::OnCrdSessionFinished() {
  if (start_success_cb_) {
    RecordPresentOwnScreenResult(/* failure */ false, is_session_active_);
    RecordPresentOwnScreenFailureReason(
        BocaPresentOwnScreenFailureReason::kGetCrdConnectionCodeFailed,
        is_session_active_);
    std::move(start_success_cb_).Run(false);
  } else if (disconnected_cb_) {
    std::move(disconnected_cb_).Run();
  }
  Reset();
}

void TeacherScreenPresenterImpl::Reset() {
  receiver_id_.reset();
  disconnected_cb_.Reset();
}

}  // namespace ash::boca
