// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/request_matcher.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/test/http_request.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "net/base/url_util.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace updater::test::request {

namespace {

// Returns `request.apps` (protocol V4), else `request.app` (protocol V3).
// Older updaters that the integration tests install still send protocol V3.
const base::ListValue* FindAppList(const base::DictValue& doc) {
  if (const base::ListValue* app_list =
          doc.FindListByDottedPath("request.apps")) {
    return app_list;
  }
  return doc.FindListByDottedPath("request.app");
}

// Returns the parts of the multipart `body`, each with its headers and data
// but without the surrounding delimiters, or an error message if `body` is not
// well-formed.
base::expected<std::vector<std::string_view>, std::string> SplitMultipartBody(
    std::string_view body,
    std::string_view boundary) {
  const std::string dash_boundary = base::StrCat({"--", boundary});
  const std::string delimiter = base::StrCat({"\r\n", dash_boundary});

  // The first delimiter may start the body without a preceding CRLF.
  std::string_view input = body;
  if (!input.starts_with(dash_boundary)) {
    const size_t pos = input.find(delimiter);
    if (pos == std::string_view::npos) {
      return base::unexpected(base::StrCat(
          {"No multipart delimiter [", dash_boundary, "] found."}));
    }
    input.remove_prefix(pos + std::string_view("\r\n").size());
  }
  input.remove_prefix(dash_boundary.size());

  std::vector<std::string_view> parts;
  while (!input.starts_with("--")) {
    if (!input.starts_with("\r\n")) {
      return base::unexpected(
          base::StrCat({"Multipart delimiter [", dash_boundary,
                        "] is not followed by CRLF."}));
    }
    input.remove_prefix(std::string_view("\r\n").size());
    const size_t part_end = input.find(delimiter);
    if (part_end == std::string_view::npos) {
      return base::unexpected(
          base::StrCat({"Multipart data should end with the close delimiter [",
                        dash_boundary, "--]."}));
    }
    parts.push_back(input.substr(0, part_end));
    input.remove_prefix(part_end + delimiter.size());
  }
  return parts;
}

GURL StripQueryAndRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

re2::RE2::Options CaseInsensitiveOptions() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  return options;
}

void CheckValidRegex(const std::string& regex,
                     const re2::RE2::Options& options) {
  const re2::RE2 re(regex, options);
  CHECK(re.ok()) << "Invalid regex [" << regex << "]: " << re.error();
}

}  // namespace

FormExpectations::FormExpectations(const std::string& name,
                                   std::vector<std::string> regexes)
    : name(name), regex_sequence(std::move(regexes)) {}

FormExpectations::FormExpectations(const FormExpectations&) = default;
FormExpectations& FormExpectations::operator=(const FormExpectations& other) =
    default;
FormExpectations::~FormExpectations() = default;

Matcher GetPathMatcher(const std::string& expected_path_regex) {
  CheckValidRegex(expected_path_regex, re2::RE2::Options());
  return base::BindLambdaForTesting(
      [expected_path_regex](
          const HttpRequest& request) -> testing::AssertionResult {
        if (!re2::RE2::FullMatch(request.relative_url, expected_path_regex)) {
          return testing::AssertionFailure()
                 << "Request path [" << request.relative_url
                 << "], did not match expected path regex ["
                 << expected_path_regex
                 << "], request content: " << GetPrintableContent(request);
        }
        return testing::AssertionSuccess();
      });
}

Matcher GetHeaderMatcher(
    const base::flat_map<std::string, std::string>& expected_headers) {
  for (const auto& [header_name, expected_header_regex] : expected_headers) {
    CheckValidRegex(expected_header_regex, CaseInsensitiveOptions());
  }
  return base::BindLambdaForTesting(
      [expected_headers](
          const HttpRequest& request) -> testing::AssertionResult {
        const re2::RE2::Options options = CaseInsensitiveOptions();
        for (const auto& [header_name, expected_header_regex] :
             expected_headers) {
          const HttpRequest::HeaderMap::const_iterator it =
              request.headers.find(header_name);
          if (it == request.headers.end()) {
            return testing::AssertionFailure()
                   << "Request header '" << header_name
                   << "' not found, expected regex " << expected_header_regex;
          }
          if (!re2::RE2::FullMatch(it->second,
                                   re2::RE2(expected_header_regex, options))) {
            return testing::AssertionFailure()
                   << "Request header [" << it->first << " = '" << it->second
                   << "'], did not match expected regex ["
                   << expected_header_regex << "]";
          }
        }
        return testing::AssertionSuccess();
      });
}

Matcher GetUpdaterUserAgentMatcher(const base::Version& updater_version) {
  CHECK_GE(updater_version.components().size(), 3u);
  // Escapes regex metacharacters in the updater `User-Agent`, or matches
  // `<product>_test/<third component>` with a non-digit boundary (`(\D.*)?`).
  return GetHeaderMatcher(
      {{"User-Agent",
        base::StrCat({re2::RE2::QuoteMeta(GetUpdaterUserAgent(updater_version)),
                      "|", re2::RE2::QuoteMeta(PRODUCT_FULLNAME_STRING),
                      "_test/",
                      base::NumberToString(updater_version.components()[2]),
                      R"((\D.*)?)"})}});
}

