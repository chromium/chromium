// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/session_client_impl.h"

#include "ash/constants/ash_features.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/session_api/add_students_request.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/create_session_request.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/join_session_request.h"
#include "chromeos/ash/components/boca/session_api/remove_student_request.h"
#include "chromeos/ash/components/boca/session_api/renotify_student_request.h"
#include "chromeos/ash/components/boca/session_api/update_session_config_request.h"
#include "chromeos/ash/components/boca/session_api/update_session_request.h"
#include "chromeos/ash/components/boca/session_api/update_student_activities_request.h"
#include "chromeos/ash/components/boca/session_api/upload_token_request.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "student_heartbeat_request.h"

namespace ash::boca {

SessionClientImpl::SessionClientImpl() : sender_(CreateRequestSender()) {}
SessionClientImpl::SessionClientImpl(
    std::unique_ptr<google_apis::RequestSender> sender)
    : sender_(std::move(sender)) {}

SessionClientImpl::~SessionClientImpl() = default;

std::unique_ptr<google_apis::RequestSender>
SessionClientImpl::CreateRequestSender() {
  std::vector<std::string> scopes = {kSchoolToolsAuthScope};
  auto url_loader_factory = BocaAppClient::Get()->GetURLLoaderFactory();
  auto* identity_manager = BocaAppClient::Get()->GetIdentityManager();

  auto auth_service = std::make_unique<google_apis::AuthService>(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      url_loader_factory, scopes);

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

void SessionClientImpl::CreateSession(
    std::unique_ptr<CreateSessionRequest> request) {
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::GetSession(std::unique_ptr<GetSessionRequest> request,
                                   bool can_skip_duplicate_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (features::IsBocaSequentialSessionLoadEnabled()) {
    if (has_blocking_get_session_request_) {
      if (!can_skip_duplicate_request ||
          pending_get_session_requests_.size() == 0) {
        pending_get_session_requests_.push(std::move(request));
      }
      return;
    }
    request->set_callback(
        base::BindOnce(&SessionClientImpl::OnGetSessionCompleted,
                       weak_ptr_factory_.GetWeakPtr(), request->callback()));
    has_blocking_get_session_request_ = true;
  }
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::UploadToken(
    std::unique_ptr<UploadTokenRequest> request) {
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::UpdateSession(
    std::unique_ptr<UpdateSessionRequest> request) {
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::UpdateSessionConfig(
    std::unique_ptr<UpdateSessionConfigRequest> request) {
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::UpdateStudentActivity(
    std::unique_ptr<UpdateStudentActivitiesRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (features::IsBocaSequentialInsertActivityEnabled()) {
    if (has_blocking_update_activity_request_) {
      pending_update_student_activity_request_ = std::move(request);
      return;
    }

    request->set_callback(
        base::BindOnce(&SessionClientImpl::OnInsertStudentActivityCompleted,
                       weak_ptr_factory_.GetWeakPtr(), request->callback()));
    has_blocking_update_activity_request_ = true;
  }
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::RemoveStudent(
    std::unique_ptr<RemoveStudentRequest> request) {
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::AddStudents(
    std::unique_ptr<AddStudentsRequest> request) {
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::JoinSession(
    std::unique_ptr<JoinSessionRequest> request) {
  sender_->StartRequestWithAuthRetry(std::move(request));
}
void SessionClientImpl::StudentHeartbeat(
    std::unique_ptr<StudentHeartbeatRequest> request) {
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::RenotifyStudent(
    std::unique_ptr<RenotifyStudentRequest> request) {
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::OnInsertStudentActivityCompleted(
    UpdateStudentActivitiesCallback callback,
    base::expected<bool, google_apis::ApiErrorCode> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_blocking_update_activity_request_ = false;
  std::move(callback).Run(std::move(result));
  if (!pending_update_student_activity_request_) {
    return;
  }
  auto request = std::move(pending_update_student_activity_request_);
  request->set_callback(
      base::BindOnce(&SessionClientImpl::OnInsertStudentActivityCompleted,
                     weak_ptr_factory_.GetWeakPtr(), request->callback()));
  sender_->StartRequestWithAuthRetry(std::move(request));
}

void SessionClientImpl::OnGetSessionCompleted(
    GetSessionCallback callback,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_blocking_get_session_request_ = false;
  std::move(callback).Run(std::move(result));
  if (pending_get_session_requests_.empty()) {
    return;
  }
  auto request = std::move(pending_get_session_requests_.front());
  pending_get_session_requests_.pop();
  request->set_callback(
      base::BindOnce(&SessionClientImpl::OnGetSessionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), request->callback()));
  sender_->StartRequestWithAuthRetry(std::move(request));
}

}  // namespace ash::boca
