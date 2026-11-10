// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/request_matcher.h"

#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/test/http_request.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater::test::request {

namespace {

// Returns a `multipart/form-data` body with one part for each `(name, data)`
// pair in `forms`, followed by the close delimiter.
std::string MakeMultipartBody(
    std::string_view boundary,
    const std::vector<std::pair<std::string, std::string>>& forms) {
  std::string body;
  for (const auto& [name, data] : forms) {
    base::StrAppend(&body, {"--", boundary, "\r\n",
                            R"(Content-Disposition: form-data; name=")", name,
                            "\"\r\n\r\n", data, "\r\n"});
  }
  base::StrAppend(&body, {"--", boundary, "--\r\n"});
  return body;
}

// Returns success if `matcher` rejects `request` with a message that contains
// `expected_message`.
testing::AssertionResult FailsWithMessage(const Matcher& matcher,
                                          const HttpRequest& request,
                                          std::string_view expected_message) {
  const testing::AssertionResult result = matcher.Run(request);
  if (result) {
    return testing::AssertionFailure() << "The matcher unexpectedly succeeded.";
  }
  if (std::string_view(result.message()).find(expected_message) ==
      std::string_view::npos) {
    return testing::AssertionFailure()
           << "Message [" << result.message() << "] does not contain ["
           << expected_message << "].";
  }
  return testing::AssertionSuccess();
}

// Substring of the `GetAppJSONContentMatcher()` failure message for an app list
// without a matching entry.
constexpr char kAppJSONMismatch[] = "not found in (parsed) request app list";

// Returns a matcher for app `{app-1}` with an event of type 3.
Matcher MakeApp1EventsMatcher() {
  return GetAppJSONContentMatcher(
      base::DictValue()
          .Set("appid", "{app-1}")
          .Set("events", base::ListValue().Append(
                             base::DictValue().Set("eventtype", 3))));
}

}  // namespace

TEST(RequestMatcherTest, GetPathMatcher) {
  Matcher matcher = GetPathMatcher(R"(/update\?id=\d+)");

  HttpRequest request;
  EXPECT_FALSE(matcher.Run(request));

  request.relative_url = "/update?id=123";
  EXPECT_TRUE(matcher.Run(request));

  request.relative_url = "/update?id=abc";
  EXPECT_FALSE(matcher.Run(request));

  request.relative_url = "/other/update?id=123";
  EXPECT_FALSE(matcher.Run(request));
}

TEST(RequestMatcherTest, GetHeaderMatcher) {
  Matcher matcher = GetHeaderMatcher(
      {{"X-Cup-Server-Proof", "proof-[0-9]+"}, {"X-Custom", "expected-value"}});

  HttpRequest request;
  EXPECT_TRUE(FailsWithMessage(
      matcher, request, "Request header 'X-Cup-Server-Proof' not found"));

  request.headers["X-Cup-Server-Proof"] = "PROOF-42";
  EXPECT_TRUE(FailsWithMessage(matcher, request,
                               "Request header 'X-Custom' not found"));

  request.headers["X-Custom"] = "EXPECTED-VALUE";
  EXPECT_TRUE(matcher.Run(request));

  request.headers["X-Custom"] = "unexpected-value";
  EXPECT_TRUE(FailsWithMessage(
      matcher, request, "Request header [X-Custom = 'unexpected-value'"));
}

