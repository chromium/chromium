// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/register_screen_request.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
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

RegisterScreenParam::RegisterScreenParam(std::string connection_code_param,
                                         std::string student_gaia_id_param,
                                         std::string student_device_id_param)
    : connection_code(connection_code_param),
      student_gaia_id(student_gaia_id_param),
      student_device_id(student_device_id_param) {}
RegisterScreenParam::RegisterScreenParam(RegisterScreenParam&& param)
    : connection_code(std::move(param.connection_code)),
      student_gaia_id(std::move(param.student_gaia_id)),
      student_device_id(std::move(param.student_device_id)) {}
RegisterScreenParam& RegisterScreenParam::RegisterScreenParam::operator=(
    RegisterScreenParam&& param) = default;
RegisterScreenParam::~RegisterScreenParam() = default;

RegisterScreenRequest::RegisterScreenRequest(
    google_apis::RequestSender* sender,
    std::string session_id,
    RegisterScreenParam register_screen_param,
    std::string url_base,
    RegisterScreenRequestCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      session_id_(std::move(session_id)),
      register_screen_param_(std::move(register_screen_param)),
      url_base_(std::move(url_base)),
      callback_(std::move(callback)) {}

RegisterScreenRequest ::~RegisterScreenRequest() = default;

GURL RegisterScreenRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(
      base::ReplaceStringPlaceholders(kRegisterScreenUrlTemplate, {session_id_},
                                      /*=offsets*/ nullptr));
  return url;
}

google_apis::ApiErrorCode RegisterScreenRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool RegisterScreenRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod RegisterScreenRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kPost;
}

bool RegisterScreenRequest::GetContentData(std::string* upload_content_type,
                                           std::string* upload_content) {
  *upload_content_type = boca::kContentTypeApplicationJson;

  base::Value::Dict root;
  base::Value::Dict connection_param;
  base::Value::Dict host_device_info;

  connection_param.Set(kSpotlightConnectionCode,
                       register_screen_param_.connection_code);
  root.Set(kSpotlightConnectionParam, std::move(connection_param));

  base::Value::Dict host;
  host.Set(kGaiaId, register_screen_param_.student_gaia_id);
  host_device_info.Set(kUser, std::move(host));

  base::Value::Dict host_device;
  host_device.Set(kDeviceId, register_screen_param_.student_device_id);
  host_device_info.Set(kDeviceInfo, std::move(host_device));
  root.Set(kHostDevice, std::move(host_device_info));

  *upload_content = base::WriteJson(root).value_or("");
  return true;
}

void RegisterScreenRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&RegisterScreenRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void RegisterScreenRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void RegisterScreenRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void RegisterScreenRequest::OnDataParsed(bool success) {
  std::move(callback_).Run(true);
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
