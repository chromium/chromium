// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_IPP_PARSER_PUBLIC_CPP_IPP_CONVERTER_H_
#define CHROME_SERVICES_IPP_PARSER_PUBLIC_CPP_IPP_CONVERTER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom.h"
#include "printing/backend/cups_ipp_util.h"

// This namespace is filled with helpful (conversion) functions for working with
// arbitrary byte buffers representing IPP requests.
//
// IPP(internet printing protocol) is build on top of HTTP, so most of the these
// deal with working with arbitrary HTTP requests. The Parse methods take an
// arbitrary buffer and attempt to parse out the asked-for HTTP field/s. The
// Build methods do the exact opposite, taking separate HTTP fields and building
// an arbitrary buffer containing them, reading for shipping.

namespace ipp_converter {

using HttpHeader = std::pair<std::string, std::string>;

// Carriage return; Http header-pair delimiter.
const char kCarriage[] = "\r\n";

// Defined IPP end-of-message sentinel.
const char kIppSentinel[] = "\x03";

// Request line converters
// Parses |status_line| into vector of 3, individual terms, returns empty
// Optional on failure.
base::Optional<std::vector<std::string>> ParseRequestLine(
    base::StringPiece status_line);

// Builds valid HTTP Request line from input span of 3 |terms|, returns empty
// Optional on failure.
base::Optional<std::vector<uint8_t>> BuildRequestLine(
    base::StringPiece method,
    base::StringPiece endpoint,
    base::StringPiece http_version);

// Headers converters
// Parsed |headers_slice| into vector of HTTP header name/value pairs.
// Returns empty Optional on failure.
base::Optional<std::vector<HttpHeader>> ParseHeaders(
    base::StringPiece headers_slice);

// Builds valid HTTP headers from input vector of header name/value pairs.
// Returns empty Optional on failure.
base::Optional<std::vector<uint8_t>> BuildHeaders(
    std::vector<HttpHeader> terms);

// IPP message converters
// Reads |ipp_slice| into wrapped ipp_t*, using libCUPS APIs.
// Returns nullptr on failure.
printing::ScopedIppPtr ParseIppMessage(base::span<const uint8_t> ipp_slice);

// Builds valid IPP message from |ipp|, using libCUPS APIs.
// Returns empty Optional on failure.
// Note: Does not take ownership of |ipp|.
base::Optional<std::vector<uint8_t>> BuildIppMessage(ipp_t* ipp);

// Often used helper wrapping the above commands for building a complete IPP
// request. Overloaded for cases without ipp_data.
// Returns empty Optional on any failure.
base::Optional<std::vector<uint8_t>> BuildIppRequest(
    base::StringPiece method,
    base::StringPiece endpoint,
    base::StringPiece http_version,
    std::vector<HttpHeader> terms,
    ipp_t* ipp,
    std::vector<uint8_t> ipp_data);
base::Optional<std::vector<uint8_t>> BuildIppRequest(
    base::StringPiece method,
    base::StringPiece endpoint,
    base::StringPiece http_version,
    std::vector<HttpHeader> terms,
    ipp_t* ipp);

// Mojom converter for ipp_t objects, return nullptr on failure.
// Note: This function does not take ownership of |ipp|.
ipp_parser::mojom::IppMessagePtr ConvertIppToMojo(ipp_t* ipp);

}  // namespace ipp_converter

#endif  // CHROME_SERVICES_IPP_PARSER_PUBLIC_CPP_IPP_CONVERTER_H_