TEST(RequestMatcherTest, GetUpdaterUserAgentMatcher) {
  const base::Version version("1.2.3.4");
  Matcher matcher = GetUpdaterUserAgentMatcher(version);

  HttpRequest request;
  EXPECT_TRUE(FailsWithMessage(matcher, request,
                               "Request header 'User-Agent' not found"));

  request.headers["User-Agent"] = GetUpdaterUserAgent(version);
  EXPECT_TRUE(matcher.Run(request));

  // Expects the matcher to reject `user_agent` because of its value.
  auto expect_rejected = [&](const std::string& user_agent) {
    request.headers["User-Agent"] = user_agent;
    EXPECT_TRUE(FailsWithMessage(
        matcher, request,
        base::StrCat({"Request header [User-Agent = '", user_agent, "'"})))
        << user_agent;
  };

  // Regex metacharacters in the updater `User-Agent` are escaped.
  std::string regex_lookalike_user_agent;
  base::ReplaceChars(GetUpdaterUserAgent(version), ".", "x",
                     &regex_lookalike_user_agent);
  expect_rejected(regex_lookalike_user_agent);

  // The test `User-Agent` matches on the third version component.
  request.headers["User-Agent"] =
      base::StrCat({PRODUCT_FULLNAME_STRING, "_test/3"});
  EXPECT_TRUE(matcher.Run(request));

  request.headers["User-Agent"] =
      base::StrCat({PRODUCT_FULLNAME_STRING, "_test/3.0"});
  EXPECT_TRUE(matcher.Run(request));

  request.headers["User-Agent"] =
      base::StrCat({PRODUCT_FULLNAME_STRING, "_test/3 CFNetwork/1.0"});
  EXPECT_TRUE(matcher.Run(request));

  expect_rejected(base::StrCat({PRODUCT_FULLNAME_STRING, "_test/31.0"}));
  expect_rejected(base::StrCat({PRODUCT_FULLNAME_STRING, "_test/4.0"}));
  expect_rejected("OtherAgent/1.0");
}

TEST(RequestMatcherTest, GetUpdaterUserAgentMatcherRequiresThreeComponents) {
  EXPECT_CHECK_DEATH(std::ignore =
                         GetUpdaterUserAgentMatcher(base::Version("1.2")));
}

