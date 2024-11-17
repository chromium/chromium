// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/update_student_activities_request.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"

namespace ash::boca {

namespace {

bool ParseResponse(std::string json) {
  // Always notify success if no HTTP error.
  return true;
}
}  // namespace

UpdateStudentActivitiesRequest::UpdateStudentActivitiesRequest(
    google_apis::RequestSender* sender,
    std::string session_id,
    std::string gaia_id,
    std::string device_id,
    UpdateStudentActivitiesCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      session_id_(std::move(session_id)),
      gaia_id_(std::move(gaia_id)),
      device_id_(std::move(device_id)),
      url_base_(kSchoolToolsApiBaseUrl),
      callback_(std::move(callback)) {}

UpdateStudentActivitiesRequest ::~UpdateStudentActivitiesRequest() = default;

GURL UpdateStudentActivitiesRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kInsertStudentActivity, {session_id_, gaia_id_, device_id_},
      /*=offsets*/ nullptr));
  return url;
}

google_apis::ApiErrorCode UpdateStudentActivitiesRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool UpdateStudentActivitiesRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod UpdateStudentActivitiesRequest::GetRequestType()
    const {
  return google_apis::HttpRequestMethod::kPost;
}

bool UpdateStudentActivitiesRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  *upload_content_type = boca::kContentTypeApplicationJson;
  base::Value::Dict root;
  base::Value::List activities;
  // TODO(b/371450038): Immediately dispatch event when they occur for now since
  // we only have one type of user activity, consider buffer and batch
  if (!active_tab_title_.empty()) {
    base::Value::Dict activity;
    base::Value::Dict tab;
    tab.Set(kTitle, active_tab_title_);
    activity.Set(kActiveTab, std::move(tab));
    activities.Append(std::move(activity));
  }
  root.Set(kActivities, std::move(activities));

  base::JSONWriter::Write(root, upload_content);
  return true;
}

void UpdateStudentActivitiesRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&UpdateStudentActivitiesRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void UpdateStudentActivitiesRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void UpdateStudentActivitiesRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void UpdateStudentActivitiesRequest::OnDataParsed(bool success) {
  std::move(callback_).Run(true);
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
