// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/get_session_request.h"

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash::boca {
namespace {

// TODO(b/359985023):Remove manual serialization after server is enabled to
// send proto.
::boca::StudentStatus::StudentState StudentStatusJsonToProto(
    const std::string& status) {
  if (status == "STUDENT_STATE_UNKNOWN") {
    return ::boca::StudentStatus::STUDENT_STATE_UNKNOWN;
  }
  if (status == "ADDED") {
    return ::boca::StudentStatus::ADDED;
  }
  if (status == "ACTIVE") {
    return ::boca::StudentStatus::ACTIVE;
  }
  if (status == "REMOVED_BY_OTHER_SESSION") {
    return ::boca::StudentStatus::REMOVED_BY_OTHER_SESSION;
  }
  if (status == "REMOVED_BY_BEING_TEACHER") {
    return ::boca::StudentStatus::REMOVED_BY_BEING_TEACHER;
  }
  if (status == "REMOVED_BY_TEACHER") {
    return ::boca::StudentStatus::REMOVED_BY_TEACHER;
  }
  return ::boca::StudentStatus::STUDENT_STATE_UNKNOWN;
}

::boca::Session::SessionState SessionStateJsonToProto(
    const std::string& state) {
  if (state == "SESSION_STATE_UNKNOWN") {
    return ::boca::Session::SESSION_STATE_UNKNOWN;
  }
  if (state == "PLANNING") {
    return ::boca::Session::PLANNING;
  }
  if (state == "ACTIVE") {
    return ::boca::Session::ACTIVE;
  }
  if (state == "PAST") {
    return ::boca::Session::PAST;
  }
  return ::boca::Session::SESSION_STATE_UNKNOWN;
}

::boca::LockedNavigationOptions::NavigationType NavigationTypeJsonToProto(
    const std::string& type) {
  if (type == "NAVIGATION_TYPE_UNKNOWN") {
    return ::boca::LockedNavigationOptions::NAVIGATION_TYPE_UNKNOWN;
  }
  if (type == "OPEN_NAVIGATION") {
    return ::boca::LockedNavigationOptions::OPEN_NAVIGATION;
  }
  if (type == "BLOCK_NAVIGATION") {
    return ::boca::LockedNavigationOptions::BLOCK_NAVIGATION;
  }
  if (type == "DOMAIN_NAVIGATION") {
    return ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION;
  }
  if (type == "LIMITED_NAVIGATION") {
    return ::boca::LockedNavigationOptions::LIMITED_NAVIGATION;
  }
  return ::boca::LockedNavigationOptions::NAVIGATION_TYPE_UNKNOWN;
}

void ParseTeacher(base::Value::Dict* session_dict, ::boca::Session* session) {
  if (session_dict->FindDict(kTeacher)) {
    auto* teacher = session->mutable_teacher();
    if (auto* ptr = session_dict->FindDict(kTeacher)->FindString(kEmail)) {
      teacher->set_email(*ptr);
    }
    if (auto* ptr = session_dict->FindDict(kTeacher)->FindString(kGaiaId)) {
      teacher->set_gaia_id(*ptr);
    }
    if (auto* ptr = session_dict->FindDict(kTeacher)->FindString(kFullName)) {
      teacher->set_full_name(*ptr);
    }

    if (auto* ptr = session_dict->FindDict(kTeacher)->FindString(kPhotoUrl)) {
      teacher->set_photo_url(*ptr);
    }
  }
}

void ParseRoster(base::Value::Dict* session_dict, ::boca::Session* session) {
  auto* roster_dict = session_dict->FindDict(kRoster);
  if (roster_dict) {
    auto* roster = session->mutable_roster();
    if (auto* ptr = roster_dict->FindString(kRosterTitle)) {
      roster->set_title(*ptr);
    }

    auto* student_list_dict = roster_dict->FindList(kStudentGroups);
    if (student_list_dict) {
      for (auto& students_dict : *student_list_dict) {
        auto* student_groups =
            session->mutable_roster()->mutable_student_groups()->Add();
        if (auto* ptr =
                students_dict.GetIfDict()->FindString(kStudentGroupTitle)) {
          student_groups->set_title(*ptr);
        }
        if (auto* items = students_dict.GetIfDict()->FindList(kStudents)) {
          for (auto& item : *items) {
            auto* item_dict = item.GetIfDict();

            auto* students = student_groups->mutable_students()->Add();
            if (auto* ptr = item_dict->FindString(kEmail)) {
              students->set_email(*ptr);
            }
            if (auto* ptr = item_dict->FindString(kFullName)) {
              students->set_full_name(*ptr);
            }
            if (auto* ptr = item_dict->FindString(kGaiaId)) {
              students->set_gaia_id(*ptr);
            }
            if (auto* ptr = item_dict->FindString(kPhotoUrl)) {
              students->set_photo_url(*ptr);
            }
          }
        }
      }
    }
  }
}

void ParseSessionConfig(base::Value::Dict* session_dict,
                        ::boca::Session* session) {
  if (session_dict->FindDict(kStudentGroupsConfig)) {
    auto* student_groups = session->mutable_student_group_configs();

    base::Value::Dict* config;
    if (ash::boca_util::IsProducer()) {
      config = session_dict->FindDict(kStudentGroupsConfig)
                   ->FindDict(kMainStudentGroupName);
    } else {
      // For consumer, the group name will be masked, also fetch the first item.
      config = std::move(!session_dict->FindDict(kStudentGroupsConfig)->empty()
                             ? session_dict->FindDict(kStudentGroupsConfig)
                                   ->begin()
                                   ->second.GetIfDict()
                             : nullptr);
    }

    if (config) {
      ::boca::SessionConfig session_config;

      auto* caption_config_dict = config->FindDict(kCaptionsConfig);
      if (caption_config_dict) {
        auto* caption_config = session_config.mutable_captions_config();
        caption_config->set_captions_enabled(
            caption_config_dict->FindBool(kCaptionsEnabled).value_or(false));
        caption_config->set_translations_enabled(
            caption_config_dict->FindBool(kTranslationsEnabled)
                .value_or(false));
      }
      auto* on_task_config_dict = config->FindDict(kOnTaskConfig);
      if (on_task_config_dict) {
        auto* active_bundle_dict = on_task_config_dict->FindDict(kActiveBundle);
        if (active_bundle_dict) {
          auto* on_task_config = session_config.mutable_on_task_config();
          auto* active_bundle = on_task_config->mutable_active_bundle();
          active_bundle->set_locked(
              active_bundle_dict->FindBool(kLocked).value_or(false));
          auto* content_configs_list =
              active_bundle_dict->FindList(kContentConfigs);
          if (content_configs_list) {
            for (auto& item : *content_configs_list) {
              auto* content_configs =
                  active_bundle->mutable_content_configs()->Add();
              auto* item_dict = item.GetIfDict();
              if (auto* ptr = item_dict->FindString(kUrl)) {
                content_configs->set_url(*ptr);
              }
              if (auto* ptr = item_dict->FindString(kTitle)) {
                content_configs->set_title(*ptr);
              }
              if (auto* ptr = item_dict->FindString(kFavIcon)) {
                content_configs->set_favicon_url(*ptr);
              }
              if (item_dict->FindDict(kLockedNavigationOptions) &&
                  item_dict->FindDict(kLockedNavigationOptions)
                      ->FindString(kNavigationType)) {
                auto* lock_options =
                    content_configs->mutable_locked_navigation_options();
                lock_options->set_navigation_type(NavigationTypeJsonToProto(
                    *item_dict->FindDict(kLockedNavigationOptions)
                         ->FindString(kNavigationType)));
              }
            }
          }
        }
      }
      (*student_groups)[kMainStudentGroupName] = std::move(session_config);
    }
  }
}

void ParseStudentStatus(base::Value::Dict* session_dict,
                        ::boca::Session* session) {  // Student status.
  auto* student_status_dict = session_dict->FindDict(kStudentStatus);
  if (student_status_dict) {
    // Roster feature is disabled, always fetch the first item.
    if (session->roster().student_groups().size() > 0) {
      for (auto id : session->roster().student_groups()[0].students()) {
        if (auto* ptr = student_status_dict->FindDict(id.gaia_id())
                            ->FindString(kStudentStatusState)) {
          auto* student_statuses = session->mutable_student_statuses();
          ::boca::StudentStatus student_status;
          student_status.set_state(StudentStatusJsonToProto(*ptr));
          (*student_statuses)[id.gaia_id()] = std::move(student_status);
        }
      }
    }
  }
}

std::unique_ptr<::boca::Session> ParseResponse(std::string response) {
  std::unique_ptr<base::Value> raw_value = google_apis::ParseJson(response);

  if (!raw_value) {
    return nullptr;
  }

  auto session_dict = std::move(raw_value->GetIfDict());
  if (!session_dict) {
    return nullptr;
  }

  std::unique_ptr<::boca::Session> session =
      std::make_unique<::boca::Session>();

  if (auto* ptr = session_dict->FindString(kSessionId)) {
    session->set_session_id(*ptr);
  }

  if (session_dict->FindDict(kDuration)) {
    auto* duration = session->mutable_duration();
    duration->set_seconds(
        session_dict->FindDict(kDuration)->FindInt(kSeconds).value_or(0));
    duration->set_nanos(
        session_dict->FindDict(kDuration)->FindInt(kNanos).value_or(0));
  }

  if (session_dict->FindDict(kStartTime)) {
    auto* start_time = session->mutable_start_time();
    start_time->set_seconds(
        session_dict->FindDict(kStartTime)->FindInt(kSeconds).value_or(0));
    start_time->set_nanos(
        session_dict->FindDict(kStartTime)->FindInt(kNanos).value_or(0));
  }

  if (auto* ptr = session_dict->FindString(kSessionState)) {
    session->set_session_state(SessionStateJsonToProto(*ptr));
  }

  ParseTeacher(session_dict, session.get());

  ParseRoster(session_dict, session.get());

  ParseSessionConfig(session_dict, session.get());

  ParseStudentStatus(session_dict, session.get());

  return session;
}

}  // namespace

GetSessionRequest::GetSessionRequest(google_apis::RequestSender* sender,
                                     const std::string gaia_id,
                                     Callback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      gaia_id_(gaia_id),
      url_base_(kSchoolToolsApiBaseUrl),
      callback_(std::move(callback)) {}

GetSessionRequest::~GetSessionRequest() = default;

void GetSessionRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

GURL GetSessionRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kGetSessionUrlTemplate, {gaia_id_}, nullptr));
  return url;
}

google_apis::ApiErrorCode GetSessionRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool GetSessionRequest::IsSuccessfulErrorCode(google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

void GetSessionRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&GetSessionRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void GetSessionRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void GetSessionRequest::OnDataParsed(std::unique_ptr<::boca::Session> session) {
  if (!session) {
    std::move(callback_).Run(base::unexpected(google_apis::PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(session));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace ash::boca
