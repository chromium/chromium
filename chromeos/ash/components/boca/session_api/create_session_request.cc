// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/create_session_request.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "third_party/protobuf/src/google/protobuf/map_field_lite.h"

namespace {

bool ParseResponse(std::string json) {
  // TODO(b/358476060): Always notify success if http code is success. Align
  // with server if there is additional data needs to be handled.
  return true;
}
}  // namespace

namespace ash::boca {

//=================CreateSessionRequest================
CreateSessionRequest::CreateSessionRequest(
    google_apis::RequestSender* sender,
    std::string gaia_id,
    base::TimeDelta duration,
    ::boca::Session::SessionState session_state,
    CreateSessionCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      teacher_gaia_id_(std::move(gaia_id)),
      duration_(duration),
      session_state_(session_state),
      url_base_(kSchoolToolsApiBaseUrl),
      callback_(std::move(callback)) {}

CreateSessionRequest ::~CreateSessionRequest() = default;

GURL CreateSessionRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kCreateSessionUrlTemplate, {teacher_gaia_id_}, nullptr));
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
  if (!teacher_gaia_id_.empty()) {
    base::Value::Dict teacher;
    teacher.Set("gaia_id", teacher_gaia_id_);
    root.Set("teacher", std::move(teacher));
  }
  base::Value::Dict duration;
  duration.Set("seconds", static_cast<int>(duration_.InSeconds()));
  root.Set("duration", std::move(duration));

  root.Set("session_state", session_state_);

  // Roster info
  if (!student_groups_.empty()) {
    base::Value::Dict roster;
    base::Value::Dict student_groups;
    student_groups.Set("title", kMainStudentGroupName);
    base::Value::List students;
    for (const auto& student : student_groups_) {
      base::Value::Dict item;
      item.Set("gaia_id", student.gaia_id());
      item.Set("email", student.email());
      item.Set("full_name", student.full_name());
      item.Set("photo_url", student.photo_url());
      students.Append(std::move(item));
    }
    student_groups.Set("students", base::Value(std::move(students)));
    roster.Set("student_groups", std::move(student_groups));

    root.Set("roster", std::move(roster));
  }

  base::Value::Dict student_config;

  // Ontask config
  if (on_task_config_ && on_task_config_->has_active_bundle()) {
    base::Value::Dict bundle;
    bundle.Set("locked", on_task_config_->active_bundle().locked());
    base::Value::List content_configs;
    for (const auto& content :
         on_task_config_->active_bundle().content_configs()) {
      base::Value::Dict item;
      item.Set("url", content.url());
      item.Set("title", content.title());
      item.Set("favicon_url", content.favicon_url());
      if (content.has_locked_navigation_options()) {
        base::Value::Dict navigation_type;
        navigation_type.Set(
            "navigation_type",
            content.locked_navigation_options().navigation_type());
        item.Set("locked_navigation_options", std::move(navigation_type));
      }
      content_configs.Append(std::move(item));
    }
    bundle.Set("content_configs", std::move(content_configs));

    base::Value::Dict on_task_config;
    on_task_config.Set("active_bundle", std::move(bundle));
    student_config.Set("on_task_config", std::move(on_task_config));
  }

  // Caption Config
  if (captions_config_) {
    base::Value::Dict caption_config;
    caption_config.Set("captions_enabled",
                       captions_config_->captions_enabled());
    caption_config.Set("translations_enabled",
                       captions_config_->translations_enabled());

    student_config.Set("captions_config", std::move(caption_config));
  }
  base::Value::Dict main_group_student_config;
  main_group_student_config.Set(kMainStudentGroupName,
                                std::move(student_config));
  root.Set("student_group_configs", std::move(main_group_student_config));

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
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
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

void CreateSessionRequest::OnDataParsed(bool success) {
  // Notify success immediately for now.
  std::move(callback_).Run(true);
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
