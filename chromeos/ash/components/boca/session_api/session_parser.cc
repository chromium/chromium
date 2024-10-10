// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/session_parser.h"

#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/common/base_requests.h"

namespace ash::boca {
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

void ParseTeacherProtoFromJson(base::Value::Dict* session_dict,
                               ::boca::Session* session) {
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

void ParseRosterProtoFromJson(base::Value::Dict* session_dict,
                              ::boca::Session* session) {
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

void ParseSessionConfigProtoFromJson(base::Value::Dict* session_dict,
                                     ::boca::Session* session) {
  if (!session_dict->FindDict(kStudentGroupsConfig)) {
    return;
  }
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

  if (!config) {
    return;
  }
  ::boca::SessionConfig session_config;

  auto* caption_config_dict = config->FindDict(kCaptionsConfig);
  if (caption_config_dict) {
    auto* caption_config = session_config.mutable_captions_config();
    caption_config->set_captions_enabled(
        caption_config_dict->FindBool(kCaptionsEnabled).value_or(false));
    caption_config->set_translations_enabled(
        caption_config_dict->FindBool(kTranslationsEnabled).value_or(false));
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

void ParseStudentStatusProtoFromJson(
    base::Value::Dict* session_dict,
    ::boca::Session* session) {  // Student status.
  auto* student_status_dict = session_dict->FindDict(kStudentStatus);
  if (!student_status_dict) {
    return;
  }
  // Roster feature is disabled, always fetch the first item.
  if (session->roster().student_groups().size() > 0) {
    for (auto id : session->roster().student_groups()[0].students()) {
      if (auto* ptr = student_status_dict->FindDict(id.gaia_id())) {
        auto* student_statuses = session->mutable_student_statuses();
        ::boca::StudentStatus student_status;
        // Set the student state
        if (auto* state_ptr = ptr->FindString(kStudentStatusState)) {
          student_status.set_state(StudentStatusJsonToProto(*state_ptr));
        }
        // Parse and set the devices
        if (auto* devices_ptr = ptr->FindDict(kDevices)) {
          for (auto device_iter : *devices_ptr) {
            if (auto* device_dict = device_iter.second.GetIfDict()) {
              auto& device_entry =
                  (*student_status.mutable_devices())[device_iter.first];
              // Parse and set ActiveTab from StudentDeviceActivity
              if (auto* activity = device_dict->FindDict(kActivity)) {
                if (auto* active_tab_ptr = activity->FindDict(kActiveTab)) {
                  device_entry.mutable_activity()
                      ->mutable_active_tab()
                      ->set_title(active_tab_ptr->FindString(kTitle)
                                      ? *active_tab_ptr->FindString(kTitle)
                                      : "");
                }
              }
            }
          }
        }
        (*student_statuses)[id.gaia_id()] = std::move(student_status);
      }
    }
  }
}

std::unique_ptr<::boca::Session> GetSessionProtoFromJson(std::string json) {
  std::unique_ptr<base::Value> raw_value = google_apis::ParseJson(json);
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
    if (auto* ptr = session_dict->FindDict(kDuration)->FindString(kSeconds)) {
      int64_t output;
      if (base::StringToInt64(*ptr, &output)) {
        duration->set_seconds(output);
      }
    }
    duration->set_nanos(
        session_dict->FindDict(kDuration)->FindInt(kNanos).value_or(0));
  }

  if (session_dict->FindDict(kStartTime)) {
    auto* start_time = session->mutable_start_time();
    if (auto* ptr = session_dict->FindDict(kStartTime)->FindString(kSeconds)) {
      int64_t output;
      if (base::StringToInt64(*ptr, &output)) {
        start_time->set_seconds(output);
      }
    }
    start_time->set_nanos(
        session_dict->FindDict(kStartTime)->FindInt(kNanos).value_or(0));
  }

  if (auto* ptr = session_dict->FindString(kSessionState)) {
    session->set_session_state(SessionStateJsonToProto(*ptr));
  }

  ParseTeacherProtoFromJson(session_dict, session.get());

  ParseRosterProtoFromJson(session_dict, session.get());

  ParseSessionConfigProtoFromJson(session_dict, session.get());

  ParseStudentStatusProtoFromJson(session_dict, session.get());

  return session;
}

void ParseRosterJsonFromProto(::boca::Roster* roster,
                              base::Value::Dict* roster_dict) {
  if (roster && !roster->student_groups().empty()) {
    base::Value::Dict student_groups;
    // Only handle main roster student for now.
    student_groups.Set(kTitle, kMainStudentGroupName);
    base::Value::List students;
    for (const auto& student : roster->student_groups()[0].students()) {
      base::Value::Dict item;
      item.Set(kGaiaId, student.gaia_id());
      item.Set(kEmail, student.email());
      item.Set(kFullName, student.full_name());
      item.Set(kPhotoUrl, student.photo_url());
      students.Append(std::move(item));
    }
    student_groups.Set(kStudents, base::Value(std::move(students)));
    roster_dict->Set(kStudentGroups, std::move(student_groups));
  }
}

void ParseOnTaskConfigJsonFromProto(::boca::OnTaskConfig* on_task_config,
                                    base::Value::Dict* on_task_config_dict) {
  if (on_task_config && on_task_config->has_active_bundle()) {
    base::Value::Dict bundle;
    bundle.Set(kLocked, on_task_config->active_bundle().locked());
    base::Value::List content_configs;
    for (const auto& content :
         on_task_config->active_bundle().content_configs()) {
      base::Value::Dict item;
      item.Set(kUrl, content.url());
      item.Set(kTitle, content.title());
      item.Set(kFavIcon, content.favicon_url());
      if (content.has_locked_navigation_options()) {
        base::Value::Dict navigation_type;
        navigation_type.Set(
            kNavigationType,
            content.locked_navigation_options().navigation_type());
        item.Set(kLockedNavigationOptions, std::move(navigation_type));
      }
      content_configs.Append(std::move(item));
    }
    bundle.Set(kContentConfigs, std::move(content_configs));
    on_task_config_dict->Set(kActiveBundle, std::move(bundle));
  }
}

void ParseCaptionConfigJsonFromProto(::boca::CaptionsConfig* captions_config,
                                     base::Value::Dict* caption_config_dict) {
  if (captions_config) {
    caption_config_dict->Set(kCaptionsEnabled,
                             captions_config->captions_enabled());
    caption_config_dict->Set(kTranslationsEnabled,
                             captions_config->translations_enabled());
  }
}

}  // namespace ash::boca
