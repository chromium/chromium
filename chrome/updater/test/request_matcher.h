// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_REQUEST_MATCHER_H_
#define CHROME_UPDATER_TEST_REQUEST_MATCHER_H_

#include <list>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace net::test_server {

struct HttpRequest;

}  // namespace net::test_server

namespace updater::test::request {

// Defines a generic matcher to match expectations for a request.
using Matcher =
    base::RepeatingCallback<bool(const net::test_server::HttpRequest&)>;

// Defines a group of matchers which all must pass in order to match a request.
// This allows for combining several matchers when matching a single request.
using MatcherGroup = std::list<Matcher>;

// Returns a matcher which returns true if the `expected_path_regex` fully
// matches the request path.
[[nodiscard]] Matcher GetPathMatcher(const std::string& expected_path_regex);

// Returns a matcher which returns true if the `expected_header_regex` fully
// matches the specified header in request.
[[nodiscard]] Matcher GetHeaderMatcher(
    const std::string& header_name,
    const std::string& expected_header_regex);

// Returns a matcher which returns true if the `expected_content_regex`
// partially matches the request content.
[[nodiscard]] Matcher GetContentMatcher(
    const std::string& expected_content_regex);

// Returns a matcher which returns true if request scope is same as the given
// value.
[[nodiscard]] Matcher GetScopeMatcher(UpdaterScope scope);

// Returns a matcher which returns true if the app priority in request matches
// the given value.
[[nodiscard]] Matcher GetAppPriorityMatcher(const std::string& app_id,
                                            UpdateService::Priority priority);

}  // namespace updater::test::request

#endif  // CHROME_UPDATER_TEST_REQUEST_MATCHER_H_
