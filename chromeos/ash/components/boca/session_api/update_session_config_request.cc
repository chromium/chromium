// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/update_session_config_request.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/session_parser.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"

namespace ash::boca {

UpdateSessionConfigRequest::UpdateSessionConfigRequest(
    google_apis::RequestSender* sender,
    std::string url_base,
    ::boca::UserIdentity teacher,
    std::string session_id,
    UpdateSessionConfigCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      teacher_(std::move(teacher)),
      session_id_(session_id),
      url_base_(std::move(url_base)),
      callback_(std::move(callback)) {}

UpdateSessionConfigRequest ::~UpdateSessionConfigRequest() = default;

GURL UpdateSessionConfigRequest::GetURL() const {
  if (on_task_config_.has_value() && captions_config_.has_value()) {
    return GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
        kUpdateSessionConfigUrlTemplate, {teacher_.gaia_id(), session_id_},
        /*=offsets*/ nullptr));
  }

  std::string field_mask;
  if (on_task_config_.has_value()) {
    field_mask = kOnTaskConfig;
  } else if (captions_config_.has_value()) {
    field_mask = kCaptionsConfig;
  } else {
    return GURL();
  }

  return GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kUpdateSessionConfigUrlTemplateWithUpdateMask,
      {teacher_.gaia_id(), session_id_, field_mask},
      /*=offsets*/ nullptr));
}

google_apis::ApiErrorCode UpdateSessionConfigRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool UpdateSessionConfigRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod UpdateSessionConfigRequest::GetRequestType()
    const {
  return google_apis::HttpRequestMethod::kPatch;
}

bool UpdateSessionConfigRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  *upload_content_type = boca::kContentTypeApplicationJson;

  base::Value::Dict root;

  if (on_task_config_.has_value() || captions_config_.has_value()) {
    base::Value::Dict student_config;

    if (on_task_config_.has_value()) {
      base::Value::Dict on_task_config;
      ParseOnTaskConfigJsonFromProto(&on_task_config_.value(), &on_task_config);
      student_config.Set(kOnTaskConfig, std::move(on_task_config));
    }

    if (captions_config_.has_value()) {
      base::Value::Dict caption_config;
      ParseCaptionConfigJsonFromProto(&captions_config_.value(),
                                      &caption_config);
      student_config.Set(kCaptionsConfig, std::move(caption_config));
    }

    base::Value::List group_ids;
    for (auto id : group_ids_) {
      group_ids.Append(id);
    }

    root.Set(kSessionConfig, std::move(student_config));
    root.Set(kStudentGroupIds, std::move(group_ids));
  }

  auto json_string = base::WriteJson(root);
  if (!json_string.has_value()) {
    return false;
  }

  *upload_content = *base::WriteJson(root);
  return true;
}

void UpdateSessionConfigRequest::ProcessURLFetchResults(
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

void UpdateSessionConfigRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void UpdateSessionConfigRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void UpdateSessionConfigRequest::OnDataParsed() {
  std::move(callback_).Run(true);
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
