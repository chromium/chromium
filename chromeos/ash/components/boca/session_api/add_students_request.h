// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_ADD_STUDENTS_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_ADD_STUDENTS_REQUEST_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/gaia/gaia_id.h"

namespace google_apis {
class RequestSender;
enum ApiErrorCode;
}  // namespace google_apis

namespace ash::boca {

//=================AddStudentsRequest================

using AddStudentsCallback = base::OnceCallback<void(
    base::expected<bool, google_apis::ApiErrorCode> result)>;

class AddStudentsRequest : public google_apis::UrlFetchRequestBase {
 public:
  AddStudentsRequest(google_apis::RequestSender* sender,
                     std::string base_url,
                     GaiaId gaia_id,
                     std::string session_id,
                     AddStudentsCallback callback);
  AddStudentsRequest(const AddStudentsRequest&) = delete;
  AddStudentsRequest& operator=(const AddStudentsRequest&) = delete;
  ~AddStudentsRequest() override;

  // For testing.
  void OverrideURLForTesting(std::string url);

  AddStudentsCallback callback() { return std::move(callback_); }

  const GaiaId& gaia_id() { return gaia_id_; }
  const std::string& session_id() { return session_id_; }
  const std::string& student_group_id() { return student_group_id_; }
  const std::vector<::boca::UserIdentity>& students() { return students_; }
  void set_student_group_id(std::string student_group_id) {
    student_group_id_ = std::move(student_group_id);
  }
  void set_students(std::vector<::boca::UserIdentity> students) {
    students_ = std::move(students);
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

  GaiaId gaia_id_;
  std::string session_id_;
  std::string student_group_id_;
  std::vector<::boca::UserIdentity> students_;
  std::string url_base_;

  AddStudentsCallback callback_;

  base::WeakPtrFactory<AddStudentsRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_ADD_STUDENTS_REQUEST_H_
