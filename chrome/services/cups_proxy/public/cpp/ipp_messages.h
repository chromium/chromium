// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_IPP_MESSAGES_H_
#define CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_IPP_MESSAGES_H_

#include <cups/cups.h>
#include <stddef.h>

#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/services/ipp_parser/public/cpp/ipp_converter.h"
#include "printing/backend/cups_ipp_util.h"

// POD representations of HTTP/IPP objects.
namespace cups_proxy {

// Helpful wrapper for a HTTP Request request-line.
struct HttpRequestLine {
  std::string method;
  std::string endpoint;
  std::string http_version;
};

// POD representation of an IPP request and assorted metadata.
struct IppRequest {
  // Explicitly declared/defined defaults since [chromium-style] flagged this as
  // a complex struct.
  IppRequest();
  IppRequest(IppRequest&& other);
  ~IppRequest();

  // Implicitly deleted by DISALLOW, so adding back in.
  IppRequest& operator=(IppRequest&& other) = default;

  std::vector<uint8_t> buffer;

  HttpRequestLine request_line;
  std::vector<ipp_converter::HttpHeader> headers;
  printing::ScopedIppPtr ipp;
  std::vector<uint8_t> ipp_data;

  DISALLOW_COPY_AND_ASSIGN(IppRequest);
};

// Helpful wrapper for a HTTP Response status-line.
struct HttpStatusLine {
  std::string http_version;
  std::string status_code;
  std::string reason_phrase;
};

// POD representation of an IPP response and assorted metadata.
struct IppResponse {
  // Explicitly declared/defined defaults since [chromium-style] flagged this as
  // a complex struct.
  IppResponse();
  IppResponse(IppResponse&& other);
  ~IppResponse();

  // Implicitly deleted by DISALLOW, so adding back in.
  IppResponse& operator=(IppResponse&& other) = default;

  std::vector<uint8_t> buffer;

  HttpStatusLine status_line;
  std::vector<ipp_converter::HttpHeader> headers;
  printing::ScopedIppPtr ipp;
  std::vector<uint8_t> ipp_data;

  DISALLOW_COPY_AND_ASSIGN(IppResponse);
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_IPP_MESSAGES_H_
