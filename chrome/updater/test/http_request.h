// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_HTTP_REQUEST_H_
#define CHROME_UPDATER_TEST_HTTP_REQUEST_H_

#include <string>

#include "net/test/embedded_test_server/http_request.h"

namespace updater::test {

struct HttpRequest : public net::test_server::HttpRequest {
  explicit HttpRequest(const net::test_server::HttpRequest& raw_request);
  HttpRequest();
  HttpRequest(const HttpRequest&);
  HttpRequest& operator=(const HttpRequest&);
  ~HttpRequest();

  std::string decoded_content;
};

// Returns the request content string that replaces unprintable characters with
// a '.', up-to the limit of 2k characters.
std::string GetPrintableContent(const HttpRequest& request);

}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_HTTP_REQUEST_H_
