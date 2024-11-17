// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_UPDATE_STUDENT_ACTIVITIES_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_UPDATE_STUDENT_ACTIVITIES_REQUEST_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "google_apis/common/base_requests.h"

namespace google_apis {
class RequestSender;
enum ApiErrorCode;
}  // namespace google_apis

namespace ash::boca {

using UpdateStudentActivitiesCallback = base::OnceCallback<void(
    base::expected<bool, google_apis::ApiErrorCode> result)>;

class UpdateStudentActivitiesRequest : public google_apis::UrlFetchRequestBase {
 public:
  UpdateStudentActivitiesRequest(google_apis::RequestSender* sender,
                                 std::string session_id,
                                 std::string gaia_id,
                                 std::string device_id,
                                 UpdateStudentActivitiesCallback callback);
  UpdateStudentActivitiesRequest(const UpdateStudentActivitiesRequest&) =
      delete;
  UpdateStudentActivitiesRequest& operator=(
      const UpdateStudentActivitiesRequest&) = delete;
  ~UpdateStudentActivitiesRequest() override;

  std::string session_id() const { return session_id_; }
  std::string gaia_id() const { return gaia_id_; }
  std::string device_id() const { return device_id_; }
  std::string active_tab_title() const { return active_tab_title_; }

  void set_active_tab_title(std::string active_tab_title) {
    active_tab_title_ = std::move(active_tab_title);
  }

  // For testing.
  void OverrideURLForTesting(std::string url);

  UpdateStudentActivitiesCallback callback() { return std::move(callback_); }

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

  std::string session_id_;
  std::string gaia_id_;
  std::string device_id_;
  std::string active_tab_title_;
  std::string url_base_;
  UpdateStudentActivitiesCallback callback_;
  base::WeakPtrFactory<UpdateStudentActivitiesRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_UPDATE_STUDENT_ACTIVITIES_REQUEST_H_
