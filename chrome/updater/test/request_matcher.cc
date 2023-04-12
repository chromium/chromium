// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/request_matcher.h"

#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"

namespace updater::test::request {

Matcher GetPathMatcher(const std::string& expected_path_regex) {
  return base::BindLambdaForTesting(
      [expected_path_regex](const net::test_server::HttpRequest& request) {
        if (!re2::RE2::FullMatch(request.relative_url, expected_path_regex)) {
          ADD_FAILURE() << "Request path [" << request.relative_url
                        << "], did not match expected path regex ["
                        << expected_path_regex << "].";
          return false;
        }
        return true;
      });
}

Matcher GetHeaderMatcher(const std::string& header_name,
                         const std::string& expected_header_regex) {
  return base::BindLambdaForTesting(
      [header_name,
       expected_header_regex](const net::test_server::HttpRequest& request) {
        re2::RE2::Options opt;
        opt.set_case_sensitive(false);
        net::test_server::HttpRequest::HeaderMap::const_iterator it =
            request.headers.find(header_name);
        if (it == request.headers.end()) {
          ADD_FAILURE() << "Request header '" << header_name
                        << "' not found, expected regex "
                        << expected_header_regex;
          return false;
        } else if (!re2::RE2::FullMatch(it->second,
                                        re2::RE2(expected_header_regex, opt))) {
          ADD_FAILURE() << "Request header [" << it->first << " = '"
                        << it->second << "], did not match expected regex ["
                        << expected_header_regex << "]";
          return false;
        }
        return true;
      });
}

Matcher GetContentMatcher(const std::string& expected_content_regex) {
  return base::BindLambdaForTesting(
      [expected_content_regex](const net::test_server::HttpRequest& request) {
        re2::RE2::Options opt;
        opt.set_case_sensitive(false);
        if (!re2::RE2::PartialMatch(request.content,
                                    re2::RE2(expected_content_regex, opt))) {
          ADD_FAILURE() << "Request content match failed. body: ["
                        << request.content
                        << "] did not match expected regex: ["
                        << expected_content_regex << "]";
          return false;
        }
        return true;
      });
}

Matcher GetScopeMatcher(UpdaterScope scope) {
  return base::BindLambdaForTesting(
      [scope](const net::test_server::HttpRequest& request) {
        const bool is_match = [&scope, &request]() {
          const absl::optional<base::Value> doc =
              base::JSONReader::Read(request.content);
          if (!doc || !doc->is_dict()) {
            return false;
          }
          const base::Value::Dict* object_request =
              doc->GetDict().FindDict("request");
          if (!object_request) {
            return false;
          }
          absl::optional<bool> ismachine =
              object_request->FindBool("ismachine");
          if (!ismachine.has_value()) {
            return false;
          }
          switch (scope) {
            case UpdaterScope::kSystem:
              return *ismachine;
            case UpdaterScope::kUser:
              return !*ismachine;
          }
        }();
        if (!is_match) {
          ADD_FAILURE() << R"(Request does not match "ismachine": )"
                        << request.content;
        }
        return is_match;
      });
}

Matcher GetAppPriorityMatcher(const std::string& app_id,
                              UpdateService::Priority priority) {
  return base::BindLambdaForTesting(
      [app_id, priority](const net::test_server::HttpRequest& request) {
        const bool is_match = [&app_id, priority, &request]() {
          const absl::optional<base::Value> doc =
              base::JSONReader::Read(request.content);
          if (!doc || !doc->is_dict()) {
            return false;
          }
          const base::Value::List* app_list =
              doc->GetDict().FindListByDottedPath("request.app");
          if (!app_list) {
            return false;
          }
          for (const base::Value& app : *app_list) {
            if (const auto* dict = app.GetIfDict()) {
              if (const auto* appid = dict->FindString("appid");
                  *appid == app_id) {
                if (const auto* install_source =
                        dict->FindString("installsource")) {
                  return (*install_source == "ondemand") ==
                         (priority == UpdateService::Priority::kForeground);
                }
              }
            }
          }
          return priority != UpdateService::Priority::kForeground;
        }();
        if (!is_match) {
          ADD_FAILURE() << R"(Request does not match "appid", "priority: )"
                        << request.content;
        }
        return is_match;
      });
}

}  // namespace updater::test::request
