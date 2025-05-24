// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_VIEW_SCREEN_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_VIEW_SCREEN_REQUEST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/types/expected.h"
#include "google_apis/common/base_requests.h"

namespace google_apis {
class RequestSender;
enum ApiErrorCode;
}  // namespace google_apis

namespace ash::boca {

struct ViewScreenParam {
  std::string teacher_gaia_id;
  std::string teacher_device_id;
  std::optional<std::string> teacher_device_robot_id;
  std::string student_gaia_id;
  std::string student_device_id;
  ViewScreenParam(std::string teacher_gaia_id_param,
                  std::string teacher_device_id_param,
                  std::optional<std::string> teacher_device_robot_id_param,
                  std::string student_gaia_id_param,
                  std::string student_device_id_param);
  ViewScreenParam(ViewScreenParam&&);
  ViewScreenParam(const ViewScreenParam&) = delete;
  ViewScreenParam& operator=(const ViewScreenParam&) = delete;
  ViewScreenParam& operator=(ViewScreenParam&&);
  ~ViewScreenParam();
};

using ViewScreenRequestCallback = base::OnceCallback<void(
    base::expected<bool, google_apis::ApiErrorCode> result)>;

class ViewScreenRequest : public google_apis::UrlFetchRequestBase {
 public:
  ViewScreenRequest(google_apis::RequestSender* sender,
                    std::string session_id,
                    ViewScreenParam view_screen_param,
                    std::string url_base,
                    ViewScreenRequestCallback callback);
  ViewScreenRequest(const ViewScreenRequest&) = delete;
  ViewScreenRequest& operator=(const ViewScreenRequest&) = delete;
  ~ViewScreenRequest() override;

  std::string session_id() const { return session_id_; }
  void set_view_screen_param(ViewScreenParam view_screen_param) {
    view_screen_param_ = std::move(view_screen_param);
  }

  // For testing.
  void OverrideURLForTesting(std::string url);

  ViewScreenRequestCallback callback() { return std::move(callback_); }

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
  ViewScreenParam view_screen_param_;
  std::string url_base_;
  ViewScreenRequestCallback callback_;
  base::WeakPtrFactory<ViewScreenRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_VIEW_SCREEN_REQUEST_H_
