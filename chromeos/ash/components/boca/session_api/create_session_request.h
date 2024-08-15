// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_CREATE_SESSION_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_CREATE_SESSION_REQUEST_H_

#include <memory>
#include <string>

#include "base/types/expected.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "google_apis/common/base_requests.h"

namespace google_apis {
class RequestSender;
enum ApiErrorCode;
}  // namespace google_apis

namespace ash::boca {

//=================CreateSessionRequest================

using CreateSessionCallback = base::OnceCallback<void(
    base::expected<bool, google_apis::ApiErrorCode> result)>;
// This class performs the request for creating a session
class CreateSessionRequest : public google_apis::UrlFetchRequestBase {
 public:
  CreateSessionRequest(google_apis::RequestSender* sender,
                       std::string gaia_id,
                       int duration_in_sec,
                       ::boca::Session::SessionState session_state,
                       CreateSessionCallback callback);
  CreateSessionRequest(const CreateSessionRequest&) = delete;
  CreateSessionRequest& operator=(const CreateSessionRequest&) = delete;
  ~CreateSessionRequest() override;

  const std::string& teacher_gaia_id() const { return teacher_gaia_id_; }
  const int& duration() const { return duration_; }
  const ::boca::Session::SessionState& session_state() {
    return session_state_;
  }

  std::vector<::boca::UserIdentity> student_groups() const {
    return student_groups_;
  }
  ::boca::OnTaskConfig* on_task_config() const { return on_task_config_.get(); }
  ::boca::CaptionsConfig* captions_config() const {
    return captions_config_.get();
  }

  // For testing.
  void OverrideURLForTesting(std::string url);

  void set_student_groups(std::vector<::boca::UserIdentity> student_groups) {
    student_groups_ = std::move(student_groups);
  }

  void set_on_task_config(
      std::unique_ptr<::boca::OnTaskConfig> on_task_content) {
    on_task_config_ = std::move(on_task_content);
  }

  void set_captions_config(
      std::unique_ptr<::boca::CaptionsConfig> captions_config) {
    captions_config_ = std::move(captions_config);
  }

  CreateSessionCallback callback() { return std::move(callback_); }

 protected:
  // UrlFetchRequestBase:
  google_apis::HttpRequestMethod GetRequestType() const override;
  GURL GetURL() const override;
  google_apis::ApiErrorCode MapReasonToError(
      google_apis::ApiErrorCode code,
      const std::string& reason) override;
  bool IsSuccessfulErrorCode(google_apis::ApiErrorCode error) override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(google_apis::ApiErrorCode code) override;

 private:
  void OnDataParsed(bool success);

  std::string teacher_gaia_id_;
  int duration_;
  ::boca::Session::SessionState session_state_;

  std::vector<::boca::UserIdentity> student_groups_;

  std::unique_ptr<::boca::OnTaskConfig> on_task_config_;

  std::unique_ptr<::boca::CaptionsConfig> captions_config_;

  std::string url_base_;

  CreateSessionCallback callback_;

  base::WeakPtrFactory<CreateSessionRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_CREATE_SESSION_REQUEST_H_
