// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/json_matchers.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <tuple>

#include "base/test/gtest_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater::test::json {

namespace {

using ::base::test::IsJson;
using ::base::test::ParseJson;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Each;
using ::testing::EndsWith;
using ::testing::Not;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrCaseEq;

// Returns the explanation that `matcher` gives for `value`.
std::string Explain(const ValueMatcher& matcher, const base::Value& value) {
  testing::StringMatchResultListener listener;
  std::ignore = matcher.MatchAndExplain(value, &listener);
  return listener.str();
}

// Returns the description of `matcher`.
std::string Describe(const ValueMatcher& matcher) {
  return testing::DescribeMatcher<const base::Value&>(matcher);
}

}  // namespace

TEST(JsonMatchersTest, HasKey) {
  const base::Value value = ParseJson(R"({"a": 1, "b": {"c": "x"}})");
  EXPECT_THAT(value, HasKey("a"));
  EXPECT_THAT(value, HasKey("a", 1));
  EXPECT_THAT(value, HasKey("b", HasKey("c", "x")));
  EXPECT_THAT(value, Not(HasKey("z")));
  EXPECT_THAT(value, Not(HasKey("a", 2)));
  EXPECT_THAT(base::Value(1), Not(HasKey("a")));

  EXPECT_EQ(Explain(HasKey("a", 2), value), ".a: is 1");
  EXPECT_EQ(Explain(HasKey("z"), value), ".z: is missing");
  EXPECT_EQ(Explain(HasKey("b", HasKey("c", "y")), value), R"(.b.c: is "x")");
  EXPECT_EQ(Explain(HasKey("a"), base::Value(1)), "which is not a dict");

  EXPECT_EQ(Describe(HasKey("a")), R"(has key "a")");
  EXPECT_EQ(Describe(Not(HasKey("a"))), R"(doesn't have key "a")");
  EXPECT_EQ(Describe(HasKey("c", "x")), R"(has key "c" that is equal to "x")");
  EXPECT_EQ(Describe(Not(HasKey("a", 2))),
            R"(doesn't have key "a", or has it with a value that )"
            "isn't equal to 2");
}

TEST(JsonMatchersTest, HasKeyComparesIntegersOfAnyType) {
  const base::Value value = ParseJson(R"({"a": 3, "big": 5000000000})");
  EXPECT_THAT(value, HasKey("a", 3u));
  EXPECT_THAT(value, HasKey("a", int64_t{3}));
  EXPECT_THAT(value, HasKey("a", size_t{3}));
  EXPECT_THAT(value, Not(HasKey("a", uint8_t{4})));
  EXPECT_THAT(value, Not(HasKey("a", 3.0)));
  // `base::JSONReader` parses an integer that doesn't fit in an `int` as a
  // `double`.
  EXPECT_THAT(value, HasKey("big", int64_t{5000000000}));
  EXPECT_THAT(value, HasKey("big", 5000000000.0));

  EXPECT_EQ(Describe(HasKey("a", int64_t{3})),
            R"(has key "a" that is equal to 3)");
  EXPECT_EQ(Describe(HasKey("big", int64_t{5000000000})),
            R"(has key "big" that is equal to 5000000000.0)");
}

TEST(JsonMatchersTest, HasKeyComparesJsonValues) {
  const base::Value value = ParseJson(R"({"a": {"b": [1, "x"]}, "n": null})");
  EXPECT_THAT(value, HasKey("a", ParseJson(R"({"b": [1, "x"]})")));
  EXPECT_THAT(value,
              HasKey("a", base::DictValue().Set(
                              "b", base::ListValue().Append(1).Append("x"))));
  EXPECT_THAT(value, HasPath("a.b", base::ListValue().Append(1).Append("x")));
  EXPECT_THAT(value, HasKey("n", base::Value()));
  EXPECT_THAT(value, Not(HasKey("a", ParseJson(R"({"b": [1]})"))));
  EXPECT_THAT(value,
              HasPath("a.b", AnyOf(IsJson(R"([1])"), IsJson(R"([1, "x"])"))));

  EXPECT_EQ(Explain(HasKey("a", ParseJson(R"({"b": [1]})")), value),
            R"(.a: is {"b":[1,"x"]})");
  EXPECT_EQ(Describe(HasKey("a", ParseJson(R"({"b": [1, "x"]})"))),
            R"(has key "a" that is equal to {"b":[1,"x"]})");
  EXPECT_EQ(Describe(HasKey("n", base::Value())),
            R"(has key "n" that is equal to null)");
}

