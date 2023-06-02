// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_

#include "base/strings/string_piece.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"

namespace supervised_user {

void SetFamilyMemberAttributesForTesting(
    kids_chrome_management::FamilyMember* mutable_member,
    kids_chrome_management::FamilyRole role,
    base::StringPiece username);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_
