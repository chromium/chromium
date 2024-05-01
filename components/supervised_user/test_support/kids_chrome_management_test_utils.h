// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_

#include <string_view>

#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"

namespace supervised_user {

void SetFamilyMemberAttributesForTesting(
    kidsmanagement::FamilyMember* mutable_member,
    kidsmanagement::FamilyRole role,
    std::string_view username);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_
