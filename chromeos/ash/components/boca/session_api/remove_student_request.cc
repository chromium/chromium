// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/remove_student_request.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"

namespace {

bool ParseResponse(std::string json) {
  // Always notify success if no HTTP error.
  return true;
}
}  // namespace

namespace ash::boca {

RemoveStudentRequest::RemoveStudentRequest(google_apis::RequestSender* sender,
                                           std::string gaia_id,
                                           std::string session_id,
                                           RemoveStudentCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      gaia_id_(gaia_id),
      session_id_(session_id),
      url_base_(kSchoolToolsApiBaseUrl),
      callback_(std::move(callback)) {}

RemoveStudentRequest ::~RemoveStudentRequest() = default;

GURL RemoveStudentRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kRemoveStudentUrlTemplate, {gaia_id_, session_id_}, nullptr));
  return url;
}

google_apis::ApiErrorCode RemoveStudentRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool RemoveStudentRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod RemoveStudentRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kPost;
}

bool RemoveStudentRequest::GetContentData(std::string* upload_content_type,
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

void RemoveStudentRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&RemoveStudentRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void RemoveStudentRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void RemoveStudentRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void RemoveStudentRequest::OnDataParsed(bool success) {
  std::move(callback_).Run(true);
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
