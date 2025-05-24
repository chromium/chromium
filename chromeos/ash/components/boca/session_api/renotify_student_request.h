// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_RENOTIFY_STUDENT_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_RENOTIFY_STUDENT_REQUEST_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/gaia/gaia_id.h"

namespace google_apis {
class RequestSender;
enum ApiErrorCode;
}  // namespace google_apis

namespace ash::boca {

//=================RenotifyStudentRequest================

using RenotifyStudentCallback = base::OnceCallback<void(
    base::expected<bool, google_apis::ApiErrorCode> result)>;

class RenotifyStudentRequest : public google_apis::UrlFetchRequestBase {
 public:
  RenotifyStudentRequest(google_apis::RequestSender* sender,
                         std::string base_url,
                         GaiaId gaia_id,
                         std::string session_id,
                         RenotifyStudentCallback callback);
  RenotifyStudentRequest(const RenotifyStudentRequest&) = delete;
  RenotifyStudentRequest& operator=(const RenotifyStudentRequest&) = delete;
  ~RenotifyStudentRequest() override;

  // For testing.
  void OverrideURLForTesting(std::string url);

  RenotifyStudentCallback callback() { return std::move(callback_); }

  GaiaId gaia_id() { return gaia_id_; }
  std::string session_id() { return session_id_; }
  std::vector<std::string>& student_ids() { return student_ids_; }
  void set_student_ids(std::vector<std::string> student_ids) {
    student_ids_ = std::move(student_ids);
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
  void OnDataParsed();

  GaiaId gaia_id_;
  std::string session_id_;
  std::vector<std::string> student_ids_;
  std::string url_base_;

  RenotifyStudentCallback callback_;

  base::WeakPtrFactory<RenotifyStudentRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_RENOTIFY_STUDENT_REQUEST_H_
