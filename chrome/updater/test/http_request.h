// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_HTTP_REQUEST_H_
#define CHROME_UPDATER_TEST_HTTP_REQUEST_H_

#include <string>
#include <string_view>

#include "net/test/embedded_test_server/http_request.h"

namespace updater::test {

// Request captured by the test server, with gzip bodies pre-decoded.
struct HttpRequest : public net::test_server::HttpRequest {
  explicit HttpRequest(const net::test_server::HttpRequest& raw_request);
  HttpRequest();
  HttpRequest(const HttpRequest&);
  HttpRequest& operator=(const HttpRequest&);
  ~HttpRequest();

  // `content` gunzipped if `Content-Encoding` is `gzip` (case-insensitively),
  // else `content` (including when gunzipping fails).
  std::string decoded_content;
};

// Returns `request.decoded_content` as `GetPrintableContent(std::string_view)`
// does, or "<no content>" if `!request.has_content`. For diagnostic messages
// only.
std::string GetPrintableContent(const HttpRequest& request);

// Returns up to 2048 bytes of `content`, with bytes outside printable ASCII and
// ASCII whitespace replaced by '.', followed by a size summary line if
// `content` is longer. For diagnostic messages only.
std::string GetPrintableContent(std::string_view content);

}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_HTTP_REQUEST_H_