TEST(JsonMatchersTest, LacksKey) {
  const base::Value value = ParseJson(R"({"a": 1})");
  EXPECT_THAT(value, LacksKey("b"));
  EXPECT_THAT(value, Not(LacksKey("a")));
  EXPECT_THAT(base::Value(1), Not(LacksKey("a")));

  EXPECT_EQ(Explain(LacksKey("a"), value), ".a: is 1, expected absent");
  EXPECT_EQ(Explain(LacksKey("b"), value), ".b: is missing");
  EXPECT_EQ(Explain(LacksKey("a"), base::Value(1)), "which is not a dict");
  EXPECT_EQ(Describe(LacksKey("a")), R"(doesn't have key "a")");
  EXPECT_EQ(Describe(Not(LacksKey("a"))), R"(isn't a dict, or has key "a")");
}

TEST(JsonMatchersTest, HasPath) {
  const base::Value value = ParseJson(R"({"a": {"b": {"c": true}}})");
  EXPECT_THAT(value, HasPath("a"));
  EXPECT_THAT(value, HasPath("a", HasKey("b")));
  EXPECT_THAT(value, Not(HasPath("z")));
  EXPECT_THAT(value, HasPath("a.b.c"));
  EXPECT_THAT(value, Not(HasPath("a.c")));
  EXPECT_THAT(value, HasPath("a.b.c", true));
  EXPECT_THAT(value, Not(HasPath("a.b.c", false)));
  EXPECT_THAT(value, Not(HasPath("a.x.c", true)));

  EXPECT_EQ(Explain(HasPath("z", 1), value), ".z: is missing");
  EXPECT_EQ(Explain(HasPath("a.b.c", false), value), ".a.b.c: is true");
  EXPECT_EQ(Explain(HasPath("a.x.c", true), value), ".a.x: is missing");
  EXPECT_EQ(Explain(HasPath("a.b.c.d"), value),
            ".a.b.c: is true, which is not a dict");
  EXPECT_EQ(Describe(HasPath("a", 1)), R"(has key "a" that is equal to 1)");
  EXPECT_EQ(Describe(HasPath("a.b", true)),
            R"(has key "a" that has key "b" that is equal to true)");
  EXPECT_EQ(Describe(HasPath("a.b")), R"(has key "a" that has key "b")");
  EXPECT_EQ(Describe(Not(HasPath("a.b", true))),
            R"(doesn't have key "a", or has it with a value that doesn't )"
            R"(have key "b", or has it with a value that isn't equal to true)");
}

TEST(JsonMatchersTest, HasPathChecksForEmptyKeys) {
  EXPECT_CHECK_DEATH(std::ignore = HasPath(""));
  EXPECT_CHECK_DEATH(std::ignore = HasPath("a..b", true));
}

TEST(JsonMatchersTest, IsString) {
  EXPECT_THAT(base::Value("1.2.3"), IsString(StartsWith("1.")));
  EXPECT_THAT(base::Value("2.0"), Not(IsString(StartsWith("1."))));
  EXPECT_THAT(base::Value("b"), IsString(AnyOf("a", "b")));
  EXPECT_THAT(base::Value(1), Not(IsString(_)));
  EXPECT_THAT(ParseJson(R"({"k": "value"})"),
              HasKey("k", IsString(StrCaseEq("VALUE"))));
  EXPECT_THAT(ParseJson(R"({"k": "other"})"),
              Not(HasKey("k", IsString(StrCaseEq("VALUE")))));

  EXPECT_EQ(Explain(IsString(_), base::Value(1)), "which is not a string");
  EXPECT_EQ(Describe(IsString("x")), R"(is a string that is equal to "x")");
  EXPECT_EQ(Describe(Not(IsString("x"))),
            R"(isn't a string, or is a string that isn't equal to "x")");
}

