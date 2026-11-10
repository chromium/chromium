// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_REQUEST_MATCHER_H_
#define CHROME_UPDATER_TEST_REQUEST_MATCHER_H_

// Predicates (`Matcher`s) that test `HttpRequest`s received by `ScopedServer`.

#include <list>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/version.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_version.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace base {
class DictValue;
}  // namespace base

namespace updater {
enum class UpdaterScope;
}  // namespace updater

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

// Returns a matcher that succeeds if `expected_path_regex` full-matches
// `HttpRequest::relative_url` (path and query) case-sensitively. CHECKs that
// `expected_path_regex` is valid.
[[nodiscard]] Matcher GetPathMatcher(const std::string& expected_path_regex);

// Returns a matcher that succeeds if `request.headers` contains each header in
// `expected_headers` and its value full-matches the mapped regex
// (both case-insensitively). CHECKs that each regex is valid.
[[nodiscard]] Matcher GetHeaderMatcher(
    const base::flat_map<std::string, std::string>& expected_headers);

// Returns a matcher that succeeds if the request has the updater or test
// `User-Agent` header for `updater_version`. CHECKs that `updater_version` has
// at least 3 components.
[[nodiscard]] Matcher GetUpdaterUserAgentMatcher(
    const base::Version& updater_version = base::Version(kUpdaterVersion));

// Returns a matcher that succeeds if the request is a proxied `POST` to
// `target_url`, ignoring the request's query, and its `Host` header is
// `target_url`'s host, followed by its port if not the scheme's default.
// CHECKs that `target_url` is valid and has no query or fragment.
[[nodiscard]] Matcher GetTargetURLMatcher(GURL target_url);

// Returns a matcher that succeeds if the regexes in
// `expected_content_regex_sequence` partially match non-overlapping substrings
// of `decoded_content` in order, case-insensitively. CHECKs that each regex is
// valid.
[[nodiscard]] Matcher GetContentMatcher(
    const std::vector<std::string>& expected_content_regex_sequence);

// Returns a matcher that succeeds if the decoded body parses as a JSON
// dictionary covering `expected_target` (comparing string values
// case-insensitively; see `IsJSONSubset`).
[[nodiscard]] Matcher GetJSONContentMatcher(
    const base::DictValue& expected_target);

// Returns a matcher that succeeds if any entry of the JSON body's app list
// (`request.apps`, else `request.app`) covers `expected_app` (comparing string
// values case-insensitively; see `IsJSONSubset`). For a `request.app` (protocol
// V3) list, a top-level `events` key matches the entry's `event` key. CHECKs
// that `expected_app` does not set `event`.
[[nodiscard]] Matcher GetAppJSONContentMatcher(
    const base::DictValue& expected_app);

// Returns a matcher that succeeds if `request.ismachine` in the JSON body
// matches `scope`.
[[nodiscard]] Matcher GetScopeMatcher(UpdaterScope scope);

// Returns a matcher that succeeds if `app_id` (case-insensitively) in the JSON
// body's app list (`request.apps`, else `request.app`) has an `installsource`
// matching `priority` (case-sensitively). Also matches `Priority::kBackground`
// if `app_id` is absent or omits `installsource`.
[[nodiscard]] Matcher GetAppPriorityMatcher(const std::string& app_id,
                                            UpdateService::Priority priority);

// Returns a matcher that succeeds if the JSON body's app list (`request.apps`,
// else `request.app`) does not set `updatecheck.updatedisabled` to `true` for
// `kUpdaterAppId` (case-insensitively).
[[nodiscard]] Matcher GetUpdaterEnableUpdatesMatcher();

// Expected form name (case-sensitive) and content regexes for a multipart part.
struct FormExpectations {
  FormExpectations(const std::string& name, std::vector<std::string> regexes);
  FormExpectations(const FormExpectations&);
  FormExpectations& operator=(const FormExpectations& other);
  ~FormExpectations();

  std::string name;

  // Regexes that must match the part in order, case-insensitively. See
  // `GetMultipartContentMatcher()`.
  std::vector<std::string> regex_sequence;
};

// Returns a matcher that succeeds if the request has a well-formed
// `multipart/form-data` body that contains the forms in `form_expectations` in
// order, possibly with other parts between them. Each form's `regex_sequence`
// must match in order within the rest of that part's headers after
// `name="<name>"`, and its data. CHECKs that each regex is valid.
[[nodiscard]] Matcher GetMultipartContentMatcher(
    const std::vector<FormExpectations>& form_expectations);

}  // namespace request
}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_REQUEST_MATCHER_H_
