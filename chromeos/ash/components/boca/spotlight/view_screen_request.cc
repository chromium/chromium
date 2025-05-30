// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/view_screen_request.h"

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/json/json_writer.h"
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

ViewScreenParam::ViewScreenParam(
    std::string teacher_gaia_id_param,
    std::string teacher_device_id_param,
    std::optional<std::string> teacher_device_robot_id_param,
    std::string student_gaia_id_param,
    std::string student_device_id_param)
    : teacher_gaia_id(teacher_gaia_id_param),
      teacher_device_id(teacher_device_id_param),
      teacher_device_robot_id(teacher_device_robot_id_param),
      student_gaia_id(student_gaia_id_param),
      student_device_id(student_device_id_param) {}
ViewScreenParam::ViewScreenParam(ViewScreenParam&& param)
    : teacher_gaia_id(std::move(param.teacher_gaia_id)),
      teacher_device_id(std::move(param.teacher_device_id)),
      teacher_device_robot_id(std::move(param.teacher_device_robot_id)),
      student_gaia_id(std::move(param.student_gaia_id)),
      student_device_id(std::move(param.student_device_id)) {}
ViewScreenParam& ViewScreenParam::ViewScreenParam::operator=(
    ViewScreenParam&& param) = default;
ViewScreenParam::~ViewScreenParam() = default;

ViewScreenRequest::ViewScreenRequest(google_apis::RequestSender* sender,
                                     std::string session_id,
                                     ViewScreenParam view_screen_param,
                                     std::string url_base,
                                     ViewScreenRequestCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      session_id_(std::move(session_id)),
      view_screen_param_(std::move(view_screen_param)),
      url_base_(std::move(url_base)),
      callback_(std::move(callback)) {}

ViewScreenRequest ::~ViewScreenRequest() = default;

GURL ViewScreenRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(
      base::ReplaceStringPlaceholders(kViewScreenUrlTemplate, {session_id_},
                                      /*=offsets*/ nullptr));
  return url;
}

google_apis::ApiErrorCode ViewScreenRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool ViewScreenRequest::IsSuccessfulErrorCode(google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod ViewScreenRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kPost;
}

bool ViewScreenRequest::GetContentData(std::string* upload_content_type,
                                       std::string* upload_content) {
  *upload_content_type = boca::kContentTypeApplicationJson;
  base::Value::Dict root;
  base::Value::Dict teacher_info;
  base::Value::Dict teacher;
  teacher.Set(kGaiaId, view_screen_param_.teacher_gaia_id);
  teacher_info.Set(kUser, std::move(teacher));

  base::Value::Dict teacher_device;
  teacher_device.Set(kDeviceId, view_screen_param_.teacher_device_id);
  teacher_info.Set(kDeviceInfo, std::move(teacher_device));

  if (view_screen_param_.teacher_device_robot_id.has_value()) {
    base::Value::Dict teacher_service_account;
    teacher_service_account.Set(
        kEmail, view_screen_param_.teacher_device_robot_id.value());
    teacher_info.Set(kServiceAccount, std::move(teacher_service_account));
  }

  root.Set(kTeacherClientDevice, std::move(teacher_info));

  base::Value::Dict host_device_info;
  base::Value::Dict host;
  host.Set(kGaiaId, view_screen_param_.student_gaia_id);
  host_device_info.Set(kUser, std::move(host));

  base::Value::Dict host_device;
  host_device.Set(kDeviceId, view_screen_param_.student_device_id);
  host_device_info.Set(kDeviceInfo, std::move(host_device));
  root.Set(kHostDevice, std::move(host_device_info));

  base::JSONWriter::Write(root, upload_content);
  return true;
}

void ViewScreenRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&ViewScreenRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void ViewScreenRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void ViewScreenRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void ViewScreenRequest::OnDataParsed(bool success) {
  std::move(callback_).Run(true);
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
