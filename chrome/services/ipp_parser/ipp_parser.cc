// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/ipp_parser/ipp_parser.h"

#include <cups/ipp.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/services/ipp_parser/public/cpp/ipp_converter.h"
#include "net/http/http_util.h"

namespace ipp_parser {
namespace {

using ipp_converter::HttpHeader;
using ipp_converter::kCarriage;
using ipp_converter::kIppSentinel;

// Log debugging error and send empty response, signalling error.
void Fail(const std::string& error_log, IppParser::ParseIppCallback cb) {
  DVLOG(1) << "IPP Parser Error: " << error_log;
  std::move(cb).Run(nullptr);
  return;
}

// Returns the starting index of the request-line-delimiter, -1 on failure.
int LocateEndOfRequestLine(std::string_view request) {
  auto end_of_request_line = request.find(kCarriage);
  if (end_of_request_line == std::string::npos) {
    return -1;
  }

  return end_of_request_line;
}

// Returns the starting index of the first HTTP header, -1 on failure.
int LocateStartOfHeaders(std::string_view request) {
  auto idx = LocateEndOfRequestLine(request);
  if (idx < 0) {
    return -1;
  }

  // Advance to first header and check it exists
  idx += strlen(kCarriage);
  return idx < static_cast<int>(request.size()) ? idx : -1;
}

// Returns the starting index of the end-of-headers-delimiter, -1 on failure.
int LocateEndOfHeaders(std::string_view request) {
  auto idx = net::HttpUtil::LocateEndOfHeaders(base::as_byte_span(request));
  if (idx < 0) {
    return -1;
  }

  // Back up to the start of the delimiter.
  // Note: The end-of-http-headers delimiter is 2 back-to-back carriage returns.
  const int end_of_headers_delimiter_size = 2 * strlen(kCarriage);
  return idx - end_of_headers_delimiter_size;
}

// Returns the starting index of the IPP metadata, -1 on failure.
int LocateStartOfIppMetadata(base::span<const uint8_t> request) {
  return net::HttpUtil::LocateEndOfHeaders(request);
}

bool SplitRequestMetadata(base::span<const uint8_t> request,
                          std::string* http_metadata,
                          base::span<const uint8_t>* ipp_metadata) {
  size_t start_of_ipp_metadata = LocateStartOfIppMetadata(request);
  if (start_of_ipp_metadata < 0) {
    return false;
  }

  *http_metadata =
      std::string(base::as_string_view(request.first(start_of_ipp_metadata)));
  *ipp_metadata = request.subspan(start_of_ipp_metadata);
  return true;
}

std::optional<std::vector<std::string>> ExtractHttpRequestLine(
    std::string_view request) {
  size_t end_of_request_line = LocateEndOfRequestLine(request);
  if (end_of_request_line < 0) {
    return std::nullopt;
  }

  const std::string_view request_line_slice =
      request.substr(0, end_of_request_line);
  return ipp_converter::ParseRequestLine(request_line_slice);
}

std::optional<std::vector<HttpHeader>> ExtractHttpHeaders(
    std::string_view request) {
  size_t start_of_headers = LocateStartOfHeaders(request);
  if (start_of_headers < 0) {
    return std::nullopt;
  }

  size_t end_of_headers = LocateEndOfHeaders(request);
  if (end_of_headers < 0) {
    return std::nullopt;
  }

  const std::string_view headers_slice =
      request.substr(start_of_headers, end_of_headers - start_of_headers);
  return ipp_converter::ParseHeaders(headers_slice);
}

// Parses |ipp_metadata| and sets |ipp_message| and |ipp_data| accordingly.
// Returns false and leaves the outputs unchanged on failure.
bool ExtractIppMetadata(base::span<const uint8_t> ipp_metadata,
                        mojom::IppMessagePtr* ipp_message,
                        std::vector<uint8_t>* ipp_data) {
  printing::ScopedIppPtr ipp = ipp_converter::ParseIppMessage(ipp_metadata);
  if (!ipp) {
    return false;
  }

  mojom::IppMessagePtr message = ipp_converter::ConvertIppToMojo(ipp.get());
  if (!message) {
    return false;
  }

  size_t ipp_message_length = ippLength(ipp.get());
  ipp_metadata = ipp_metadata.subspan(ipp_message_length);
  *ipp_data = std::vector<uint8_t>(ipp_metadata.begin(), ipp_metadata.end());

  *ipp_message = std::move(message);
  return true;
}

}  // namespace

IppParser::IppParser(mojo::PendingReceiver<mojom::IppParser> receiver)
    : receiver_(this, std::move(receiver)) {}

IppParser::~IppParser() = default;

void IppParser::ParseIpp(const std::vector<uint8_t>& to_parse,
                         ParseIppCallback callback) {
  // Separate |to_parse| into http metadata (interpreted as ASCII chars), and
  // ipp metadata (interpreted as arbitrary bytes).
  std::string http_metadata;
  base::span<const uint8_t> ipp_metadata;
  if (!SplitRequestMetadata(to_parse, &http_metadata, &ipp_metadata)) {
    return Fail("Failed to split HTTP and IPP metadata", std::move(callback));
  }

  // Parse Request line.
  auto request_line = ExtractHttpRequestLine(http_metadata);
  if (!request_line) {
    return Fail("Failed to parse request line", std::move(callback));
  }

  // Parse Headers.
  auto headers = ExtractHttpHeaders(http_metadata);
  if (!headers) {
    return Fail("Failed to parse headers", std::move(callback));
  }

  // Parse IPP message and IPP data.
  mojom::IppMessagePtr ipp_message;
  std::vector<uint8_t> ipp_data;
  if (!ExtractIppMetadata(ipp_metadata, &ipp_message, &ipp_data)) {
    return Fail("Failed to parse IPP metadata", std::move(callback));
  }

  // Marshall response.
  mojom::IppRequestPtr parsed_request = mojom::IppRequest::New();

  std::vector<std::string> request_line_terms = *request_line;
  parsed_request->method = request_line_terms[0];
  parsed_request->endpoint = request_line_terms[1];
  parsed_request->http_version = request_line_terms[2];

  parsed_request->headers =
      base::flat_map<std::string, std::string>(std::move(*headers));
  parsed_request->ipp = std::move(ipp_message);
  parsed_request->data = std::move(ipp_data);

  DVLOG(1) << "Finished parsing IPP request.";
  std::move(callback).Run(std::move(parsed_request));
}

}  // namespace ipp_parser
