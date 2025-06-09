// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_SESSION_CLIENT_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_SESSION_CLIENT_IMPL_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "student_heartbeat_request.h"

namespace boca {
class Session;
}

namespace google_apis {
class RequestSender;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}

namespace ash::boca {
class RemoveStudentRequest;
class CreateSessionRequest;
class GetSessionRequest;
class UpdateSessionRequest;
class UpdateStudentActivitiesRequest;
class UploadTokenRequest;
class JoinSessionRequest;
class StudentHeartbeatRequest;
class AddStudentsRequest;
class UpdateSessionConfigRequest;
class RenotifyStudentRequest;

class SessionClientImpl {
 public:
  using GetSessionCallback = base::OnceCallback<void(
      base::expected<std::unique_ptr<::boca::Session>,
                     google_apis::ApiErrorCode> result)>;
  using UpdateStudentActivitiesCallback = base::OnceCallback<void(
      base::expected<bool, google_apis::ApiErrorCode> result)>;

  SessionClientImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  explicit SessionClientImpl(
      std::unique_ptr<google_apis::RequestSender> sender);
  SessionClientImpl(const SessionClientImpl&) = delete;
  SessionClientImpl& operator=(const SessionClientImpl&) = delete;
  virtual ~SessionClientImpl();

  virtual std::unique_ptr<google_apis::RequestSender> CreateRequestSender();
  virtual void CreateSession(std::unique_ptr<CreateSessionRequest> request);
  virtual void GetSession(std::unique_ptr<GetSessionRequest> request,
                          bool can_skip_duplicate_request);
  virtual void UploadToken(std::unique_ptr<UploadTokenRequest> request);
  virtual void UpdateSession(std::unique_ptr<UpdateSessionRequest> request);
  virtual void UpdateSessionConfig(
      std::unique_ptr<UpdateSessionConfigRequest> request);
  virtual void UpdateStudentActivity(
      std::unique_ptr<UpdateStudentActivitiesRequest> request);
  virtual void RemoveStudent(std::unique_ptr<RemoveStudentRequest> request);
  virtual void AddStudents(std::unique_ptr<AddStudentsRequest> request);
  virtual void JoinSession(std::unique_ptr<JoinSessionRequest> request);
  virtual void StudentHeartbeat(
      std::unique_ptr<StudentHeartbeatRequest> request);
  virtual void RenotifyStudent(std::unique_ptr<RenotifyStudentRequest> request);

  google_apis::RequestSender* sender() { return sender_.get(); }

 private:
  void OnInsertStudentActivityCompleted(
      UpdateStudentActivitiesCallback callback,
      base::expected<bool, google_apis::ApiErrorCode> result);
  void OnGetSessionCompleted(GetSessionCallback callback,
                             base::expected<std::unique_ptr<::boca::Session>,
                                            google_apis::ApiErrorCode> result);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<google_apis::RequestSender> sender_;
  base::queue<std::unique_ptr<GetSessionRequest>> pending_get_session_requests_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool has_blocking_get_session_request_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  bool has_blocking_update_activity_request_
      GUARDED_BY_CONTEXT(sequence_checker_) = false;
  std::unique_ptr<UpdateStudentActivitiesRequest>
      pending_update_student_activity_request_;
  base::WeakPtrFactory<SessionClientImpl> weak_ptr_factory_{this};
};
}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_SESSION_CLIENT_IMPL_H_
