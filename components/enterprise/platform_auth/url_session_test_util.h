// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_TEST_UTIL_H_
#define COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_TEST_UTIL_H_

#include <Foundation/Foundation.h>

#include <cstddef>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "url/gurl.h"

namespace url_session_test_util {

// Configuration defining how the mock NSURLSession should behave when a
// task is started.
// Out of |body|, |os_error| and |hang| only a single field should have a
// non-default value. If all the fields keep the default values the response
// will be 200 OK without a body.
class COMPONENT_EXPORT(ENTERPRISE_PLATFORM_AUTH) ResponseConfig {
 public:
  ResponseConfig();
  ~ResponseConfig();
  ResponseConfig(const ResponseConfig&) = delete;
  ResponseConfig(ResponseConfig&&);
  ResponseConfig& operator=(ResponseConfig&&);

  // If set, the mock session will return a 200 OK HTTP response with this
  // string as the body.
  std::optional<std::string> body;

  // If true, simulates a low-level OS networking error (e.g., offline)
  // instead of returning an HTTP response.
  bool os_error = false;

  // If true, the request will never complete, simulating a timeout or
  // server hang.
  bool hang = false;

  // If set the URLSession server will return a redirect response to this url.
  std::optional<std::string> redirect_url;

  // Headers that will be attached to the response.
  std::vector<std::pair<std::string, std::string>> headers;

  // Callback invoked immediately when the network task starts.
  base::OnceClosure on_started;

  // Callback invoked when the network task completes (or fails).
  base::OnceClosure on_stopped;
};

// Configures the provided `NSURLSessionConfiguration` to use a mock
// `NSURLProtocol` based on the given `ResponseConfig`.
//
// This modifies the `session_config` in-place by registering a dynamic
// protocol class. Any `NSURLSession` created with this configuration will
// intercept requests and return the configured response.
//
// Example usage:
//   NSURLRequest* ns_request = ...;
//   ResponseConfig config;
//   config.body = "success_result";
//
//   // Create a configuration and attach the mock protocol
//   NSURLSessionConfiguration* session_config =
//       [NSURLSessionConfiguration ephemeralSessionConfiguration];
//   url_session_test_util::AttachProtocolToSessionForTesting(
//       std::move(config), session_config);
//
//   // Create the session using the modified configuration
//   NSURLSession* session =
//       [NSURLSession sessionWithConfiguration:session_config];
//
//   // This task will now return 200 OK with "success_result" without
//   // reaching the actual network.
//   NSURLSessionDataTask* task = [session dataTaskWithRequest:ns_request];
//   [task resume];
COMPONENT_EXPORT(ENTERPRISE_PLATFORM_AUTH)
void AttachProtocolToSessionForTesting(ResponseConfig&& config,
                                       NSURLSessionConfiguration* session);

}  // namespace url_session_test_util

#endif  // COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_TEST_UTIL_H_
