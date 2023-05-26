// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/proto/kids_chrome_management_test_utils.h"

namespace supervised_user {

void SetFamilyMemberAttributesForTesting(
    kids_chrome_management::FamilyMember* mutable_member,
    kids_chrome_management::FamilyRole role,
    std::string username) {
  mutable_member->mutable_profile()->set_display_name(username);
  mutable_member->mutable_profile()->set_email(username + "@gmail.com");
  mutable_member->mutable_profile()->set_profile_url("http://profile.url/" +
                                                     username);
  mutable_member->mutable_profile()->set_profile_image_url("http://image.url/" +
                                                           username);
  mutable_member->set_role(role);
  mutable_member->set_user_id("obfuscatedGaiaId" + username);
}

}  // namespace supervised_user
