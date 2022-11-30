// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_H_
#define CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace chrome_cleaner {

class HttpResponse;

// Defines an interface for issuing HTTP requests.
class HttpAgent {
 public:
  virtual ~HttpAgent() {}

  // Issues an HTTP POST request.
  // @param host The target host.
  // @param port The target port.
  // @param path The resource path.
  // @param secure Whether to use HTTPS.
  // @param extra_headers Zero or more CRLF-delimited HTTP header lines to
  //     include in the request.
  // @param body The request body.
  // @param traffic_annotation provides the required description for auditing.
  //    Please refer to //docs/network_traffic_annotations.md for more details.
  // @returns NULL if the request fails for any reason. Otherwise, returns an
  //     HttpResponse that may be used to access the HTTP response.
  virtual std::unique_ptr<HttpResponse> Post(
      const std::wstring& host,
      uint16_t port,
      const std::wstring& path,
      bool secure,
      const std::wstring& extra_headers,
      const std::string& body,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;

  // Issues an HTTP GET request.
  // @param host The target host.
  // @param port The target port.
  // @param path The resource path.
  // @param secure Whether to use HTTPS.
  // @param extra_headers Zero or more CRLF-delimited HTTP header lines to
  //     include in the request.
  // @param traffic_annotation provides the required description for auditing.
  //    Please refer to //docs/network_traffic_annotations.md for more details.
  // @returns NULL if the request fails for any reason. Otherwise, returns an
  //     HttpResponse that may be used to access the HTTP response.
  virtual std::unique_ptr<HttpResponse> Get(
      const std::wstring& host,
      uint16_t port,
      const std::wstring& path,
      bool secure,
      const std::wstring& extra_headers,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_H_
