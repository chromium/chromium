// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/session_client_impl.h"

#include "base/task/thread_pool.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/create_session_request.h"
#include "chromeos/ash/components/boca/session_api/update_student_activities_request.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"

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

void SessionClientImpl::GetSession(std::unique_ptr<GetSessionRequest> request) {
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

void SessionClientImpl::UpdateStudentActivity(
    std::unique_ptr<UpdateStudentActivitiesRequest> request) {
  sender_->StartRequestWithAuthRetry(std::move(request));
}

}  // namespace ash::boca
