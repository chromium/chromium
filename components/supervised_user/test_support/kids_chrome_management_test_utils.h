// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_

#include <string_view>

#include "base/command_line.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace supervised_user {

void SetFamilyMemberAttributesForTesting(
    kidsmanagement::FamilyMember* mutable_member,
    kidsmanagement::FamilyRole role,
    std::string_view username);

// Append host resolver rule to command line. This should used in preference to
// MockHostResolver::AddRule, as it ensures that the rules are active before the
// test's SetUpOnMainThread() method is called.
//
// This can be called multiple times, and ensures that the rules are added
// rather than overwritten. This does not attempt to deduplicate hosts which are
// mapped multiple times.
void AddHostResolverRule(base::CommandLine* command_line,
                         std::string_view host,
                         const net::test_server::EmbeddedTestServer& target);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_KIDS_CHROME_MANAGEMENT_TEST_UTILS_H_
