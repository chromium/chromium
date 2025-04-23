// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/add_students_request.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/gaia/gaia_id.h"

namespace {

bool ParseResponse(std::string json) {
  // Always notify success if no HTTP error.
  return true;
}
}  // namespace

namespace ash::boca {

AddStudentsRequest::AddStudentsRequest(google_apis::RequestSender* sender,
                                       std::string url_base,
                                       GaiaId gaia_id,
                                       std::string session_id,
                                       AddStudentsCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      gaia_id_(gaia_id),
      session_id_(session_id),
      url_base_(std::move(url_base)),
      callback_(std::move(callback)) {}

AddStudentsRequest ::~AddStudentsRequest() = default;

GURL AddStudentsRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kAddStudentsUrlTemplate, {gaia_id_.ToString(), session_id_}, nullptr));
  return url;
}

google_apis::ApiErrorCode AddStudentsRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool AddStudentsRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod AddStudentsRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kPost;
}

bool AddStudentsRequest::GetContentData(std::string* upload_content_type,
                                        std::string* upload_content) {
  *upload_content_type = boca::kContentTypeApplicationJson;

  base::Value::Dict root;
  base::Value::Dict student_group;
  student_group.Set(kStudentGroupId, student_group_id_);
  base::Value::List students;
  for (auto& student : students_) {
    base::Value::Dict item;
    item.Set(kGaiaId, student.gaia_id());
    item.Set(kEmail, student.email());
    item.Set(kFullName, student.full_name());
    item.Set(kPhotoUrl, student.photo_url());
    students.Append(std::move(item));
  }
  student_group.Set(kStudents, std::move(students));
  base::Value::List student_groups;
  student_groups.Append(std::move(student_group));
  root.Set(kStudentGroups, std::move(student_groups));

  base::JSONWriter::Write(root, upload_content);
  return true;
}

void AddStudentsRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&AddStudentsRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void AddStudentsRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void AddStudentsRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void AddStudentsRequest::OnDataParsed(bool success) {
  std::move(callback_).Run(true);
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
