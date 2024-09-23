// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_CUPS_UTIL_H_
#define CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_CUPS_UTIL_H_

#include <cups/cups.h>
#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "chrome/services/cups_proxy/public/cpp/ipp_messages.h"

namespace chromeos {
class Printer;
}  // namespace chromeos

// Utility namespace that encapsulates helpful libCUPS-dependent
// constants/methods.
namespace cups_proxy {

// Max HTTP buffer size, as defined libCUPS at cups/http.h.
// Note: This is assumed to be stable.
static const size_t kHttpMaxBufferSize = 2048;

// Max number of printers PluginVm can handle in a CUPS-Get-Printers IPP
// response.
static const int kPluginVmPrinterLimit = 40;

// PDF and PostScript document format identifiers.
constexpr std::array<uint8_t, 5> pdf_magic_bytes = {0x25, 0x50, 0x44, 0x46,
                                                    0x2d};  // { %PDF- }
constexpr std::array<uint8_t, 4> ps_magic_bytes = {0x25, 0x21, 0x50,
                                                   0x53};  // { %!PS }

// Expects |request| to be an IPP_OP_GET_PRINTERS IPP request. This function
// creates an appropriate IPP response referencing |printers|.
// TODO(crbug.com/945409): Expand testing suite.
std::optional<IppResponse> BuildGetDestsResponse(
    const IppRequest& request,
    const std::vector<chromeos::Printer>& printers);

// If |ipp| refers to a printer, we return the associated printer_id.
// Note: Expects the printer id to be embedded in the resource field of the
// 'printer-uri' IPP attribute.
std::optional<std::string> GetPrinterId(ipp_t* ipp);

// Expects |endpoint| to be of the form '/printers/{printer_id}'.
// Returns an empty Optional if parsing fails or yields an empty printer_id.
std::optional<std::string> ParseEndpointForPrinterId(std::string_view endpoint);

// Return list of printers containing first recently used printers,
// then |saved| printers and |enterprise| printers up until the hard printer
// limit of kPluginVmPrinterLimit.
std::vector<chromeos::Printer> FilterPrintersForPluginVm(
    const std::vector<chromeos::Printer>& saved,
    const std::vector<chromeos::Printer>& enterprise,
    const std::vector<std::string>& recent);

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_CUPS_UTIL_H_
