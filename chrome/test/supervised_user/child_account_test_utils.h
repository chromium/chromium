// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_CHILD_ACCOUNT_TEST_UTILS_H_
#define CHROME_TEST_SUPERVISED_USER_CHILD_ACCOUNT_TEST_UTILS_H_

#include <string>

namespace base {
class CommandLine;
}  // namespace base

namespace net::test_server {
class EmbeddedTestServer;
}  // namespace net::test_server

namespace supervised_user {

// Returns a base64-encoded placeholder token for child log-in.
std::string GetChildAccountOAuthIdToken();

// Append host resolver rule to command line. This should used in preference to
// MockHostResolver::AddRule, as it ensures that the rules are active before the
// test's SetUpOnMainThread() method is called.
//
// This can be called multiple times, and ensures that the rules are added
// rather than overwritten. This does not attempt to deduplicate hosts which are
// mapped multiple times.
//
// TODO(crbug.com/370393748): move this method somewhere more widely usable.
void AddHostResolverRule(base::CommandLine* command_line,
                         std::string_view host,
                         const net::test_server::EmbeddedTestServer& target);

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_CHILD_ACCOUNT_TEST_UTILS_H_
