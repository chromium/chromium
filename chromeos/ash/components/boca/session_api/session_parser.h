// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_SESSION_PARSER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_SESSION_PARSER_H_

#include <string>

#include "base/values.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"

namespace ash::boca {
// Enum translation
::boca::StudentStatus::StudentState StudentStatusJsonToProto(
    const std::string& status);

::boca::Session::SessionState SessionStateJsonToProto(const std::string& state);

::boca::LockedNavigationOptions::NavigationType NavigationTypeJsonToProto(
    const std::string& type);

::boca::StudentGroup::GroupSource GroupSourceJsonToProto(
    const std::string& type);

::boca::ViewScreenConfig::ViewScreenState ViewScreenStateJsonToProto(
    const std::string& type);
// Proto to Json
void ParseTeacherProtoFromJson(base::DictValue* session_dict,
                               ::boca::Session* session);
void ParseJoinCodeProtoFromJson(base::DictValue* session_dict,
                                ::boca::Session* session);
void ParseRosterProtoFromJson(base::DictValue* session_dict,
                              ::boca::Session* session);
void ParseSessionConfigProtoFromJson(base::DictValue* session_dict,
                                     ::boca::Session* session,
                                     bool is_producer);
void ParseStudentStatusProtoFromJson(base::DictValue* session_dict,
                                     ::boca::Session* session,
                                     bool is_producer);
void ParseIndividualStudentStatusFromJson(::boca::StudentStatus* student_status,
                                          base::DictValue* student_status_dict);
// This helper returns unique_ptr for easier lifecycle management.
std::unique_ptr<::boca::Session> GetSessionProtoFromJson(std::string json,
                                                         bool is_producer);
// Json to Proto
void ParseRosterJsonFromProto(::boca::Roster* roster,
                              base::DictValue* roster_dict);

void ParseOnTaskConfigJsonFromProto(::boca::OnTaskConfig* on_task_config,
                                    base::DictValue* on_task_config_dict);

void ParseCaptionConfigJsonFromProto(::boca::CaptionsConfig* captions_config,
                                     base::DictValue* caption_config_dict);

}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_SESSION_PARSER_H_
