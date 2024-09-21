// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/update_session_request.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/session_parser.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "third_party/protobuf/src/google/protobuf/map_field_lite.h"

namespace ash::boca {

UpdateSessionRequest::UpdateSessionRequest(google_apis::RequestSender* sender,
                                           ::boca::UserIdentity teacher,
                                           std::string session_id,
                                           UpdateSessionCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      teacher_(std::move(teacher)),
      session_id_(session_id),
      url_base_(kSchoolToolsApiBaseUrl),
      callback_(std::move(callback)) {}

UpdateSessionRequest ::~UpdateSessionRequest() = default;

GURL UpdateSessionRequest::GetURL() const {
  std::vector<std::string_view> update_masks;
  if (session_state_) {
    update_masks.push_back(kSessionState);
  }
  if (duration_) {
    update_masks.push_back(kDuration);
  }
  if (on_task_config_ || captions_config_) {
    update_masks.push_back(kStudentGroupsConfig);
  }
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kUpdateSessionUrlTemplate,
      {teacher_.gaia_id(), session_id_, base::JoinString(update_masks, ",")},
      /*=offsets*/ nullptr));
  return url;
}

google_apis::ApiErrorCode UpdateSessionRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool UpdateSessionRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod UpdateSessionRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kPatch;
}

bool UpdateSessionRequest::GetContentData(std::string* upload_content_type,
                                          std::string* upload_content) {
  *upload_content_type = boca::kContentTypeApplicationJson;

  base::Value::Dict root;

  // Mandatory data for all update request.
  root.Set(kSessionId, session_id_);
  base::Value::Dict teacher;
  teacher.Set(kGaiaId, teacher_.gaia_id());
  root.Set(kTeacher, std::move(teacher));

  if (duration_) {
    base::Value::Dict duration;
    duration.Set(kSeconds, static_cast<int>(duration_->InSeconds()));
    root.Set(kDuration, std::move(duration));
  }

  if (session_state_) {
    root.Set(kSessionState, *session_state_);
  }

  if (on_task_config_ || captions_config_) {
    // We always have to patch the full student groups config.
    base::Value::Dict student_config;

    // Ontask config
    if (on_task_config_) {
      base::Value::Dict on_task_config;
      ParseOnTaskConfigJsonFromProto(on_task_config_.get(), &on_task_config);
      student_config.Set(kOnTaskConfig, std::move(on_task_config));
    }

    // Caption Config
    if (captions_config_) {
      base::Value::Dict caption_config;
      ParseCaptionConfigJsonFromProto(captions_config_.get(), &caption_config);
      student_config.Set(kCaptionsConfig, std::move(caption_config));
    }

    base::Value::Dict main_group_student_config;
    main_group_student_config.Set(kMainStudentGroupName,
                                  std::move(student_config));
    root.Set(kStudentGroupsConfig, std::move(main_group_student_config));
  }

  base::JSONWriter::Write(root, upload_content);
  return true;
}

void UpdateSessionRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&GetSessionProtoFromJson, std::move(response_body)),
          base::BindOnce(&UpdateSessionRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void UpdateSessionRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void UpdateSessionRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void UpdateSessionRequest::OnDataParsed(
    std::unique_ptr<::boca::Session> session) {
  if (!session) {
    std::move(callback_).Run(base::unexpected(google_apis::PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(session));
  }
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
