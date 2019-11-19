// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/cups_util.h"

#include <queue>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/cups_jobs.h"

namespace cups_proxy {
namespace {

// This comparator defines a priority_queue of printers in descending order by
// display name.
class DisplayNameComparator {
 public:
  bool operator()(const chromeos::Printer& a, const chromeos::Printer& b) {
    return a.display_name() < b.display_name();
  }
};

// Return the top |k| printers from |printers| sorted alphabetically by display
// name.
std::vector<chromeos::Printer> GetFirstKPrinters(
    const std::vector<chromeos::Printer>& printers,
    size_t k) {
  auto pq =
      std::priority_queue<chromeos::Printer, std::vector<chromeos::Printer>,
                          DisplayNameComparator>();

  // Filter through |printers|, only keeping the first |k| printers in the pq.
  for (const chromeos::Printer& printer : printers) {
    pq.push(printer);
    if (pq.size() > k) {
      pq.pop();
    }
  }

  // We want the returned list in ascending order, so we assign to ret in
  // reverse order.
  std::vector<chromeos::Printer> ret;
  ret.resize(pq.size());
  for (int i = pq.size() - 1; i >= 0; --i) {
    ret[i] = pq.top();
    pq.pop();
  }

  return ret;
}

}  // namespace

base::Optional<IppResponse> BuildGetDestsResponse(
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
    return base::nullopt;
  }

  ret.buffer = std::move(*response_buffer);
  return ret;
}

base::Optional<std::string> GetPrinterId(ipp_t* ipp) {
  // We expect the printer id to be embedded in the printer-uri.
  ipp_attribute_t* printer_uri_attr =
      ippFindAttribute(ipp, "printer-uri", IPP_TAG_URI);
  if (!printer_uri_attr) {
    return base::nullopt;
  }

  // Only care about the resource, throw everything else away
  char resource[HTTP_MAX_URI], unwanted_buffer[HTTP_MAX_URI];
  int unwanted_port;

  std::string printer_uri = ippGetString(printer_uri_attr, 0, NULL);
  httpSeparateURI(HTTP_URI_CODING_RESOURCE, printer_uri.data(), unwanted_buffer,
                  HTTP_MAX_URI, unwanted_buffer, HTTP_MAX_URI, unwanted_buffer,
                  HTTP_MAX_URI, &unwanted_port, resource, HTTP_MAX_URI);

  // The printer id should be the last component of the resource.
  base::StringPiece uuid(resource);
  auto uuid_start = uuid.find_last_of('/');
  if (uuid_start == base::StringPiece::npos || uuid_start + 1 >= uuid.size()) {
    return base::nullopt;
  }

  return uuid.substr(uuid_start + 1).as_string();
}

base::Optional<std::string> ParseEndpointForPrinterId(
    base::StringPiece endpoint) {
  size_t last_path = endpoint.find_last_of('/');
  if (last_path == base::StringPiece::npos ||
      last_path + 1 >= endpoint.size()) {
    return base::nullopt;
  }

  return endpoint.substr(last_path + 1).as_string();
}

std::vector<chromeos::Printer> FilterPrintersForPluginVm(
    const std::vector<chromeos::Printer>& saved,
    const std::vector<chromeos::Printer>& enterprise) {
  if (saved.size() >= kPluginVmPrinterLimit) {
    return std::vector<chromeos::Printer>(
        saved.begin(), saved.begin() + kPluginVmPrinterLimit);
  }

  // Filter down enterprise printers to backfill.
  size_t num_enterprise_printers = kPluginVmPrinterLimit - saved.size();
  auto filtered_enterprise =
      GetFirstKPrinters(enterprise, num_enterprise_printers);

  // Concatenate saved printers and filtered_enterprise to return.
  std::vector<chromeos::Printer> ret = saved;
  ret.insert(ret.end(), filtered_enterprise.begin(), filtered_enterprise.end());
  return ret;
}

}  // namespace cups_proxy