TEST(RequestMatcherTest, GetTargetURLMatcher) {
  Matcher matcher =
      GetTargetURLMatcher(GURL("https://example.com/update/check"));

  HttpRequest request;
  EXPECT_FALSE(matcher.Run(request));

  request.all_headers =
      "POST https://example.com/update/check HTTP/1.1\r\n"
      "Host: example.com\r\n";
  request.headers["Host"] = "example.com";
  EXPECT_TRUE(matcher.Run(request));

  request.headers["Host"] = "other.example.com";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request header [Host"));

  // Regex metacharacters in the host are escaped.
  request.headers["Host"] = "exampleXcom";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request header [Host"));

  // The `Host` header must match the host exactly, without a default port.
  request.headers["Host"] = "example.com:443";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request header [Host"));

  request.headers["Host"] = "example.com";

  // The query of the request target is ignored.
  request.all_headers =
      "POST https://example.com/update/check?x=1 HTTP/1.1\r\n";
  EXPECT_TRUE(matcher.Run(request));

  // Scheme and host are compared after canonicalization.
  request.all_headers =
      "POST HTTPS://EXAMPLE.COM:443/update/check HTTP/1.1\r\n";
  EXPECT_TRUE(matcher.Run(request));

  // The path must match exactly, not just as a prefix.
  request.all_headers = "POST https://example.com/update/check2 HTTP/1.1\r\n";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request target ["));

  request.all_headers = "POST https://example.com/update HTTP/1.1\r\n";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request target ["));

  request.all_headers = "POST https://example.com/UPDATE/CHECK HTTP/1.1\r\n";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request target ["));

  request.all_headers = "POST http://example.com/update/check HTTP/1.1\r\n";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request target ["));

  request.all_headers =
      "POST https://example.com:8443/update/check HTTP/1.1\r\n";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request target ["));

  request.all_headers = "POST https://other.com/update/check HTTP/1.1\r\n";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request target ["));

  request.all_headers = "POST not-a-url HTTP/1.1\r\n";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request target ["));

  // Requests other than `POST` and malformed request lines fail.
  request.all_headers = "GET https://example.com/update/check HTTP/1.1\r\n";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "`POST` request line"));

  request.all_headers = "POST https://example.com/update/check\r\n";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "`POST` request line"));
}

TEST(RequestMatcherTest, GetTargetURLMatcherRequiresValidURL) {
  EXPECT_CHECK_DEATH(std::ignore = GetTargetURLMatcher(GURL()));
  EXPECT_CHECK_DEATH(std::ignore = GetTargetURLMatcher(GURL("not-a-url")));
}

TEST(RequestMatcherTest, GetTargetURLMatcherRejectsQueryAndFragment) {
  EXPECT_CHECK_DEATH(std::ignore = GetTargetURLMatcher(
                         GURL("https://example.com/dm/api?request=policy")));
  EXPECT_CHECK_DEATH(std::ignore = GetTargetURLMatcher(
                         GURL("https://example.com/dm/api#fragment")));
}

TEST(RequestMatcherTest, GetTargetURLMatcherWithPort) {
  Matcher matcher = GetTargetURLMatcher(GURL("http://localhost:1234/ping"));

  HttpRequest request;
  request.all_headers = "POST http://localhost:1234/ping HTTP/1.1\r\n";
  request.headers["Host"] = "localhost:1234";
  EXPECT_TRUE(matcher.Run(request));

  // A non-default port must be present in the `Host` header.
  request.headers["Host"] = "localhost";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request header [Host"));

  request.headers["Host"] = "localhost:1235";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Request header [Host"));
}

TEST(RequestMatcherTest, GetContentMatcher) {
  Matcher matcher = GetContentMatcher({"first.*token", "second-[0-9]+"});

  HttpRequest request;
  EXPECT_FALSE(matcher.Run(request));

  request.decoded_content = "prefix FIRST_abc_TOKEN middle SECOND-99 suffix";
  EXPECT_TRUE(matcher.Run(request));

  request.decoded_content = "SECOND-99 before FIRST_abc_TOKEN";
  EXPECT_FALSE(matcher.Run(request));

  request.decoded_content = "only FIRST_abc_TOKEN present";
  EXPECT_FALSE(matcher.Run(request));
}

TEST(RequestMatcherTest, GetJSONContentMatcher) {
  Matcher matcher = GetJSONContentMatcher(base::DictValue().Set(
      "request",
      base::DictValue()
          .Set("protocol", "4.0")
          .Set("apps", base::ListValue().Append(
                           base::DictValue().Set("appid", "{app-1}")))));

  HttpRequest request;
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Could not parse"));
  EXPECT_TRUE(FailsWithMessage(matcher, request, "{app-1}"));

  request.decoded_content = "not valid json";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Could not parse"));

  request.decoded_content = R"({
    "request": {
      "protocol": "4.0",
      "extra": true,
      "apps": [
        {"appid": "{other}"},
        {"appid": "{APP-1}", "version": "1.0"}
      ]
    }
  })";
  EXPECT_TRUE(matcher.Run(request));

  request.decoded_content = R"({
    "request": {
      "protocol": "3.1",
      "apps": [{"appid": "{app-1}"}]
    }
  })";
  EXPECT_TRUE(FailsWithMessage(matcher, request,
                               "not found in (parsed) request content"));
}

TEST(RequestMatcherTest, GetAppJSONContentMatcherRequiresAppList) {
  Matcher matcher = MakeApp1EventsMatcher();

  // The failure messages name the expected app.
  HttpRequest request;
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Could not parse"));
  EXPECT_TRUE(FailsWithMessage(matcher, request, "{app-1}"));

  request.decoded_content = R"({"request": {}})";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "No `request.apps`"));
  EXPECT_TRUE(FailsWithMessage(matcher, request, "{app-1}"));
}

TEST(RequestMatcherTest, GetAppJSONContentMatcherMatchesV4) {
  Matcher matcher = MakeApp1EventsMatcher();

  // Any app may match, and app ids compare case-insensitively.
  HttpRequest request;
  request.decoded_content = R"({"request": {"apps": [
    {"appid": "{other}"},
    {"appid": "{APP-1}", "events": [{"eventtype": 14}, {"eventtype": 3}]}
  ]}})";
  EXPECT_TRUE(matcher.Run(request));

  request.decoded_content = R"({"request": {"apps": [
    {"appid": "{app-1}", "events": [{"eventtype": 14}]}
  ]}})";
  EXPECT_TRUE(FailsWithMessage(matcher, request, kAppJSONMismatch));

  // `events` in the needle does not match `event` in a V4 entry.
  request.decoded_content = R"({"request": {"apps": [
    {"appid": "{app-1}", "event": [{"eventtype": 3}]}
  ]}})";
  EXPECT_TRUE(FailsWithMessage(matcher, request, kAppJSONMismatch));
}

