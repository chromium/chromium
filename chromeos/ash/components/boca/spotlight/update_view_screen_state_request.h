// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_UPDATE_VIEW_SCREEN_STATE_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_UPDATE_VIEW_SCREEN_STATE_REQUEST_H_

#include <memory>
#include <string>

#include "base/types/expected.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "google_apis/common/base_requests.h"

namespace google_apis {
class RequestSender;
enum ApiErrorCode;
}  // namespace google_apis

namespace ash::boca {

struct UpdateViewScreenStateParam {
  std::string teacher_gaia_id;
  std::string teacher_device_id;
  std::string student_gaia_id;
  std::string student_device_id;
  ::boca::ViewScreenConfig::ViewScreenState view_screen_state;
  UpdateViewScreenStateParam(
      std::string teacher_gaia_id_param,
      std::string teacher_device_id_param,
      std::string student_gaia_id_param,
      std::string student_device_id_param,
      ::boca::ViewScreenConfig::ViewScreenState view_screen_state);
  UpdateViewScreenStateParam(UpdateViewScreenStateParam&&);
  UpdateViewScreenStateParam(const UpdateViewScreenStateParam&) = delete;
  UpdateViewScreenStateParam& operator=(const UpdateViewScreenStateParam&) =
      delete;
  UpdateViewScreenStateParam& operator=(UpdateViewScreenStateParam&&);
  ~UpdateViewScreenStateParam();
};

using UpdateViewScreenStateRequestCallback = base::OnceCallback<void(
    base::expected<bool, google_apis::ApiErrorCode> result)>;

class UpdateViewScreenStateRequest : public google_apis::UrlFetchRequestBase {
 public:
  UpdateViewScreenStateRequest(
      google_apis::RequestSender* sender,
      std::string session_id,
      UpdateViewScreenStateParam update_view_screen_state_param,
      std::string url_base,
      UpdateViewScreenStateRequestCallback callback);
  UpdateViewScreenStateRequest(const UpdateViewScreenStateRequest&) = delete;
  UpdateViewScreenStateRequest& operator=(const UpdateViewScreenStateRequest&) =
      delete;
  ~UpdateViewScreenStateRequest() override;

  std::string session_id() const { return session_id_; }
  void set_update_view_screen_state_param(
      UpdateViewScreenStateParam update_view_screen_state_param) {
    update_view_screen_state_param_ = std::move(update_view_screen_state_param);
  }

  // For testing.
  void OverrideURLForTesting(std::string url);

  UpdateViewScreenStateRequestCallback callback() {
    return std::move(callback_);
  }

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
  UpdateViewScreenStateParam update_view_screen_state_param_;
  std::string url_base_;
  UpdateViewScreenStateRequestCallback callback_;
  base::WeakPtrFactory<UpdateViewScreenStateRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_UPDATE_VIEW_SCREEN_STATE_REQUEST_H_