TEST(JsonMatchersTest, IsList) {
  const base::Value list = ParseJson("[1, 2]");
  EXPECT_THAT(list, IsList(SizeIs(2)));
  EXPECT_THAT(list, Not(IsList(SizeIs(3))));
  EXPECT_THAT(base::Value(1), Not(IsList(_)));

  EXPECT_EQ(Explain(IsList(_), base::Value(1)), "which is not a list");
  EXPECT_EQ(Describe(IsList(SizeIs(2))),
            "is a list that has a size that is equal to 2");
  EXPECT_EQ(Describe(Not(IsList(SizeIs(2)))),
            "isn't a list, or is a list that has a size that isn't equal to 2");
}

TEST(JsonMatchersTest, IsListExplainsItemsWithTheirPaths) {
  const base::Value value = ParseJson(R"({"a": [{"x": 1}, {}]})");
  EXPECT_EQ(Explain(HasKey("a", IsList(Each(HasKey("x")))), value),
            R"(.a: is [{"x":1},{}], whose element #1 doesn't match, )"
            ".a[1].x: is missing");
  EXPECT_EQ(
      Explain(HasKey("a", IsList(AllOf(Each(HasKey("x")), SizeIs(3)))), value),
      R"(.a: is [{"x":1},{}], whose element #1 doesn't match, )"
      ".a[1].x: is missing, and "
      R"(.a: is [{"x":1},{}], whose size 2 doesn't match)");
  EXPECT_EQ(Explain(IsList(Each(HasKey("x"))), ParseJson(R"([{"x": 1}, {}])")),
            "whose element #1 doesn't match, [1].x: is missing");
}

TEST(JsonMatchersTest, UpdateRequest) {
  const base::Value body = ParseJson(R"({"request": {"ismachine": true}})");
  EXPECT_THAT(body, UpdateRequest(HasKey("ismachine", true)));
  EXPECT_THAT(body, Not(UpdateRequest(HasKey("ismachine", false))));

  EXPECT_EQ(Explain(UpdateRequest(HasKey("ismachine", false)), body),
            ".request.ismachine: is true");
  EXPECT_EQ(Explain(UpdateRequest(HasKey("ismachine")),
                    ParseJson(R"({"request": 1})")),
            ".request: is 1, which is not a dict");
}

TEST(JsonMatchersTest, HasApp) {
  const base::Value body = ParseJson(R"({"request": {"apps": [
    "app1",
    {"version": "1.0"},
    {"appid": "{APP-1}", "version": "1.0"},
    {"appid": "app2", "version": "2.0"}
  ]}})");
  // App ids compare case-insensitively.
  EXPECT_THAT(body, HasApp("{app-1}"));
  EXPECT_THAT(body, HasApp("{app-1}", HasKey("version", "1.0")));
  EXPECT_THAT(body, HasApp("app2", HasKey("version", "2.0")));
  EXPECT_THAT(body, Not(HasApp("app2", HasKey("version", "1.0"))));
  // Only dict entries with an `appid` count.
  EXPECT_THAT(body, Not(HasApp("app1")));

  EXPECT_EQ(Explain(HasApp("app2", HasKey("version", "1.0")), body),
            R"(.request.apps[3].version: is "2.0")");
  EXPECT_EQ(Explain(HasApp("app3"), body),
            R"(.request.apps: has no app "app3", only "{APP-1}", "app2")");
  EXPECT_EQ(Explain(HasApp("app1"), ParseJson(R"({"request": {}})")),
            ".request.apps: is missing");
  EXPECT_EQ(Explain(HasApp("app1"), ParseJson(R"({"request": []})")),
            ".request: is [], which is not a dict");
  EXPECT_EQ(Explain(HasApp("app1"), ParseJson(R"({"request": {"apps": {}}})")),
            ".request.apps: is {}, which is not a list");
  EXPECT_EQ(Explain(HasApp("app1"), base::Value(1)), "which is not a dict");

  EXPECT_EQ(Describe(HasApp("app1", HasKey("version", "1.0"))),
            R"(has app "app1" that has key "version" that is equal to "1.0")");
  EXPECT_EQ(Describe(Not(HasApp("app1"))), R"(has no app "app1")");
}

