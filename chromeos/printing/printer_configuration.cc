// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_configuration.h"

#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "chromeos/printing/printing_constants.h"
#include "chromeos/printing/uri.h"
#include "net/base/ip_endpoint.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

namespace chromeos {

namespace {

std::string ErrMsg(std::string_view prefix, std::string_view message) {
  return base::StrCat({prefix, message});
}

std::string_view ToString(Uri::ParserStatus status) {
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

IppPrinterInfo::IppPrinterInfo() = default;

IppPrinterInfo::IppPrinterInfo(const IppPrinterInfo& other) = default;

IppPrinterInfo::IppPrinterInfo(const std::vector<std::string>& document_formats,
                               const std::string& document_format_default,
                               const std::string& document_format_preferred,
                               const std::vector<std::string>& urf_supported,
                               const std::vector<std::string>& pdf_versions,
                               const std::vector<std::string>& ipp_features,
                               const std::string& mopria_certified,
                               const std::vector<std::string>& printer_kind) {
  this->document_formats = document_formats;
  this->document_format_default = document_format_default;
  this->document_format_preferred = document_format_preferred;
  this->urf_supported = urf_supported;
  this->pdf_versions = pdf_versions;
  this->ipp_features = ipp_features;
  this->mopria_certified = mopria_certified;
  this->printer_kind = printer_kind;
}

IppPrinterInfo::~IppPrinterInfo() = default;

std::string_view ToString(PrinterClass pclass) {
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
  NOTREACHED();
}

bool IsValidPrinterUri(const Uri& uri, std::string* error_message) {
  static constexpr auto kKnownSchemes =
      base::MakeFixedFlatSet<std::string_view>(
          {"http", "https", "ipp", "ipps", "ippusb", "lpd", "socket", "usb"});
  static constexpr std::string_view kPrefix = "Malformed printer URI: ";

  if (!kKnownSchemes.contains(uri.GetScheme())) {
    if (error_message)
      *error_message = ErrMsg(kPrefix, "unknown or missing scheme");
    return false;
  }

  // Only printer URIs with the lpd scheme are allowed to have Userinfo.
  if (!uri.GetUserinfo().empty() && uri.GetScheme() != "lpd") {
    if (error_message)
      *error_message =
          ErrMsg(kPrefix, "user info is not allowed for this scheme");
    return false;
  }

  if (uri.GetHost().empty()) {
    if (error_message)
      *error_message = ErrMsg(kPrefix, "missing host");
    return false;
  }

  if (uri.GetScheme() == "ippusb" || uri.GetScheme() == "usb") {
    if (uri.GetPort() > -1) {
      if (error_message)
        *error_message = ErrMsg(kPrefix, "port is not allowed for this scheme");
      return false;
    }
    if (uri.GetPath().empty()) {
      if (error_message)
        *error_message = ErrMsg(kPrefix, "path is required for this scheme");
      return false;
    }
  }

  if (uri.GetScheme() == "socket" && !uri.GetPath().empty()) {
    if (error_message)
      *error_message = ErrMsg(kPrefix, "path is not allowed for this scheme");
    return false;
  }

  if (!uri.GetFragment().empty()) {
    if (error_message)
      *error_message = ErrMsg(kPrefix, "fragment is not allowed");
    return false;
  }

  return true;
}

bool Printer::PpdReference::IsFilled() const {
  return autoconf || !user_supplied_ppd_url.empty() ||
         !effective_make_and_model.empty();
}

Printer::ManagedPrintOptions::ManagedPrintOptions() = default;
Printer::ManagedPrintOptions::ManagedPrintOptions(
    const Printer::ManagedPrintOptions& other) = default;
Printer::ManagedPrintOptions& Printer::ManagedPrintOptions::operator=(
    const Printer::ManagedPrintOptions& other) = default;
Printer::ManagedPrintOptions::~ManagedPrintOptions() = default;

Printer::Printer()
    : id_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      source_(SRC_USER_PREFS) {}

Printer::Printer(std::string id) : id_(std::move(id)), source_(SRC_USER_PREFS) {
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

bool Printer::SetUri(std::string_view uri, std::string* error_message) {
  Uri parsed_uri(uri);
  const Uri::ParserError& parser_status = parsed_uri.GetLastParsingError();
  if (parser_status.status == Uri::ParserStatus::kNoErrors)
    return SetUri(parsed_uri, error_message);
  if (error_message) {
    *error_message = ErrMsg("Malformed URI: ", ToString(parser_status.status));
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
          "epson et-5180 series",          // b/319373509
          "epson et-8550 series",          // b/301387697
          "epson wf-110 series",           // b/287159028
          "hp deskjet 2600 series",        // b/399480007
          "hp deskjet 4100 series",        // b/279387801
          "hp officejet 8010 series",      // b/401543989
          "hp officejet 8020 series",      // b/401543989
          "hp officejet 8700",             // b/401543989
          "hp officejet pro 9010 series",  // b/401543989
          "hp officejet pro 9020 series",  // b/401543989
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
  static constexpr auto kProtocolMap =
      base::MakeFixedFlatMap<std::string_view, Printer::PrinterProtocol>({
          {"usb", PrinterProtocol::kUsb},
          {"ipp", PrinterProtocol::kIpp},
          {"ipps", PrinterProtocol::kIpps},
          {"http", PrinterProtocol::kHttp},
          {"https", PrinterProtocol::kHttps},
          {"socket", PrinterProtocol::kSocket},
          {"lpd", PrinterProtocol::kLpd},
          {"ippusb", PrinterProtocol::kIppUsb},
      });
  auto iter = kProtocolMap.find(uri_.GetScheme());
  if (iter == kProtocolMap.cend()) {
    return PrinterProtocol::kUnknown;
  }
  return iter->second;
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
