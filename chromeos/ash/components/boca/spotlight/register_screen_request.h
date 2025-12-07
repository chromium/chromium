// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_REGISTER_SCREEN_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_REGISTER_SCREEN_REQUEST_H_

#include <memory>
#include <string>

#include "base/types/expected.h"
#include "google_apis/common/base_requests.h"

namespace google_apis {
class RequestSender;
enum ApiErrorCode;
}  // namespace google_apis

namespace ash::boca {

struct RegisterScreenParam {
  std::string connection_code;
  std::string student_gaia_id;
  std::string student_device_id;
  RegisterScreenParam(std::string connection_code_param,
                      std::string student_gaia_id_param,
                      std::string student_device_id_param);
  RegisterScreenParam(RegisterScreenParam&&);
  RegisterScreenParam(const RegisterScreenParam&) = delete;
  RegisterScreenParam& operator=(const RegisterScreenParam&) = delete;
  RegisterScreenParam& operator=(RegisterScreenParam&&);
  ~RegisterScreenParam();
};

using RegisterScreenRequestCallback = base::OnceCallback<void(
    base::expected<bool, google_apis::ApiErrorCode> result)>;

class RegisterScreenRequest : public google_apis::UrlFetchRequestBase {
 public:
  RegisterScreenRequest(google_apis::RequestSender* sender,
                        std::string session_id,
                        RegisterScreenParam register_screen_param,
                        std::string url_base,
                        RegisterScreenRequestCallback callback);
  RegisterScreenRequest(const RegisterScreenRequest&) = delete;
  RegisterScreenRequest& operator=(const RegisterScreenRequest&) = delete;
  ~RegisterScreenRequest() override;

  std::string session_id() const { return session_id_; }
  void set_register_screen_param(RegisterScreenParam register_screen_param) {
    register_screen_param_ = std::move(register_screen_param);
  }

  // For testing.
  void OverrideURLForTesting(std::string url);

  RegisterScreenRequestCallback callback() { return std::move(callback_); }

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
  RegisterScreenParam register_screen_param_;
  std::string url_base_;
  RegisterScreenRequestCallback callback_;
  base::WeakPtrFactory<RegisterScreenRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_REGISTER_SCREEN_REQUEST_H_