TEST(RequestMatcherTest, GetAppJSONContentMatcherMatchesV3) {
  Matcher matcher = MakeApp1EventsMatcher();

  // Protocol V3 uses `request.app` and `event`.
  HttpRequest request;
  request.decoded_content = R"({"request": {"app": [
    {"appid": "{app-1}", "event": [{"eventtype": 3}]}
  ]}})";
  EXPECT_TRUE(matcher.Run(request));

  request.decoded_content = R"({"request": {"app": [
    {"appid": "{app-1}", "event": [{"eventtype": 2}]}
  ]}})";
  EXPECT_TRUE(FailsWithMessage(matcher, request, kAppJSONMismatch));

  // `events` in the needle does not match `events` in a V3 entry.
  request.decoded_content = R"({"request": {"app": [
    {"appid": "{app-1}", "events": [{"eventtype": 3}]}
  ]}})";
  EXPECT_TRUE(FailsWithMessage(matcher, request, kAppJSONMismatch));
}

TEST(RequestMatcherTest, GetAppJSONContentMatcherWithoutEvents) {
  // A needle without events matches either protocol.
  Matcher matcher =
      GetAppJSONContentMatcher(base::DictValue().Set("appid", "{app-1}"));

  HttpRequest request;
  request.decoded_content = R"({"request": {"app": [{"appid": "{app-1}"}]}})";
  EXPECT_TRUE(matcher.Run(request));
  request.decoded_content = R"({"request": {"apps": [{"appid": "{app-1}"}]}})";
  EXPECT_TRUE(matcher.Run(request));
  request.decoded_content = R"({"request": {"apps": [{"appid": "{app-2}"}]}})";
  EXPECT_TRUE(FailsWithMessage(matcher, request, kAppJSONMismatch));
}

TEST(RequestMatcherTest, GetAppJSONContentMatcherSkipsNonDictEntries) {
  Matcher matcher = MakeApp1EventsMatcher();

  HttpRequest request;
  request.decoded_content = R"({"request": {"apps": [
    "{app-1}", 3, null, ["{app-1}"],
    {"appid": "{app-1}", "events": [{"eventtype": 3}]}
  ]}})";
  EXPECT_TRUE(matcher.Run(request));

  request.decoded_content = R"({"request": {"apps": ["{app-1}"]}})";
  EXPECT_TRUE(FailsWithMessage(matcher, request, kAppJSONMismatch));
}

TEST(RequestMatcherTest, GetAppJSONContentMatcherPrefersAppsList) {
  Matcher matcher = MakeApp1EventsMatcher();
  Matcher appid_matcher =
      GetAppJSONContentMatcher(base::DictValue().Set("appid", "{app-1}"));

  // If both lists are present, the matcher searches only `request.apps`, with
  // the protocol V4 needle.
  HttpRequest request;
  request.decoded_content = R"({"request": {
    "apps": [{"appid": "{app-2}"}],
    "app": [{"appid": "{app-1}"}]
  }})";
  EXPECT_TRUE(FailsWithMessage(appid_matcher, request, kAppJSONMismatch));
  request.decoded_content = R"({"request": {
    "apps": [{"appid": "{app-1}", "events": [{"eventtype": 3}]}],
    "app": [{"appid": "{app-2}"}]
  }})";
  EXPECT_TRUE(matcher.Run(request));

  // A non-list `request.apps` falls back to `request.app`.
  request.decoded_content = R"({"request": {
    "apps": null,
    "app": [{"appid": "{app-1}", "event": [{"eventtype": 3}]}]
  }})";
  EXPECT_TRUE(matcher.Run(request));
}

