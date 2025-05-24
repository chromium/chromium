// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_UPDATE_SESSION_CONFIG_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_UPDATE_SESSION_CONFIG_REQUEST_H_

#include <memory>
#include <string>

#include "base/types/expected.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"

namespace google_apis {
class RequestSender;
}  // namespace google_apis

namespace ash::boca {

using UpdateSessionConfigCallback = base::OnceCallback<void(
    base::expected<bool, google_apis::ApiErrorCode> result)>;
// This class performs the request for updating a session
class UpdateSessionConfigRequest : public google_apis::UrlFetchRequestBase {
 public:
  UpdateSessionConfigRequest(google_apis::RequestSender* sender,
                             std::string url_base,
                             ::boca::UserIdentity teacher,
                             std::string session_id,
                             UpdateSessionConfigCallback callback);
  UpdateSessionConfigRequest(const UpdateSessionConfigRequest&) = delete;
  UpdateSessionConfigRequest& operator=(const UpdateSessionConfigRequest&) =
      delete;
  ~UpdateSessionConfigRequest() override;

  const ::boca::UserIdentity& teacher() const { return teacher_; }

  const std::optional<::boca::OnTaskConfig>& on_task_config() {
    return on_task_config_;
  }
  const std::optional<::boca::CaptionsConfig>& captions_config() {
    return captions_config_;
  }
  const std::vector<std::string>& group_ids() { return group_ids_; }

  void set_on_task_config(::boca::OnTaskConfig on_task_content) {
    on_task_config_ = std::move(on_task_content);
  }
  void set_captions_config(::boca::CaptionsConfig captions_config) {
    captions_config_ = std::move(captions_config);
  }
  void set_group_ids(std::vector<std::string> group_ids) {
    group_ids_ = std::move(group_ids);
  }

  // For testing.
  void OverrideURLForTesting(std::string url);

  UpdateSessionConfigCallback callback() { return std::move(callback_); }

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
  void OnDataParsed();

  ::boca::UserIdentity teacher_;
  std::string session_id_;
  std::vector<std::string> group_ids_;
  std::optional<::boca::OnTaskConfig> on_task_config_;
  std::optional<::boca::CaptionsConfig> captions_config_;
  std::string url_base_;
  UpdateSessionConfigCallback callback_;
  base::WeakPtrFactory<UpdateSessionConfigRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_UPDATE_SESSION_CONFIG_REQUEST_H_
