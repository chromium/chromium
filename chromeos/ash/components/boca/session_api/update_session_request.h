// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_UPDATE_SESSION_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_UPDATE_SESSION_REQUEST_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "google_apis/common/base_requests.h"

namespace google_apis {
class RequestSender;
enum ApiErrorCode;
}  // namespace google_apis

namespace ash::boca {

using UpdateSessionCallback =
    base::OnceCallback<void(base::expected<std::unique_ptr<::boca::Session>,
                                           google_apis::ApiErrorCode> result)>;
// This class performs the request for updating a session
class UpdateSessionRequest : public google_apis::UrlFetchRequestBase {
 public:
  UpdateSessionRequest(google_apis::RequestSender* sender,
                       ::boca::UserIdentity teacher,
                       std::string session_id,
                       UpdateSessionCallback callback);
  UpdateSessionRequest(const UpdateSessionRequest&) = delete;
  UpdateSessionRequest& operator=(const UpdateSessionRequest&) = delete;
  ~UpdateSessionRequest() override;

  const ::boca::UserIdentity& teacher() const { return teacher_; }
  base::TimeDelta* duration() const { return duration_.get(); }
  ::boca::Session::SessionState* session_state() const {
    return session_state_.get();
  }

  ::boca::OnTaskConfig* on_task_config() const { return on_task_config_.get(); }
  ::boca::CaptionsConfig* captions_config() const {
    return captions_config_.get();
  }

  void set_duration(std::unique_ptr<base::TimeDelta> duration) {
    duration_ = std::move(duration);
  }
  void set_session_state(std::unique_ptr<::boca::Session::SessionState> state) {
    session_state_ = std::move(state);
  }
  void set_on_task_config(
      std::unique_ptr<::boca::OnTaskConfig> on_task_content) {
    on_task_config_ = std::move(on_task_content);
  }
  void set_captions_config(
      std::unique_ptr<::boca::CaptionsConfig> captions_config) {
    captions_config_ = std::move(captions_config);
  }

  // For testing.
  void OverrideURLForTesting(std::string url);

  UpdateSessionCallback callback() { return std::move(callback_); }

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
  void OnDataParsed(std::unique_ptr<::boca::Session> session);

  ::boca::UserIdentity teacher_;
  std::string session_id_;
  std::unique_ptr<base::TimeDelta> duration_;
  std::unique_ptr<::boca::Session::SessionState> session_state_;
  std::unique_ptr<::boca::OnTaskConfig> on_task_config_;
  std::unique_ptr<::boca::CaptionsConfig> captions_config_;
  std::string url_base_;
  UpdateSessionCallback callback_;
  base::WeakPtrFactory<UpdateSessionRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_UPDATE_SESSION_REQUEST_H_
