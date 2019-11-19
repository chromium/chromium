// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_configuration.h"

#include "base/guid.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "chromeos/printing/printing_constants.h"
#include "chromeos/printing/uri_components.h"
#include "net/base/ip_endpoint.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

namespace chromeos {

// Returns true if the scheme is both valid and non-empty.
bool IsSchemeValid(const url::Parsed& parsed) {
  return parsed.scheme.is_valid() && parsed.scheme.is_nonempty();
}

// Returns true if |parsed| contains a valid port. A valid port is one that
// either contains a valid value or is completely missing.
bool IsPortValid(const url::Parsed& parsed) {
  // A length of -1 indicates that the port is missing.
  return parsed.port.len == -1 ||
         (parsed.port.is_valid() && parsed.port.is_nonempty());
}

// Returns |printer_uri| broken into components if it represents a valid uri. A
// valid uri contains a scheme, host, and a valid or missing port number.
// Optionally, the uri contains a path.
base::Optional<UriComponents> ParseUri(const std::string& printer_uri) {
  const char* uri_ptr = printer_uri.c_str();
  url::Parsed parsed;
  url::ParseStandardURL(uri_ptr, printer_uri.length(), &parsed);
  if (!IsSchemeValid(parsed) || !parsed.host.is_valid() ||
      !IsPortValid(parsed)) {
    LOG(WARNING) << "Could not parse printer uri";
    return {};
  }
  base::StringPiece scheme(&uri_ptr[parsed.scheme.begin], parsed.scheme.len);
  base::StringPiece host(&uri_ptr[parsed.host.begin], parsed.host.len);
  base::StringPiece path =
      parsed.path.is_valid()
          ? base::StringPiece(&uri_ptr[parsed.path.begin], parsed.path.len)
          : "";

  int port = ParsePort(uri_ptr, parsed.port);
  if (port == url::SpecialPort::PORT_INVALID) {
    LOG(WARNING) << "Port is invalid";
    return {};
  }
  // Port not specified.
  if (port == url::SpecialPort::PORT_UNSPECIFIED) {
    if (scheme == kIppScheme) {
      port = kIppPort;
    } else if (scheme == kIppsScheme) {
      port = kIppsPort;
    }
  }

  bool encrypted = scheme != kIppScheme;
  return UriComponents(encrypted, scheme.as_string(), host.as_string(), port,
                       path.as_string());
}

namespace {

// Returns the index of the first character representing the hostname in |uri|.
// Returns npos if the start of the hostname is not found.
//
// We should use GURL to do this except that uri could start with ipp:// which
// is not a standard url scheme (according to GURL).
size_t HostnameStart(base::StringPiece uri) {
  size_t scheme_separator_start = uri.find(url::kStandardSchemeSeparator);
  if (scheme_separator_start == base::StringPiece::npos) {
    return base::StringPiece::npos;
  }
  return scheme_separator_start + strlen(url::kStandardSchemeSeparator);
}

base::StringPiece HostAndPort(base::StringPiece uri) {
  size_t hostname_start = HostnameStart(uri);
  if (hostname_start == base::StringPiece::npos) {
    return "";
  }

  size_t hostname_end = uri.find("/", hostname_start);
  if (hostname_end == base::StringPiece::npos) {
    // No trailing slash.  Use end of string.
    hostname_end = uri.size();
  }

  CHECK_GT(hostname_end, hostname_start);
  return uri.substr(hostname_start, hostname_end - hostname_start);
}

}  // namespace

Printer::Printer() : id_(base::GenerateGUID()), source_(SRC_USER_PREFS) {}

Printer::Printer(const std::string& id) : id_(id), source_(SRC_USER_PREFS) {
  if (id_.empty())
    id_ = base::GenerateGUID();
}

Printer::Printer(const Printer& other) = default;

Printer& Printer::operator=(const Printer& other) = default;

Printer::~Printer() = default;

bool Printer::IsIppEverywhere() const {
  return ppd_reference_.autoconf;
}

net::HostPortPair Printer::GetHostAndPort() const {
  if (uri_.empty()) {
    return net::HostPortPair();
  }

  return net::HostPortPair::FromString(HostAndPort(uri_).as_string());
}

std::string Printer::ReplaceHostAndPort(const net::IPEndPoint& ip) const {
  if (uri_.empty()) {
    return "";
  }

  size_t hostname_start = HostnameStart(uri_);
  if (hostname_start == base::StringPiece::npos) {
    return "";
  }
  size_t host_port_len = HostAndPort(uri_).length();
  return base::JoinString({uri_.substr(0, hostname_start), ip.ToString(),
                           uri_.substr(hostname_start + host_port_len)},
                          "");
}

Printer::PrinterProtocol Printer::GetProtocol() const {
  const base::StringPiece uri(uri_);

  if (uri.starts_with("usb:"))
    return PrinterProtocol::kUsb;

  if (uri.starts_with("ipp:"))
    return PrinterProtocol::kIpp;

  if (uri.starts_with("ipps:"))
    return PrinterProtocol::kIpps;

  if (uri.starts_with("http:"))
    return PrinterProtocol::kHttp;

  if (uri.starts_with("https:"))
    return PrinterProtocol::kHttps;

  if (uri.starts_with("socket:"))
    return PrinterProtocol::kSocket;

  if (uri.starts_with("lpd:"))
    return PrinterProtocol::kLpd;

  if (uri.starts_with("ippusb:"))
    return PrinterProtocol::kIppUsb;

  return PrinterProtocol::kUnknown;
}

bool Printer::HasNetworkProtocol() const {
  Printer::PrinterProtocol current_protocol = GetProtocol();
  switch (current_protocol) {
    case PrinterProtocol::kIpp:
    case PrinterProtocol::kIpps:
    case PrinterProtocol::kHttp:
    case PrinterProtocol::kHttps:
    case PrinterProtocol::kSocket:
    case PrinterProtocol::kLpd:
      return true;
    default:
      return false;
  }
}

bool Printer::IsUsbProtocol() const {
  Printer::PrinterProtocol current_protocol = GetProtocol();
  switch (current_protocol) {
    case PrinterProtocol::kUsb:
    case PrinterProtocol::kIppUsb:
      return true;
    default:
      return false;
  }
}

base::Optional<UriComponents> Printer::GetUriComponents() const {
  return chromeos::ParseUri(uri_);
}

}  // namespace chromeos
