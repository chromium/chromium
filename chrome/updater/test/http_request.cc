// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/http_request.h"

#include <algorithm>
#include <iterator>
#include <string>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/test/embedded_test_server/http_request.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
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

  const size_t dump_limit =
      std::min(request.decoded_content.size(), size_t{2048});
  std::string printable_content;
  printable_content.reserve(dump_limit);
  base::ranges::transform(
      request.decoded_content.begin(),
      request.decoded_content.begin() + dump_limit,
      std::back_inserter(printable_content), [](unsigned char c) {
        return absl::ascii_isprint(c) || absl::ascii_isspace(c) ? c : '.';
      });

  if (request.decoded_content.size() <= dump_limit) {
    return printable_content;
  }

  return base::StringPrintf("%s\n<Total size: %zu, skipped printing %zu bytes>",
                            printable_content.c_str(),
                            request.decoded_content.size(),
                            request.decoded_content.size() - dump_limit);
}

}  // namespace updater::test
