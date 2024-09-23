// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/cups_util.h"

#include <map>
#include <queue>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/cups_jobs.h"

namespace cups_proxy {

std::optional<IppResponse> BuildGetDestsResponse(
    const IppRequest& request,
    const std::vector<chromeos::Printer>& printers) {
  IppResponse ret;

  // Standard OK status line.
  ret.status_line = HttpStatusLine{"HTTP/1.1", "200", "OK"};

  // Generic HTTP headers.
  ret.headers = std::vector<ipp_converter::HttpHeader>{
      {"Content-Language", "en"},
      {"Content-Type", "application/ipp"},
      {"Server", "CUPS/2.1 IPP/2.1"},
      {"X-Frame-Options", "DENY"},
      {"Content-Security-Policy", "frame-ancestors 'none'"}};

  // Fill in IPP attributes.
  ret.ipp = printing::WrapIpp(ippNewResponse(request.ipp.get()));
  for (const auto& printer : printers) {
    // Setting the printer-uri.
    std::string printer_uri = printing::PrinterUriFromName(printer.id());
    ippAddString(ret.ipp.get(), IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "printer-uri-supported", nullptr, printer_uri.c_str());

    // Setting the printer uuid.
    ippAddString(ret.ipp.get(), IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name",
                 nullptr, printer.id().c_str());

    // Setting the display name.
    ippAddString(ret.ipp.get(), IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info",
                 nullptr, printer.display_name().c_str());

    // Optional setting of the make_and_model, if known.
    if (!printer.make_and_model().empty()) {
      ippAddString(ret.ipp.get(), IPP_TAG_PRINTER, IPP_TAG_TEXT,
                   "printer-make-and-model", nullptr,
                   printer.make_and_model().c_str());
    }

    ippAddSeparator(ret.ipp.get());
  }

  // Add the final content length into headers
  const size_t ipp_metadata_sz = ippLength(ret.ipp.get());
  ret.headers.push_back(
      {"Content-Length", base::NumberToString(ipp_metadata_sz)});

  // Build parsed response buffer
  // Note: We are reusing the HTTP request building function since the responses
  // have the exact same format.
  auto response_buffer = ipp_converter::BuildIppRequest(
      ret.status_line.http_version, ret.status_line.status_code,
      ret.status_line.reason_phrase, ret.headers, ret.ipp.get(), ret.ipp_data);
  if (!response_buffer) {
    return std::nullopt;
  }

  ret.buffer = std::move(*response_buffer);
  return ret;
}

std::optional<std::string> GetPrinterId(ipp_t* ipp) {
  // We expect the printer id to be embedded in the printer-uri.
  ipp_attribute_t* printer_uri_attr =
      ippFindAttribute(ipp, "printer-uri", IPP_TAG_URI);
  if (!printer_uri_attr) {
    return std::nullopt;
  }

  // Only care about the resource, throw everything else away
  char resource[HTTP_MAX_URI], unwanted_buffer[HTTP_MAX_URI];
  int unwanted_port;

  std::string printer_uri;
  const char* printer_uri_ptr = ippGetString(printer_uri_attr, 0, nullptr);
  if (printer_uri_ptr) {
    printer_uri = printer_uri_ptr;
  }

  httpSeparateURI(HTTP_URI_CODING_RESOURCE, printer_uri.data(), unwanted_buffer,
                  HTTP_MAX_URI, unwanted_buffer, HTTP_MAX_URI, unwanted_buffer,
                  HTTP_MAX_URI, &unwanted_port, resource, HTTP_MAX_URI);

  // The printer id should be the last component of the resource.
  std::string_view uuid(resource);
  auto uuid_start = uuid.find_last_of('/');
  if (uuid_start == std::string_view::npos || uuid_start + 1 >= uuid.size()) {
    return std::nullopt;
  }

  return std::string(uuid.substr(uuid_start + 1));
}

std::optional<std::string> ParseEndpointForPrinterId(
    std::string_view endpoint) {
  size_t last_path = endpoint.find_last_of('/');
  if (last_path == std::string_view::npos || last_path + 1 >= endpoint.size()) {
    return std::nullopt;
  }

  return std::string(endpoint.substr(last_path + 1));
}

std::vector<chromeos::Printer> FilterPrintersForPluginVm(
    const std::vector<chromeos::Printer>& saved,
    const std::vector<chromeos::Printer>& enterprise,
    const std::vector<std::string>& recent) {
  std::vector<std::string> ids(recent);
  std::map<std::string, const chromeos::Printer*> printers;
  for (const auto* category : {&saved, &enterprise}) {
    for (const auto& printer : *category) {
      ids.push_back(printer.id());
      printers[printer.id()] = &printer;
    }
  }
  std::vector<chromeos::Printer> ret;
  for (const std::string& id : ids) {
    auto it = printers.find(id);
    if (it != printers.end()) {
      ret.push_back(*it->second);
      printers.erase(it);
      if (ret.size() == kPluginVmPrinterLimit) {
        break;
      }
    }
  }
  return ret;
}

}  // namespace cups_proxy