TEST(RequestMatcherTest, GetAppJSONContentMatcherNullValueForbidsKey) {
  // A null dict value requires the entry to omit the key or set it to null.
  Matcher matcher = GetAppJSONContentMatcher(
      base::DictValue()
          .Set("appid", "{app-1}")
          .Set("events",
               base::ListValue().Append(base::DictValue()
                                            .Set("eventtype", 3)
                                            .Set("errorcat", base::Value()))));

  HttpRequest request;
  request.decoded_content = R"({"request": {"apps": [
    {"appid": "{app-1}", "events": [{"eventtype": 3}]}
  ]}})";
  EXPECT_TRUE(matcher.Run(request));
  request.decoded_content = R"({"request": {"apps": [
    {"appid": "{app-1}", "events": [{"eventtype": 3, "errorcat": null}]}
  ]}})";
  EXPECT_TRUE(matcher.Run(request));
  request.decoded_content = R"({"request": {"apps": [
    {"appid": "{app-1}", "events": [{"eventtype": 3, "errorcat": 1}]}
  ]}})";
  EXPECT_TRUE(FailsWithMessage(matcher, request, kAppJSONMismatch));
  request.decoded_content = R"({"request": {"app": [
    {"appid": "{app-1}", "event": [{"eventtype": 3}]}
  ]}})";
  EXPECT_TRUE(matcher.Run(request));
  request.decoded_content = R"({"request": {"app": [
    {"appid": "{app-1}", "event": [{"eventtype": 3, "errorcat": 1}]}
  ]}})";
  EXPECT_TRUE(FailsWithMessage(matcher, request, kAppJSONMismatch));
}

TEST(RequestMatcherTest, GetAppJSONContentMatcherRejectsV3EventKey) {
  EXPECT_CHECK_DEATH(std::ignore = GetAppJSONContentMatcher(
                         base::DictValue().Set("event", base::ListValue())));
}

TEST(RequestMatcherTest, GetScopeMatcher) {
  Matcher system_matcher = GetScopeMatcher(UpdaterScope::kSystem);
  Matcher user_matcher = GetScopeMatcher(UpdaterScope::kUser);

  HttpRequest request;
  EXPECT_FALSE(system_matcher.Run(request));
  EXPECT_FALSE(user_matcher.Run(request));

  request.decoded_content = "not json";
  EXPECT_FALSE(system_matcher.Run(request));
  EXPECT_FALSE(user_matcher.Run(request));

  request.decoded_content = R"({"request": {}})";
  EXPECT_FALSE(system_matcher.Run(request));
  EXPECT_FALSE(user_matcher.Run(request));

  request.decoded_content = R"({"request": {"ismachine": true}})";
  EXPECT_TRUE(system_matcher.Run(request));
  EXPECT_FALSE(user_matcher.Run(request));

  request.decoded_content = R"({"request": {"ismachine": false}})";
  EXPECT_FALSE(system_matcher.Run(request));
  EXPECT_TRUE(user_matcher.Run(request));
}