Matcher GetTargetURLMatcher(GURL target_url) {
  CHECK(target_url.is_valid())
      << "Invalid target URL [" << target_url.possibly_invalid_spec() << "]";
  CHECK(!target_url.has_query() && !target_url.has_ref())
      << "Target URL [" << target_url.spec()
      << "] must not have a query or fragment.";
  Matcher host_matcher = GetHeaderMatcher(
      {{"Host", re2::RE2::QuoteMeta(net::GetHostAndOptionalPort(target_url))}});
  return base::BindLambdaForTesting(
      [target_url,
       host_matcher](const HttpRequest& request) -> testing::AssertionResult {
        // The request line is `POST <absolute request target> HTTP/<version>`.
        const std::string_view all_headers(request.all_headers);
        const std::vector<std::string_view> request_line =
            base::SplitStringPiece(
                all_headers.substr(0, all_headers.find("\r\n")), " ",
                base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
        if (request_line.size() != 3 ||
            !base::EqualsCaseInsensitiveASCII(request_line[0], "POST")) {
          return testing::AssertionFailure()
                 << "Request all_headers [" << request.all_headers
                 << "] does not start with a `POST` request line.";
        }
        // GURL canonicalizes scheme, host, and default port.
        const GURL request_target(request_line[1]);
        if (!request_target.is_valid() ||
            StripQueryAndRef(request_target) != target_url) {
          return testing::AssertionFailure()
                 << "Request target [" << request_line[1]
                 << "] does not match the expected [" << target_url.spec()
                 << "], ignoring the query.";
        }
        return host_matcher.Run(request);
      });
}

Matcher GetContentMatcher(
    const std::vector<std::string>& expected_content_regex_sequence) {
  for (const std::string& regex : expected_content_regex_sequence) {
    CheckValidRegex(regex, CaseInsensitiveOptions());
  }
  return base::BindLambdaForTesting(
      [expected_content_regex_sequence](
          const HttpRequest& request) -> testing::AssertionResult {
        const re2::RE2::Options options = CaseInsensitiveOptions();
        std::string_view input(request.decoded_content);
        for (const std::string& regex : expected_content_regex_sequence) {
          if (!re2::RE2::FindAndConsume(&input, re2::RE2(regex, options))) {
            return testing::AssertionFailure()
                   << "Request content match failed. Expected regex: [" << regex
                   << "] not found in content: ["
                   << GetPrintableContent(request) << "]";
          }
        }
        return testing::AssertionSuccess();
      });
}

Matcher GetScopeMatcher(UpdaterScope scope) {
  return base::BindLambdaForTesting(
      [scope](const HttpRequest& request) -> testing::AssertionResult {
        const std::optional<base::DictValue> doc = base::JSONReader::ReadDict(
            request.decoded_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
        const std::optional<bool> ismachine =
            doc ? doc->FindBoolByDottedPath("request.ismachine") : std::nullopt;
        if (!ismachine || *ismachine != IsSystemInstall(scope)) {
          return testing::AssertionFailure()
                 << "Request does not match scope [" << scope
                 << "]: " << GetPrintableContent(request);
        }
        return testing::AssertionSuccess();
      });
}

Matcher GetAppPriorityMatcher(const std::string& app_id,
                              UpdateService::Priority priority) {
  return base::BindLambdaForTesting([app_id,
                                     priority](const HttpRequest& request)
                                        -> testing::AssertionResult {
    const bool is_match = [&app_id, priority, &request] {
      const std::optional<base::DictValue> doc = base::JSONReader::ReadDict(
          request.decoded_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
      if (!doc) {
        return false;
      }
      const base::ListValue* app_list = FindAppList(*doc);
      if (!app_list) {
        return false;
      }
      for (const base::Value& app : *app_list) {
        if (const auto* dict = app.GetIfDict()) {
          if (const auto* appid = dict->FindString("appid");
              appid && base::EqualsCaseInsensitiveASCII(*appid, app_id)) {
            if (const auto* install_source =
                    dict->FindString("installsource")) {
              static constexpr auto kInstallSources =
                  std::array{"ondemand", "taggedmi", "policy"};
              return std::ranges::contains(kInstallSources, *install_source) ==
                     (priority == UpdateService::Priority::kForeground);
            }
          }
        }
      }
      return priority != UpdateService::Priority::kForeground;
    }();
    if (!is_match) {
      return testing::AssertionFailure()
             << "Request does not match app_id [" << app_id << "], priority ["
             << priority << "]: " << GetPrintableContent(request);
    }
    return testing::AssertionSuccess();
  });
}

Matcher GetUpdaterEnableUpdatesMatcher() {
  return base::BindLambdaForTesting(
      [](const HttpRequest& request) -> testing::AssertionResult {
        const std::optional<base::DictValue> doc = base::JSONReader::ReadDict(
            request.decoded_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
        if (!doc) {
          return testing::AssertionFailure()
                 << "Could not parse decoded content as a JSON dict: "
                 << GetPrintableContent(request);
        }
        const base::ListValue* app_list = FindAppList(*doc);
        if (!app_list) {
          return testing::AssertionFailure()
                 << "No `request.apps` or `request.app` list in request: "
                 << GetPrintableContent(request);
        }
        for (const base::Value& app : *app_list) {
          const base::DictValue* dict = app.GetIfDict();
          if (!dict) {
            continue;
          }
          const std::string* appid = dict->FindString("appid");
          if (!appid ||
              !base::EqualsCaseInsensitiveASCII(*appid, kUpdaterAppId)) {
            continue;
          }
          const base::DictValue* update_check = dict->FindDict("updatecheck");
          if (update_check &&
              update_check->FindBool("updatedisabled").value_or(false)) {
            return testing::AssertionFailure()
                   << "Update is wrongfully disabled for updater itself: "
                   << GetPrintableContent(request);
          }
        }
        return testing::AssertionSuccess();
      });
}

Matcher GetMultipartContentMatcher(
    const std::vector<FormExpectations>& form_expectations) {
  for (const FormExpectations& form_expectation : form_expectations) {
    for (const std::string& regex : form_expectation.regex_sequence) {
      CheckValidRegex(regex, CaseInsensitiveOptions());
    }
  }
  return base::BindLambdaForTesting(
      [form_expectations](
          const HttpRequest& request) -> testing::AssertionResult {
        const HttpRequest::HeaderMap::const_iterator content_type =
            request.headers.find("Content-Type");
        if (content_type == request.headers.end()) {
          return testing::AssertionFailure()
                 << "Content-Type header not found, which is expected "
                 << "for multipart content.";
        }

        std::string mime_type;
        std::string charset;
        bool had_charset = false;
        std::string boundary;
        net::HttpUtil::ParseContentType(content_type->second, &mime_type,
                                        &charset, &had_charset, &boundary);
        if (mime_type != "multipart/form-data") {
          return testing::AssertionFailure()
                 << "Content-Type value [" << content_type->second
                 << "] is not the expected [multipart/form-data].";
        }
        if (boundary.empty()) {
          return testing::AssertionFailure()
                 << "Content-Type value has an empty multipart boundary.";
        }

        const base::expected<std::vector<std::string_view>, std::string> parts =
            SplitMultipartBody(request.decoded_content, boundary);
        if (!parts.has_value()) {
          return testing::AssertionFailure() << parts.error();
        }

        const re2::RE2::Options options = CaseInsensitiveOptions();
        auto part = parts->begin();
        for (const FormExpectations& form_expectation : form_expectations) {
          const std::string& form_name = form_expectation.name;
          const std::string disposition = base::StrCat(
              {R"(Content-Disposition: form-data; name=")", form_name, R"(")"});

          // Skips to the next part whose headers name this form. `form_data`
          // includes the rest of the part's headers, such as `filename`.
          std::optional<std::string_view> form_data;
          for (; part != parts->end() && !form_data; ++part) {
            const std::string_view headers =
                part->substr(0, part->find("\r\n\r\n"));
            if (const size_t pos = headers.find(disposition);
                pos != std::string_view::npos) {
              form_data = part->substr(pos + disposition.size());
            }
          }
          if (!form_data) {
            return testing::AssertionFailure()
                   << "Form [" << form_name << "] not found.";
          }

          std::string_view form_input = *form_data;
          for (const std::string& regex : form_expectation.regex_sequence) {
            if (!re2::RE2::FindAndConsume(&form_input,
                                          re2::RE2(regex, options))) {
              return testing::AssertionFailure()
                     << "Form [" << form_name << "] match failed. "
                     << "Expected regex: [" << regex << "] not found in "
                     << "form data: [" << GetPrintableContent(*form_data)
                     << "]";
            }
          }
        }

        return testing::AssertionSuccess();
      });
}

Matcher GetJSONContentMatcher(const base::DictValue& expected_target) {
  return base::BindLambdaForTesting(
      [target = base::Value(expected_target.Clone())](
          const HttpRequest& request) -> testing::AssertionResult {
        std::optional<base::DictValue> doc = base::JSONReader::ReadDict(
            request.decoded_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
        if (!doc) {
          return testing::AssertionFailure()
                 << "Could not parse decoded content as a JSON dict: "
                 << GetPrintableContent(request);
        }

        base::Value doc_value(std::move(*doc));
        if (!IsJSONSubset(target, doc_value,
                          [](const std::string& a, const std::string& b) {
                            return base::EqualsCaseInsensitiveASCII(a, b);
                          })) {
          return testing::AssertionFailure()
                 << "Expected JSON subset: [" << target.DebugString()
                 << "] not found in (parsed) request content: ["
                 << doc_value.DebugString() << "]";
        }

        return testing::AssertionSuccess();
      });
}

}  // namespace updater::test::request
