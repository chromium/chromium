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
#include "base/values.h"
#include "base/version.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace updater::test {

struct HttpRequest;

namespace request {

// Tests whether an `HttpRequest` satisfies an expectation. Must handle an
// empty `HttpRequest` and report mismatches via `testing::AssertionFailure()`
// without recording GTest failures.
using Matcher =
    base::RepeatingCallback<testing::AssertionResult(const HttpRequest&)>;

// Sequence of `Matcher`s that must all succeed to match a request.
using MatcherGroup = std::list<Matcher>;

// Returns a matcher that succeeds if `expected_path_regex` fully matches
// `HttpRequest::relative_url` (path and query).
[[nodiscard]] Matcher GetPathMatcher(const std::string& expected_path_regex);

// Returns a matcher that succeeds if every `(header_name, header_regex)` entry
// in `expected_headers` is present and full-matches case-insensitively.
[[nodiscard]] Matcher GetHeaderMatcher(
    const base::flat_map<std::string, std::string> expected_headers);

// Returns a matcher that succeeds if the request has the updater `User-Agent`
// header for `updater_version` (which must have at least 3 components).
Matcher GetUpdaterUserAgentMatcher(
    const base::Version& updater_version = base::Version(kUpdaterVersion));

// Returns a matcher that succeeds if the proxied `POST` request targets
// `target_url`.
Matcher GetTargetURLMatcher(GURL target_url);

// Returns a matcher that succeeds if all regexes in
// `expected_content_regex_sequence` match the decoded body in order
// (case-insensitively).
[[nodiscard]] Matcher GetContentMatcher(
    const std::vector<std::string>& expected_content_regex_sequence);

// Returns a matcher that succeeds if the decoded body parses as a JSON
// dictionary covering `expected_target` (comparing string values
// case-insensitively; see `IsJSONSubset`).
[[nodiscard]] Matcher GetJSONContentMatcher(
    const base::DictValue& expected_target);

// Returns a matcher that succeeds if the request's `ismachine` field matches
// `scope`.
[[nodiscard]] Matcher GetScopeMatcher(UpdaterScope scope);

// Returns a matcher that succeeds if `app_id`'s install source in the request
// matches `priority`. Also succeeds for `Priority::kBackground` when `app_id`
// is absent from the request's app list.
[[nodiscard]] Matcher GetAppPriorityMatcher(const std::string& app_id,
                                            UpdateService::Priority priority);

// Returns a matcher that succeeds unless updates are disabled for the updater
// itself.
[[nodiscard]] Matcher GetUpdaterEnableUpdatesMatcher();

// Expected form name and ordered content regexes for a multipart form.
struct FormExpectations {
  FormExpectations(const std::string& name, std::vector<std::string> regexes);
  FormExpectations(const FormExpectations&);
  FormExpectations& operator=(const FormExpectations& other);
  ~FormExpectations();

  std::string name;

  // Regexes that must partially match the form data in order.
  std::vector<std::string> regex_sequence;
};

// Returns a matcher that succeeds if the `multipart/form-data` body contains
// each form in `form_expectations` and its `regex_sequence` in order.
[[nodiscard]] Matcher GetMultipartContentMatcher(
    const std::vector<FormExpectations>& form_expectations);

}  // namespace request
}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_REQUEST_MATCHER_H_