TEST(RequestMatcherTest, GetAppPriorityMatcher) {
  Matcher fg_matcher =
      GetAppPriorityMatcher("app1", UpdateService::Priority::kForeground);
  Matcher bg_matcher =
      GetAppPriorityMatcher("app1", UpdateService::Priority::kBackground);
  static constexpr char kAppMismatch[] = "Request does not match app_id [app1]";

  HttpRequest request;
  EXPECT_TRUE(FailsWithMessage(fg_matcher, request, kAppMismatch));
  EXPECT_TRUE(FailsWithMessage(bg_matcher, request, kAppMismatch));

  request.decoded_content = R"({"request": {}})";
  EXPECT_TRUE(FailsWithMessage(fg_matcher, request, kAppMismatch));
  EXPECT_TRUE(FailsWithMessage(bg_matcher, request, kAppMismatch));

  // Foreground install sources ("ondemand", "taggedmi", "policy") match
  // `Priority::kForeground`.
  request.decoded_content = R"({
    "request": {
      "apps": [
        {},
        {"appid": "app1", "installsource": "ondemand"}
      ]
    }
  })";
  EXPECT_TRUE(fg_matcher.Run(request));
  EXPECT_TRUE(FailsWithMessage(bg_matcher, request, kAppMismatch));

  // Protocol V3 uses `request.app`.
  request.decoded_content = R"({
    "request": {
      "app": [
        {"appid": "app1", "installsource": "policy"}
      ]
    }
  })";
  EXPECT_TRUE(fg_matcher.Run(request));
  EXPECT_TRUE(FailsWithMessage(bg_matcher, request, kAppMismatch));

  // Other install sources match `Priority::kBackground`.
  request.decoded_content = R"({
    "request": {
      "apps": [
        {"appid": "app1", "installsource": "scheduler"}
      ]
    }
  })";
  EXPECT_TRUE(FailsWithMessage(fg_matcher, request, kAppMismatch));
  EXPECT_TRUE(bg_matcher.Run(request));

  // Missing `installsource` or absent `app1` matches `Priority::kBackground`.
  request.decoded_content = R"({
    "request": {
      "apps": [
        {"appid": "app1"}
      ]
    }
  })";
  EXPECT_TRUE(FailsWithMessage(fg_matcher, request, kAppMismatch));
  EXPECT_TRUE(bg_matcher.Run(request));

  request.decoded_content = R"({
    "request": {
      "apps": [
        {"appid": "other_app", "installsource": "ondemand"}
      ]
    }
  })";
  EXPECT_TRUE(FailsWithMessage(fg_matcher, request, kAppMismatch));
  EXPECT_TRUE(bg_matcher.Run(request));

  // The app id is compared case-insensitively.
  request.decoded_content = R"({
    "request": {
      "apps": [
        {"appid": "APP1", "installsource": "ondemand"}
      ]
    }
  })";
  EXPECT_TRUE(fg_matcher.Run(request));
  EXPECT_TRUE(FailsWithMessage(bg_matcher, request, kAppMismatch));
}

TEST(RequestMatcherTest, GetUpdaterEnableUpdatesMatcher) {
  Matcher matcher = GetUpdaterEnableUpdatesMatcher();

  HttpRequest request;
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Could not parse"));

  request.decoded_content = R"({"request": {}})";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "No `request.apps`"));

  request.decoded_content = R"({"request": {"apps": [{}]}})";
  EXPECT_TRUE(matcher.Run(request));

  request.decoded_content =
      base::StrCat({R"({"request": {"apps": [{}, {"appid": ")", kUpdaterAppId,
                    R"(", "updatecheck": {"updatedisabled": false}}]}})"});
  EXPECT_TRUE(matcher.Run(request));

  static constexpr char kUpdateDisabled[] = "Update is wrongfully disabled";
  request.decoded_content =
      base::StrCat({R"({"request": {"apps": [{"appid": ")", kUpdaterAppId,
                    R"(", "updatecheck": {"updatedisabled": true}}]}})"});
  EXPECT_TRUE(FailsWithMessage(matcher, request, kUpdateDisabled));

  // The updater app id is compared case-insensitively.
  request.decoded_content =
      base::StrCat({R"({"request": {"apps": [{"appid": ")",
                    base::ToUpperASCII(kUpdaterAppId),
                    R"(", "updatecheck": {"updatedisabled": true}}]}})"});
  EXPECT_TRUE(FailsWithMessage(matcher, request, kUpdateDisabled));

  // Protocol V3 uses `request.app`.
  request.decoded_content =
      base::StrCat({R"({"request": {"app": [{"appid": ")", kUpdaterAppId,
                    R"(", "updatecheck": {"updatedisabled": true}}]}})"});
  EXPECT_TRUE(FailsWithMessage(matcher, request, kUpdateDisabled));
}

