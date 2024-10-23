// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/join_session_request.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/session_parser.h"

namespace ash::boca {

JoinSessionRequest::JoinSessionRequest(google_apis::RequestSender* sender,
                                       ::boca::UserIdentity user,
                                       std::string device_id,
                                       std::string join_code,
                                       JoinSessionCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      join_code_(join_code),
      user_(std::move(user)),
      device_id_(device_id),
      url_base_(kSchoolToolsApiBaseUrl),
      callback_(std::move(callback)) {}

JoinSessionRequest ::~JoinSessionRequest() = default;

GURL JoinSessionRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kJoinSessionUrlTemplate, {user_.gaia_id()}, nullptr));
  return url;
}

google_apis::ApiErrorCode JoinSessionRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool JoinSessionRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod JoinSessionRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kPost;
}

bool JoinSessionRequest::GetContentData(std::string* upload_content_type,
                                        std::string* upload_content) {
  *upload_content_type = boca::kContentTypeApplicationJson;

  base::Value::Dict root;

  root.Set(kSessionJoinCode, join_code_);

  base::Value::Dict identity;
  identity.Set(kGaiaId, user_.gaia_id());
  identity.Set(kFullName, user_.full_name());
  identity.Set(kEmail, user_.email());
  identity.Set(kPhotoUrl, user_.photo_url());
  root.Set(kStudent, std::move(identity));

  base::Value::Dict device;
  device.Set(kDeviceId, device_id_);
  root.Set(kDeviceInfo, std::move(device));

  base::JSONWriter::Write(root, upload_content);
  return true;
}

void JoinSessionRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&GetSessionProtoFromJson, std::move(response_body),
                         /*=is_producer*/ false),
          base::BindOnce(&JoinSessionRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void JoinSessionRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void JoinSessionRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void JoinSessionRequest::OnDataParsed(
    std::unique_ptr<::boca::Session> session) {
  if (!session) {
    std::move(callback_).Run(base::unexpected(google_apis::PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(session));
  }
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