TEST(JsonMatchersTest, HasAppWithRepeatedAppId) {
  const base::Value body = ParseJson(R"({"request": {"apps": [
    {"appid": "app1", "version": "1.0"},
    {"appid": "APP1", "version": "2.0"}
  ]}})");
  EXPECT_THAT(body, HasApp("app1", HasKey("version", "2.0")));
  EXPECT_THAT(body, Not(HasApp("app1", HasKey("version", "3.0"))));

  EXPECT_EQ(Explain(HasApp("app1", HasKey("version", "2.0")), body),
            R"(.request.apps[1].version: is "2.0")");
  EXPECT_EQ(Explain(HasApp("app1", HasKey("version", "3.0")), body),
            R"(.request.apps: no app "app1" matches: )"
            R"(.request.apps[0].version: is "1.0"; )"
            R"(.request.apps[1].version: is "2.0")");
}

TEST(JsonMatchersTest, HasEvent) {
  const base::Value app = ParseJson(R"({"appid": "app1", "events": [
    {"eventtype": 14},
    {"eventtype": 3, "eventresult": 1}
  ]})");
  EXPECT_THAT(app, HasEvent(HasKey("eventtype", 3)));
  EXPECT_THAT(app,
              HasEvent(AllOf(HasKey("eventtype", 3), LacksKey("errorcat"))));
  EXPECT_THAT(app, Not(HasEvent(HasKey("eventtype", 2))));

  EXPECT_EQ(Explain(HasEvent(HasKey("eventtype", 2)), app),
            ".events: no event matches: .events[0].eventtype: is 14; "
            ".events[1].eventtype: is 3");
  EXPECT_EQ(Explain(HasEvent(_), ParseJson(R"({"appid": "app1"})")),
            ".events: is missing");
  EXPECT_EQ(Explain(HasEvent(_), ParseJson(R"({"events": []})")),
            ".events: is empty");
  EXPECT_EQ(Describe(HasEvent(HasKey("eventtype", 2))),
            R"(has at least one event that has key "eventtype" that is equal )"
            "to 2");
}

TEST(JsonMatchersTest, ExplainsEachMismatchWithItsPath) {
  const base::Value body = ParseJson(R"({"request": {"apps": [
    {"appid": "test1", "ping": {"ad": -1}},
    {"appid": "test2", "ping": {"a": 2, "ad": 5098}}
  ]}})");
  const ValueMatcher matcher = AllOf(
      HasApp("test1", HasPath("ping.ad", -1)),
      HasApp("test2", HasKey("ping", AllOf(LacksKey("ad"), LacksKey("a")))));
  EXPECT_THAT(body, Not(matcher));

  EXPECT_EQ(Explain(matcher, body),
            ".request.apps[1].ping.ad: is 5098, expected absent, and "
            ".request.apps[1].ping.a: is 2, expected absent");
}

TEST(JsonMatchersTest, ExplainsAnyOfMismatchAtOnePath) {
  const base::Value body = ParseJson(R"({"request": {"apps": [
    {"appid": "app1", "installsource": "scheduler"}
  ]}})");
  EXPECT_EQ(
      Explain(HasApp("app1", HasKey("installsource",
                                    IsString(AnyOf("ondemand", "policy")))),
              body),
      R"(.request.apps[0].installsource: is "scheduler", )"
      R"(isn't equal to "ondemand", and isn't equal to "policy")");
}

TEST(JsonMatchersTest, ExplainsNestedListMismatchesWithTheirPaths) {
  const base::Value body = ParseJson(R"({"request": {"apps": [
    {"appid": "app1", "events": [
      {"eventtype": 14},
      {"eventtype": 2, "eventresult": 0}
    ]}
  ]}})");
  EXPECT_EQ(Explain(HasApp("app1", HasEvent(AllOf(HasKey("eventtype", 3),
                                                  HasKey("eventresult", 1)))),
                    body),
            ".request.apps[0].events: no event matches: "
            ".request.apps[0].events[0].eventtype: is 14, and "
            ".request.apps[0].events[0].eventresult: is missing; "
            ".request.apps[0].events[1].eventtype: is 2, and "
            ".request.apps[0].events[1].eventresult: is 0");
}