TEST(RequestMatcherTest, GetMultipartContentMatcher) {
  Matcher matcher = GetMultipartContentMatcher(
      {FormExpectations("first.field+1", {"alpha.*1", "beta"}),
       FormExpectations("second_field", {"gamma"})});

  HttpRequest request;
  EXPECT_TRUE(
      FailsWithMessage(matcher, request, "Content-Type header not found"));

  request.headers["Content-Type"] = "application/json";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "[multipart/form-data]"));

  request.headers["Content-Type"] = "multipart/form-data; boundary=";
  EXPECT_TRUE(FailsWithMessage(matcher, request, "empty multipart boundary"));

  request.headers["Content-Type"] = "multipart/form-data; boundary=BOUNDARY+1";
  request.decoded_content = "no multipart delimiter";
  EXPECT_TRUE(FailsWithMessage(matcher, request,
                               "No multipart delimiter [--BOUNDARY+1]"));

  request.decoded_content =
      MakeMultipartBody("BOUNDARY+1", {{"first.field+1", "ALPHA_value_1 BETA"},
                                       {"ignored_field", "ignored content"},
                                       {"second_field", "GAMMA"}});
  EXPECT_TRUE(matcher.Run(request));

  // A preamble before the first delimiter is skipped.
  request.decoded_content =
      base::StrCat({"preamble\r\n", request.decoded_content});
  EXPECT_TRUE(matcher.Run(request));

  // The media type is case-insensitive, and the boundary may be quoted.
  request.headers["Content-Type"] =
      R"(Multipart/Form-Data; boundary="BOUNDARY+1")";
  EXPECT_TRUE(matcher.Run(request));
  request.headers["Content-Type"] = "multipart/form-data; boundary=BOUNDARY+1";

  // A boundary ends a part's data only when it follows CRLF and `--`.
  request.decoded_content = MakeMultipartBody(
      "BOUNDARY+1", {{"first.field+1", "ALPHA_1 --BOUNDARY+1 BETA"},
                     {"second_field", "GAMMA"}});
  EXPECT_TRUE(matcher.Run(request));

  // Regexes from the first form must not match into the second form.
  request.decoded_content = MakeMultipartBody(
      "BOUNDARY+1",
      {{"first.field+1", "ALPHA_value_1"}, {"second_field", "BETA GAMMA"}});
  EXPECT_TRUE(
      FailsWithMessage(matcher, request, "Form [first.field+1] match failed"));

  // Wrong form name fails.
  request.decoded_content = MakeMultipartBody(
      "BOUNDARY+1",
      {{"wrong_field", "ALPHA_value_1 BETA"}, {"second_field", "GAMMA"}});
  EXPECT_TRUE(
      FailsWithMessage(matcher, request, "Form [first.field+1] not found"));

  // Form names are matched in part headers only, not in the data of a part.
  request.decoded_content = MakeMultipartBody(
      "BOUNDARY+1",
      {{"ignored_field",
        R"(Content-Disposition: form-data; name="first.field+1" ALPHA_1 BETA)"},
       {"second_field", "GAMMA"}});
  EXPECT_TRUE(
      FailsWithMessage(matcher, request, "Form [first.field+1] not found"));

  // Forms must appear in order.
  request.decoded_content = MakeMultipartBody(
      "BOUNDARY+1",
      {{"second_field", "GAMMA"}, {"first.field+1", "ALPHA_value_1 BETA"}});
  EXPECT_TRUE(
      FailsWithMessage(matcher, request, "Form [second_field] not found"));
}

