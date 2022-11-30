// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_ENDPOINT_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_ENDPOINT_H_

#include "url/gurl.h"

class NetAddress;

// Describes a DevTools debugger endpoint such as a
// hostname:port pair, or a full URL with auth credentials.
class DevToolsEndpoint {
 public:
  DevToolsEndpoint() = default;  // Creates an invalid endpoint
  explicit DevToolsEndpoint(int port);
  explicit DevToolsEndpoint(const NetAddress& address);
  explicit DevToolsEndpoint(const std::string& url);
  ~DevToolsEndpoint() = default;

  bool IsValid() const;
  NetAddress Address() const;
  std::string GetBrowserDebuggerUrl() const;
  std::string GetDebuggerUrl(const std::string& id) const;
  std::string GetVersionUrl() const;
  std::string GetListUrl() const;

 private:
  GURL server_url_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_ENDPOINT_H_
