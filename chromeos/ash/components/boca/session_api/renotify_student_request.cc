// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/renotify_student_request.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/gaia/gaia_id.h"

namespace ash::boca {

RenotifyStudentRequest::RenotifyStudentRequest(
    google_apis::RequestSender* sender,
    std::string url_base,
    GaiaId gaia_id,
    std::string session_id,
    RenotifyStudentCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      gaia_id_(gaia_id),
      session_id_(session_id),
      url_base_(std::move(url_base)),
      callback_(std::move(callback)) {}

RenotifyStudentRequest ::~RenotifyStudentRequest() = default;

GURL RenotifyStudentRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kNotifyGetActiveSession, {gaia_id_.ToString(), session_id_}, nullptr));
  return url;
}

google_apis::ApiErrorCode RenotifyStudentRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool RenotifyStudentRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod RenotifyStudentRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kPost;
}

bool RenotifyStudentRequest::GetContentData(std::string* upload_content_type,
                                            std::string* upload_content) {
  *upload_content_type = boca::kContentTypeApplicationJson;

  base::Value::Dict root;
  base::Value::List students;
  for (auto id : student_ids_) {
    base::Value::Dict item;
    item.Set(kGaiaId, id);
    students.Append(std::move(item));
  }
  root.Set(kUsers, std::move(students));
  base::JSONWriter::Write(root, upload_content);
  return true;
}

void RenotifyStudentRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      OnDataParsed();
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void RenotifyStudentRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void RenotifyStudentRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void RenotifyStudentRequest::OnDataParsed() {
  std::move(callback_).Run(/*success=*/true);
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
