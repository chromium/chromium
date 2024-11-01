// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_UTIL_H_

#include "chromeos/ash/components/boca/proto/session.pb.h"

namespace ash::boca {

::boca::SessionConfig GetSessionConfigSafe(::boca::Session* session);

google::protobuf::RepeatedPtrField<::boca::UserIdentity> GetStudentGroupsSafe(
    ::boca::Session* session);

::boca::Roster GetRosterSafe(::boca::Session* session);

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_UTIL_H_
