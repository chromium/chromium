// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/create_session_request.h"

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

//=================CreateSessionRequest================
CreateSessionRequest::CreateSessionRequest(
    google_apis::RequestSender* sender,
    ::boca::UserIdentity teacher,
    base::TimeDelta duration,
    ::boca::Session::SessionState session_state,
    CreateSessionCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      teacher_(std::move(teacher)),
      duration_(duration),
      session_state_(session_state),
      url_base_(kSchoolToolsApiBaseUrl),
      callback_(std::move(callback)) {}

CreateSessionRequest ::~CreateSessionRequest() = default;

GURL CreateSessionRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kCreateSessionUrlTemplate, {teacher_.gaia_id()}, nullptr));
  return url;
}

google_apis::ApiErrorCode CreateSessionRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool CreateSessionRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod CreateSessionRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kPost;
}

bool CreateSessionRequest::GetContentData(std::string* upload_content_type,
                                          std::string* upload_content) {
  *upload_content_type = boca::kContentTypeApplicationJson;

  // We have to do manual serialization because Json library only exists in
  // protobuf-full, but chromium only include protobuf-lite.
  base::Value::Dict root;
  // Session metadata.
  base::Value::Dict teacher;
  teacher.Set(kGaiaId, teacher_.gaia_id());
  teacher.Set(kFullName, teacher_.full_name());
  teacher.Set(kEmail, teacher_.email());

  root.Set(kTeacher, std::move(teacher));

  base::Value::Dict duration;
  duration.Set(kSeconds, static_cast<int>(duration_.InSeconds()));
  root.Set(kDuration, std::move(duration));

  root.Set(kSessionState, session_state_);

  // Roster info
  if (roster_) {
    base::Value::Dict roster;
    ParseRosterJsonFromProto(roster_.get(), &roster);
    root.Set(kRoster, std::move(roster));
  }

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

  base::JSONWriter::Write(root, upload_content);
  return true;
}

void CreateSessionRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&GetSessionProtoFromJson, std::move(response_body)),
          base::BindOnce(&CreateSessionRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void CreateSessionRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void CreateSessionRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

void CreateSessionRequest::OnDataParsed(
    std::unique_ptr<::boca::Session> session) {
  if (!session) {
    std::move(callback_).Run(base::unexpected(google_apis::PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(session));
  }
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
