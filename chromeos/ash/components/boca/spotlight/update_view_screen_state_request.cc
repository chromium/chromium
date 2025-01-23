// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/update_view_screen_state_request.h"

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_writer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
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

UpdateViewScreenStateParam::UpdateViewScreenStateParam(
    std::string teacher_gaia_id_param,
    std::string teacher_device_id_param,
    std::string student_gaia_id_param,
    std::string student_device_id_param,
    ::boca::ViewScreenConfig::ViewScreenState view_screen_state_param)
    : teacher_gaia_id(teacher_gaia_id_param),
      teacher_device_id(teacher_device_id_param),
      student_gaia_id(student_gaia_id_param),
      student_device_id(student_device_id_param),
      view_screen_state(view_screen_state_param) {}
UpdateViewScreenStateParam::UpdateViewScreenStateParam(
    UpdateViewScreenStateParam&& param)
    : teacher_gaia_id(std::move(param.teacher_gaia_id)),
      teacher_device_id(std::move(param.teacher_device_id)),
      student_gaia_id(std::move(param.student_gaia_id)),
      student_device_id(std::move(param.student_device_id)),
      view_screen_state(std::move(param.view_screen_state)) {}
UpdateViewScreenStateParam&
UpdateViewScreenStateParam::UpdateViewScreenStateParam::operator=(
    UpdateViewScreenStateParam&& param) = default;
UpdateViewScreenStateParam::~UpdateViewScreenStateParam() = default;

UpdateViewScreenStateRequest::UpdateViewScreenStateRequest(
    google_apis::RequestSender* sender,
    std::string session_id,
    UpdateViewScreenStateParam update_view_screen_state_param,
    std::string url_base,
    UpdateViewScreenStateRequestCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      session_id_(std::move(session_id)),
      update_view_screen_state_param_(
          std::move(update_view_screen_state_param)),
      url_base_(std::move(url_base)),
      callback_(std::move(callback)) {}

UpdateViewScreenStateRequest ::~UpdateViewScreenStateRequest() = default;

GURL UpdateViewScreenStateRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kUpdateViewScreenStateUrlTemplate, {session_id_},
      /*=offsets*/ nullptr));
  return url;
}

google_apis::ApiErrorCode UpdateViewScreenStateRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool UpdateViewScreenStateRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod UpdateViewScreenStateRequest::GetRequestType()
    const {
  return google_apis::HttpRequestMethod::kPost;
}

bool UpdateViewScreenStateRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  *upload_content_type = boca::kContentTypeApplicationJson;
  base::Value::Dict root;
  base::Value::Dict teacher_info;
  base::Value::Dict teacher;
  teacher.Set(kGaiaId, update_view_screen_state_param_.teacher_gaia_id);
  teacher_info.Set(kUser, std::move(teacher));

  base::Value::Dict teacher_device;
  teacher_device.Set(kDeviceId,
                     update_view_screen_state_param_.teacher_device_id);
  teacher_info.Set(kDeviceInfo, std::move(teacher_device));

  root.Set(kTeacherClientDevice, std::move(teacher_info));

  base::Value::Dict host_device_info;
  base::Value::Dict host;
  host.Set(kGaiaId, update_view_screen_state_param_.student_gaia_id);
  host_device_info.Set(kUser, std::move(host));

  base::Value::Dict host_device;
  host_device.Set(kDeviceId, update_view_screen_state_param_.student_device_id);
  host_device_info.Set(kDeviceInfo, std::move(host_device));
  root.Set(kHostDevice, std::move(host_device_info));

  root.Set(kViewScreenState, update_view_screen_state_param_.view_screen_state);

  base::JSONWriter::Write(root, upload_content);
  return true;
}

void UpdateViewScreenStateRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&UpdateViewScreenStateRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void UpdateViewScreenStateRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void UpdateViewScreenStateRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void UpdateViewScreenStateRequest::OnDataParsed(bool success) {
  std::move(callback_).Run(true);
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
