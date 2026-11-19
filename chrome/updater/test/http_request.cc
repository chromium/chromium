// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/http_request.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/test/embedded_test_server/http_request.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/zlib/google/compression_utils.h"

namespace updater::test {
namespace {

std::string GetDecodedContent(const net::test_server::HttpRequest& request) {
  net::test_server::HttpRequest::HeaderMap::const_iterator it =
      request.headers.find("Content-Encoding");
  if (it == request.headers.end() ||
      base::CompareCaseInsensitiveASCII(it->second, "gzip") != 0) {
    return request.content;
  }

  std::string content;
  if (!compression::GzipUncompress(request.content, &content)) {
    VLOG(0) << "Cannot inflate gzip content.";
    return request.content;
  }
  return content;
}

// Renames protocol V3 names in `update_request` to their V4 names. See
// `ParseJsonContent()`.
void RenameProtocolV3Names(base::DictValue& update_request) {
  if (!update_request.FindList("apps")) {
    if (base::ListValue* apps = update_request.FindList("app")) {
      for (base::Value& app : *apps) {
        if (base::DictValue* app_dict = app.GetIfDict()) {
          // V3 has no `events`, so `events` comes only from `event`.
          app_dict->Remove("events");
          if (std::optional<base::Value> events = app_dict->Extract("event")) {
            app_dict->Set("events", std::move(*events));
          }
        }
      }
      update_request.Set("apps", std::move(*update_request.Extract("app")));
    }
  }
  if (!update_request.contains("updaters")) {
    if (std::optional<base::Value> updater =
            update_request.Extract("updater")) {
      update_request.Set("updaters", std::move(*updater));
    }
  }
}

}  // namespace

HttpRequest::HttpRequest(const net::test_server::HttpRequest& request)
    : net::test_server::HttpRequest(request),
      decoded_content(GetDecodedContent(request)) {}
HttpRequest::HttpRequest() = default;
HttpRequest::HttpRequest(const HttpRequest&) = default;
HttpRequest& HttpRequest::operator=(const HttpRequest&) = default;
HttpRequest::~HttpRequest() = default;

std::string GetPrintableContent(const HttpRequest& request) {
  if (!request.has_content) {
    return "<no content>";
  }
  return GetPrintableContent(request.decoded_content);
}

std::string GetPrintableContent(std::string_view content) {
  const size_t dump_limit = std::min(content.size(), size_t{2048});
  std::string printable_content;
  printable_content.reserve(dump_limit);
  std::ranges::transform(
      content.substr(0, dump_limit), std::back_inserter(printable_content),
      [](unsigned char c) {
        return absl::ascii_isprint(c) || absl::ascii_isspace(c) ? c : '.';
      });

  if (content.size() <= dump_limit) {
    return printable_content;
  }

  return absl::StrFormat("%s\n<Total size: %zu, skipped printing %zu bytes>",
                         printable_content.c_str(), content.size(),
                         content.size() - dump_limit);
}

std::optional<base::DictValue> ParseJsonContent(const HttpRequest& request) {
  std::optional<base::DictValue> content = base::JSONReader::ReadDict(
      request.decoded_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (content) {
    if (base::DictValue* update_request = content->FindDict("request")) {
      RenameProtocolV3Names(*update_request);
    }
  }
  return content;
}

}  // namespace updater::test