TEST(JsonMatchersTest, ExplainsMismatchesOfAValueAndItsKeysWithTheirPaths) {
  const base::Value value = ParseJson(R"({"ping": {"ad": 5}})");
  EXPECT_EQ(Explain(HasKey("ping", AllOf(LacksKey("ad"), IsList(_))), value),
            ".ping.ad: is 5, expected absent, and "
            R"(.ping: is {"ad":5}, which is not a list)");
  EXPECT_EQ(Explain(HasKey("ping", AllOf(IsString(_), HasKey("a"))), value),
            R"(.ping: is {"ad":5}, which is not a string, and )"
            ".ping.a: is missing");
  EXPECT_EQ(Explain(HasKey("ping", AllOf(HasKey("a"), LacksKey("ad"))), value),
            ".ping.a: is missing, and .ping.ad: is 5, expected absent");
}

TEST(JsonMatchersTest, ExplainsOtherMatchersWithTheirPaths) {
  const base::Value value = ParseJson(R"({"ping": {"ad": 5}})");
  const auto is_list = Property("is_list", &base::Value::is_list, true);
  EXPECT_EQ(Explain(HasKey("ping", AllOf(LacksKey("ad"), is_list)), value),
            ".ping.ad: is 5, expected absent, and "
            R"(.ping: is {"ad":5}, whose property `is_list` is false)");
  EXPECT_EQ(Explain(HasKey("ping", AllOf(is_list, LacksKey("ad"))), value),
            R"(.ping: is {"ad":5}, whose property `is_list` is false, and )"
            ".ping.ad: is 5, expected absent");
  EXPECT_EQ(Explain(HasKey("ping", AllOf(LacksKey("ad"), is_list, HasKey("a"))),
                    value),
            ".ping.ad: is 5, expected absent, and "
            R"(.ping: is {"ad":5}, whose property `is_list` is false, and )"
            ".ping.a: is missing");
  EXPECT_EQ(Explain(HasKey("ping", AnyOf(HasKey("a"), is_list)), value),
            ".ping.a: is missing, and "
            R"(.ping: is {"ad":5}, whose property `is_list` is false)");
  // GMock prints the value that the outermost matcher matches.
  EXPECT_EQ(Explain(AllOf(HasKey("a"), is_list), value),
            ".a: is missing, and whose property `is_list` is false");
}

TEST(JsonMatchersTest, ExplainsOtherMatchersOfAppsWithTheirPaths) {
  const base::Value body = ParseJson(R"({"request": {"apps": [
    {"appid": "app1", "version": "1.0"}
  ]}})");
  EXPECT_EQ(
      Explain(HasApp("app1",
                     AllOf(HasKey("version", "2.0"),
                           Property("is_list", &base::Value::is_list, true))),
              body),
      R"(.request.apps[0].version: is "1.0", and )"
      R"(.request.apps[0]: is {"appid":"app1","version":"1.0"}, )"
      "whose property `is_list` is false");
}

TEST(JsonMatchersTest, ExplainsCopiesWithPathsFromTheCopy) {
  EXPECT_THAT(Explain(HasKey("a", Property("clone", &base::Value::Clone,
                                           HasPath("x.y", 2))),
                      ParseJson(R"({"a": {"x": {"y": 1}}})")),
              EndsWith(", .x.y: is 1"));
}

TEST(JsonMatchersTest, ExplainsValuesThatLookLikeExplanations) {
  const base::Value value = ParseJson(R"({"b": {"a": "x, and .c: is 1"}})");
  EXPECT_EQ(Explain(HasKey("b", HasKey("a", "y")), value),
            R"(.b.a: is "x, and .c: is 1")");
}

TEST(JsonMatchersTest, ShortensLongValues) {
  // 150 two-byte characters, which JSON quotes to 302 bytes.
  std::string long_string;
  for (int i = 0; i < 150; ++i) {
    long_string += "\u00e9";
  }
  const base::Value value = ParseJson(R"({"a": ")" + long_string + R"("})");
  std::string expected = R"(.a: is ")";
  for (int i = 0; i < 99; ++i) {
    expected += "\xC3\xA9";
  }
  expected += "...";
  EXPECT_EQ(Explain(HasKey("a", 1), value), expected);
}

}  // namespace updater::test::json
