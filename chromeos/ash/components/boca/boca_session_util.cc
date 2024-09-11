// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_session_util.h"

#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"

namespace ash::boca {
::boca::SessionConfig GetSessionConfigSafe(::boca::Session* session) {
  if (!session) {
    return ::boca::SessionConfig();
  }
  if (session->student_group_configs().empty()) {
    return ::boca::SessionConfig();
  }
  auto it = session->student_group_configs().find(kMainStudentGroupName);
  if (it == session->student_group_configs().end()) {
    return ::boca::SessionConfig();
  }
  return it->second;
}

google::protobuf::RepeatedPtrField<::boca::UserIdentity> GetStudentGroupsSafe(
    ::boca::Session* session) {
  if (!session || session->roster().student_groups().empty()) {
    return google::protobuf::RepeatedPtrField<::boca::UserIdentity>();
  }
  return session->roster().student_groups()[0].students();
}

::boca::Roster GetRosterSafe(::boca::Session* session) {
  return session ? session->roster() : ::boca::Roster();
}
}  // namespace ash::boca
