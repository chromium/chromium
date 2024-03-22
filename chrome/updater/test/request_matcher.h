// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_REQUEST_MATCHER_H_
#define CHROME_UPDATER_TEST_REQUEST_MATCHER_H_

#include <list>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

class GURL;

namespace updater::test {

struct HttpRequest;

namespace request {
// Defines a generic matcher to match expectations for a request.
using Matcher = base::RepeatingCallback<bool(const HttpRequest&)>;

// Defines a group of matchers which all must pass in order to match a request.
// This allows for combining several matchers when matching a single request.
using MatcherGroup = std::list<Matcher>;

// Returns a matcher which returns true if the `expected_path_regex` fully
// matches the request path.
[[nodiscard]] Matcher GetPathMatcher(const std::string& expected_path_regex);

// Returns a matcher which returns true if all the `expected_headers` are
// found. `expected_headers` is a map of (header_name, header_content_regex).
[[nodiscard]] Matcher GetHeaderMatcher(
    const base::flat_map<std::string, std::string> expected_headers);

// Returns a matcher which returns true if the request has updater's user-agent.
Matcher GetUpdaterUserAgentMatcher();

// Returns a matcher which returns true if the request is actually intended
// for the `target_url` (uses current server as a proxy).
Matcher GetTargetURLMatcher(GURL target_url);

// Returns a matcher which returns true if all regexex are found in the given
// order.
[[nodiscard]] Matcher GetContentMatcher(
    const std::vector<std::string>& expected_content_regex_sequence);

// Returns a matcher which returns true if request scope is same as the given
// value.
[[nodiscard]] Matcher GetScopeMatcher(UpdaterScope scope);

// Returns a matcher which returns true if the app priority in request matches
// the given value.
[[nodiscard]] Matcher GetAppPriorityMatcher(const std::string& app_id,
                                            UpdateService::Priority priority);

// Returns a matcher which checks that update is enabled for updater itself.
[[nodiscard]] Matcher GetUpdaterEnableUpdatesMatcher();

// Defines the expectations of a form in a multipart content.
struct FormExpectations {
  FormExpectations(const std::string& name, std::vector<std::string> regexes);
  FormExpectations(const FormExpectations&);
  FormExpectations& operator=(const FormExpectations& other);
  ~FormExpectations();

  std::string name;

  // A list of regexes to partially match the form data in the given order.
  std::vector<std::string> regex_sequence;
};

// Returns a matcher, which returns true if the forms are separated by the
// form-data boundary, each form has the expected name, and all form-regexex are
// found in the given order.
[[nodiscard]] Matcher GetMultipartContentMatcher(
    const std::vector<FormExpectations>& form_expections);

}  // namespace request
}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_REQUEST_MATCHER_H_
