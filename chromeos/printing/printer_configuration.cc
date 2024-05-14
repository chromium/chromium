// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_configuration.h"

#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "chromeos/printing/printing_constants.h"
#include "chromeos/printing/uri.h"
#include "net/base/ip_endpoint.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

namespace chromeos {

namespace {
std::string ToString(Uri::ParserStatus status) {
  switch (status) {
    case Uri::ParserStatus::kInvalidPercentEncoding:
      return "invalid percent encoding";
    case Uri::ParserStatus::kDisallowedASCIICharacter:
      return "disallowed ASCII character";
    case Uri::ParserStatus::kInvalidUTF8Character:
      return "invalid UTF-8 character";
    case Uri::ParserStatus::kInvalidScheme:
      return "invalid scheme";
    case Uri::ParserStatus::kInvalidPortNumber:
      return "invalid port number";
    case Uri::ParserStatus::kRelativePathsNotAllowed:
      return "relative paths not allowed";
    case Uri::ParserStatus::kEmptySegmentInPath:
      return "empty segment in path";
    case Uri::ParserStatus::kEmptyParameterNameInQuery:
      return "empty parameter name in query";
    case Uri::ParserStatus::kNoErrors:
      return "no errors";
  }
  return "unknown error";
}
}  // namespace

std::string ToString(PrinterClass pclass) {
  switch (pclass) {
    case PrinterClass::kEnterprise:
      return "Enterprise";
    case PrinterClass::kAutomatic:
      return "Automatic";
    case PrinterClass::kDiscovered:
      return "Discovered";
    case PrinterClass::kSaved:
      return "Saved";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

bool IsValidPrinterUri(const Uri& uri, std::string* error_message) {
  static constexpr auto kKnownSchemes =
      base::MakeFixedFlatSet<std::string_view>(
          {"http", "https", "ipp", "ipps", "ippusb", "lpd", "socket", "usb"});
  static const std::string kPrefix = "Malformed printer URI: ";

  if (!kKnownSchemes.contains(uri.GetScheme())) {
    if (error_message)
      *error_message = kPrefix + "unknown or missing scheme";
    return false;
  }

  // Only printer URIs with the lpd scheme are allowed to have Userinfo.
  if (!uri.GetUserinfo().empty() && uri.GetScheme() != "lpd") {
    if (error_message)
      *error_message = kPrefix + "user info is not allowed for this scheme";
    return false;
  }

  if (uri.GetHost().empty()) {
    if (error_message)
      *error_message = kPrefix + "missing host";
    return false;
  }

  if (uri.GetScheme() == "ippusb" || uri.GetScheme() == "usb") {
    if (uri.GetPort() > -1) {
      if (error_message)
        *error_message = kPrefix + "port is not allowed for this scheme";
      return false;
    }
    if (uri.GetPath().empty()) {
      if (error_message)
        *error_message = kPrefix + "path is required for this scheme";
      return false;
    }
  }

  if (uri.GetScheme() == "socket" && !uri.GetPath().empty()) {
    if (error_message)
      *error_message = kPrefix + "path is not allowed for this scheme";
    return false;
  }

  if (!uri.GetFragment().empty()) {
    if (error_message)
      *error_message = kPrefix + "fragment is not allowed";
    return false;
  }

  return true;
}

bool Printer::PpdReference::IsFilled() const {
  return autoconf || !user_supplied_ppd_url.empty() ||
         !effective_make_and_model.empty();
}

Printer::Printer()
    : id_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      source_(SRC_USER_PREFS) {}

Printer::Printer(const std::string& id) : id_(id), source_(SRC_USER_PREFS) {
  if (id_.empty())
    id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
}

Printer::Printer(const Printer& other) = default;

Printer& Printer::operator=(const Printer& other) = default;

Printer::~Printer() = default;

bool Printer::SetUri(const Uri& uri, std::string* error_message) {
  if (!IsValidPrinterUri(uri, error_message))
    return false;
  uri_ = uri;
  return true;
}

bool Printer::SetUri(const std::string& uri, std::string* error_message) {
  Uri parsed_uri(uri);
  const Uri::ParserError& parser_status = parsed_uri.GetLastParsingError();
  if (parser_status.status == Uri::ParserStatus::kNoErrors)
    return SetUri(parsed_uri, error_message);
  if (error_message) {
    *error_message = "Malformed URI: " + ToString(parser_status.status);
  }
  return false;
}

bool Printer::IsIppEverywhere() const {
  return ppd_reference_.autoconf;
}

bool Printer::RequiresDriverlessUsb() const {
  // TODO(b/184293121): Replace this list with more generic logic after general
  // IPP-USB evaluation is complete.
  static constexpr auto kDriverlessUsbMakeModels =
      base::MakeFixedFlatSet<std::string_view>({
          "epson et-5180 series",    // b/319373509
          "epson et-8550 series",    // b/301387697
          "epson wf-110 series",     // b/287159028
          "hp deskjet 4100 series",  // b/279387801
      });
  return kDriverlessUsbMakeModels.contains(base::ToLowerASCII(make_and_model_));
}

net::HostPortPair Printer::GetHostAndPort() const {
  if (!HasUri()) {
    return net::HostPortPair();
  }

  return net::HostPortPair(uri_.GetHost(), uri_.GetPort());
}

Uri Printer::ReplaceHostAndPort(const net::IPEndPoint& ip) const {
  if (!HasUri()) {
    return Uri();
  }

  const std::string host = ip.ToStringWithoutPort();
  if (host.empty()) {
    return Uri();
  }
  Uri uri = uri_;
  uri.SetHost(host);
  uri.SetPort(ip.port());

  return uri;
}

Printer::PrinterProtocol Printer::GetProtocol() const {
  if (uri_.GetScheme() == "usb")
    return PrinterProtocol::kUsb;

  if (uri_.GetScheme() == "ipp")
    return PrinterProtocol::kIpp;

  if (uri_.GetScheme() == "ipps")
    return PrinterProtocol::kIpps;

  if (uri_.GetScheme() == "http")
    return PrinterProtocol::kHttp;

  if (uri_.GetScheme() == "https")
    return PrinterProtocol::kHttps;

  if (uri_.GetScheme() == "socket")
    return PrinterProtocol::kSocket;

  if (uri_.GetScheme() == "lpd")
    return PrinterProtocol::kLpd;

  if (uri_.GetScheme() == "ippusb")
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

bool Printer::HasSecureProtocol() const {
  Printer::PrinterProtocol current_protocol = GetProtocol();
  switch (current_protocol) {
    case PrinterProtocol::kUsb:
    case PrinterProtocol::kIpps:
    case PrinterProtocol::kHttps:
    case PrinterProtocol::kIppUsb:
      return true;
    default:
      return false;
  }
}

bool Printer::IsZeroconf() const {
  return base::EndsWith(uri_.GetHost(), ".local");
}

}  // namespace chromeos
