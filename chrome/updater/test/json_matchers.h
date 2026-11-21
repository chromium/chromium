// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_JSON_MATCHERS_H_
#define CHROME_UPDATER_TEST_JSON_MATCHERS_H_

// GMock matchers on JSON values, such as the update protocol request bodies
// that `ScopedServer` receives. Combine them with GMock matchers such as
// `testing::AllOf()` and `testing::Not()`, and match a request with
// `request::GetJSONContentMatcher()`. A mismatch explains where the value
// differs, as in `.request.apps[1].ping.ad: is 5098, expected absent`.

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace updater::test::json {

// A GMock matcher on a JSON value.
using ValueMatcher = testing::Matcher<const base::Value&>;

namespace internal {

// Implement `HasKey()` and `HasPath()` for a `matcher` that is already a
// `ValueMatcher`.
ValueMatcher HasKeyThat(std::string key, ValueMatcher matcher);
ValueMatcher HasPathThat(std::string_view path, ValueMatcher matcher);

// Returns a matcher for a value equal to `expected`.
ValueMatcher Equals(base::Value expected);

// Returns `matcher` as a `ValueMatcher`, or a matcher that compares with
// `base::Value` equality if `matcher` is a plain value.
template <typename M>
ValueMatcher ToValueMatcher(const M& matcher) {
  if constexpr (std::is_convertible_v<const M&, std::string_view>) {
    return Equals(base::Value(std::string_view(matcher)));
  } else if constexpr (std::is_same_v<M, bool>) {
    return Equals(base::Value(matcher));
  } else if constexpr (std::is_floating_point_v<M>) {
    return Equals(base::Value(static_cast<double>(matcher)));
  } else if constexpr (std::is_integral_v<M>) {
    // `base::JSONReader` parses an integer that doesn't fit in an `int` as a
    // `double`.
    return Equals(base::IsValueInRangeForNumericType<int>(matcher)
                      ? base::Value(static_cast<int>(matcher))
                      : base::Value(static_cast<double>(matcher)));
  } else if constexpr (std::is_same_v<M, base::Value> ||
                       std::is_same_v<M, base::DictValue> ||
                       std::is_same_v<M, base::ListValue>) {
    return Equals(base::Value(matcher.Clone()));
  } else {
    return testing::MatcherCast<const base::Value&>(matcher);
  }
}

}  // namespace internal

// Matches a dict that has `key`.
ValueMatcher HasKey(std::string key);

// Matches a dict that has `key` with a value that matches `matcher`. A plain
// value, such as a number, a string, or a `base::Value`, matches by
// `base::Value` equality, as in `HasKey("eventtype", 3)`: the types must be the
// same, so `3` doesn't match `3.0`, and strings compare case-sensitively. An
// integer that doesn't fit in an `int` matches a `double`, since
// `base::JSONReader` parses it as one. Inside other GMock matchers, write
// strings with `IsString()` and JSON values with `base::test::IsJson()`, as in
// `HasKey("installsource", IsString(AnyOf("a", "b")))`, since GMock prints C
// strings there as pointers and can't copy a `base::Value`. Compare strings
// case-insensitively with `IsString(StrCaseEq("a"))`.
template <typename M>
ValueMatcher HasKey(std::string key, const M& matcher) {
  return internal::HasKeyThat(std::move(key),
                              internal::ToValueMatcher(matcher));
}

// Matches a dict that doesn't have `key`. A value that isn't a dict doesn't
// match, so `Not(LacksKey(key))` matches it.
ValueMatcher LacksKey(std::string key);

// Matches a dict with a value at the dotted `path`, as in
// `HasPath("updatecheck.updatedisabled")`. `path` can't name a key that
// contains a `.`. CHECKs that `path` has no empty keys.
ValueMatcher HasPath(std::string_view path);

// Matches a dict with a value at the dotted `path` that matches `matcher`, as
// in `HasPath("updatecheck.updatedisabled", true)`. A plain value matches as in
// `HasKey()`. `path` can't name a key that contains a `.`. CHECKs that `path`
// has no empty keys.
template <typename M>
ValueMatcher HasPath(std::string_view path, const M& matcher) {
  return internal::HasPathThat(path, internal::ToValueMatcher(matcher));
}

// Matches a string that matches `matcher`, as in `IsString(StartsWith("1."))`.
ValueMatcher IsString(testing::Matcher<const std::string&> matcher);

// Matches a list that matches `matcher`, as in `IsList(SizeIs(2))`. The
// explanations of other GMock matchers in `matcher` start with the path of the
// list, even if they explain an item.
ValueMatcher IsList(testing::Matcher<const base::ListValue&> matcher);

// The matchers below match an update protocol request body, or an app in its
// `request.apps` list, that uses protocol V4 names.
// `request::GetJSONContentMatcher()` renames protocol V3 names in a request to
// their V4 names.

// Matches a body whose `request` dict matches `matcher`.
ValueMatcher UpdateRequest(ValueMatcher matcher);

// Matches a body with an app in `request.apps` whose `appid` equals `app_id`
// case-insensitively.
ValueMatcher HasApp(std::string app_id);

// Matches a body with an app in `request.apps` whose `appid` equals `app_id`
// case-insensitively and that matches `matcher`.
ValueMatcher HasApp(std::string app_id, ValueMatcher matcher);

// Matches an app with an event in `events` that matches `matcher`.
ValueMatcher HasEvent(ValueMatcher matcher);

}  // namespace updater::test::json

#endif  // CHROME_UPDATER_TEST_JSON_MATCHERS_H_