TEST(RequestMatcherTest, GetMultipartContentMatcherRequiresCloseDelimiter) {
  Matcher matcher =
      GetMultipartContentMatcher({FormExpectations("a", {"alpha"})});

  HttpRequest request;
  request.headers["Content-Type"] = "multipart/form-data; boundary=xyz42";
  const std::string body = MakeMultipartBody("xyz42", {{"a", "ALPHA"}});
  request.decoded_content = body;
  EXPECT_TRUE(matcher.Run(request));

  // Missing close delimiter.
  base::ReplaceFirstSubstringAfterOffset(&request.decoded_content, 0,
                                         "--xyz42--\r\n", "");
  EXPECT_TRUE(FailsWithMessage(matcher, request, "close delimiter"));

  // An opening delimiter in place of the close delimiter.
  request.decoded_content = body;
  base::ReplaceFirstSubstringAfterOffset(&request.decoded_content, 0,
                                         "--xyz42--", "--xyz42");
  EXPECT_TRUE(FailsWithMessage(matcher, request, "close delimiter"));

  // A delimiter followed by neither CRLF nor `--`.
  request.decoded_content = body;
  base::ReplaceFirstSubstringAfterOffset(&request.decoded_content, 0,
                                         "--xyz42\r\n", "--xyz42 ");
  EXPECT_TRUE(FailsWithMessage(matcher, request, "is not followed by CRLF"));
}

// A form's regexes must match within that form, even when a later form would
// satisfy them and the remaining structure is otherwise valid.
TEST(RequestMatcherTest, GetMultipartContentMatcherConfinesRegexesToForm) {
  Matcher matcher =
      GetMultipartContentMatcher({FormExpectations("a", {"alpha", "beta"})});

  HttpRequest request;
  request.headers["Content-Type"] = "multipart/form-data; boundary=xyz42";
  request.decoded_content =
      MakeMultipartBody("xyz42", {{"a", "ALPHA"}, {"b", "BETA"}});
  EXPECT_TRUE(FailsWithMessage(matcher, request, "Form [a] match failed"));
  // The failure message shows only the data of the failing form.
  EXPECT_TRUE(FailsWithMessage(matcher, request, "ALPHA]"));

  request.decoded_content =
      MakeMultipartBody("xyz42", {{"a", "ALPHA BETA"}, {"b", "other"}});
  EXPECT_TRUE(matcher.Run(request));
}

// Regexes can match the rest of the part headers after the form name.
TEST(RequestMatcherTest, GetMultipartContentMatcherMatchesPartHeaders) {
  Matcher matcher = GetMultipartContentMatcher(
      {FormExpectations("upload_file_minidump",
                        {R"(filename=".*dmp")",
                         "Content-Type: application/octet-stream", "MDMP"})});

  HttpRequest request;
  request.headers["Content-Type"] = "multipart/form-data; boundary=xyz42";
  request.decoded_content = base::StrCat(
      {"--xyz42\r\n",
       R"(Content-Disposition: form-data; name="upload_file_minidump"; )",
       R"(filename="crash.dmp")", "\r\n",
       "Content-Type: application/octet-stream\r\n\r\n", "MDMP\x01\x02\r\n",
       "--xyz42--\r\n"});
  EXPECT_TRUE(matcher.Run(request));
}

TEST(RequestMatcherTest, InvalidRegexes) {
  EXPECT_CHECK_DEATH(std::ignore = GetPathMatcher("("));
  EXPECT_CHECK_DEATH(std::ignore = GetHeaderMatcher({{"X-Custom", "("}}));
  EXPECT_CHECK_DEATH(std::ignore = GetContentMatcher({"valid", "("}));
  EXPECT_CHECK_DEATH(std::ignore = GetMultipartContentMatcher(
                         {FormExpectations("a", {"valid", "("})}));
}

}  // namespace updater::test::request
