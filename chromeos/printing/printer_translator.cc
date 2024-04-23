// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_translator.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chromeos {

namespace {

// For historical reasons, the effective_make_and_model field is just
// effective_model for policy printers.
const char kEffectiveModel[] = "effective_model";

// printer fields
const char kDisplayName[] = "display_name";
const char kDescription[] = "description";
const char kManufacturer[] = "manufacturer";
const char kModel[] = "model";
const char kUri[] = "uri";
const char kUUID[] = "uuid";
const char kPpdResource[] = "ppd_resource";
const char kAutoconf[] = "autoconf";
const char kGuid[] = "guid";

// Populates the |printer| object with corresponding fields from |value|.
// Returns false if |value| is missing a required field.
bool DictionaryToPrinter(const base::Value::Dict& value, Printer* printer) {
  // Mandatory fields
  const std::string* display_name = value.FindString(kDisplayName);
  if (!display_name) {
    LOG(WARNING) << "Display name required";
    return false;
  }
  printer->set_display_name(*display_name);

  const std::string* uri = value.FindString(kUri);
  if (!uri) {
    LOG(WARNING) << "Uri required";
    return false;
  }

  std::string message;
  if (!printer->SetUri(*uri, &message)) {
    LOG(WARNING) << message;
    return false;
  }

  // Optional fields
  const std::string* description = value.FindString(kDescription);
  if (description)
    printer->set_description(*description);

  const std::string* manufacturer = value.FindString(kManufacturer);
  const std::string* model = value.FindString(kModel);

  std::string make_and_model = manufacturer ? *manufacturer : std::string();
  if (!make_and_model.empty() && model && !model->empty())
    make_and_model.append(" ");
  if (model)
    make_and_model.append(*model);
  printer->set_make_and_model(make_and_model);

  const std::string* uuid = value.FindString(kUUID);
  if (uuid)
    printer->set_uuid(*uuid);

  return true;
}

// Create an empty CupsPrinterInfo dictionary value. It should be consistent
// with the fields in js side. See cups_printers_browser_proxy.js for the
// definition of CupsPrintersInfo.
base::Value::Dict CreateEmptyPrinterInfo() {
  base::Value::Dict printer_info;
  printer_info.Set("isManaged", false);
  printer_info.Set("ppdManufacturer", "");
  printer_info.Set("ppdModel", "");
  printer_info.Set("printerAddress", "");
  printer_info.SetByDottedPath("printerPpdReference.autoconf", false);
  printer_info.Set("printerDescription", "");
  printer_info.Set("printerId", "");
  printer_info.Set("printerMakeAndModel", "");
  printer_info.Set("printerName", "");
  printer_info.Set("printerPPDPath", "");
  printer_info.Set("printerProtocol", "ipp");
  printer_info.Set("printerQueue", "");
  return printer_info;
}

// Formats a host and port string. The |port| portion is omitted if it is
// unspecified or invalid.
std::string PrinterAddress(const Uri& uri) {
  const int port = uri.GetPort();
  if (port > -1) {
    return base::StringPrintf("%s:%d", uri.GetHostEncoded().c_str(), port);
  }
  return uri.GetHostEncoded();
}

}  // namespace

const char kPrinterId[] = "id";

std::unique_ptr<Printer> RecommendedPrinterToPrinter(
    const base::Value::Dict& pref) {
  std::string id;
  // Printer id comes from the id or guid field depending on the source.
  const std::string* printer_id = pref.FindString(kPrinterId);
  const std::string* printer_guid = pref.FindString(kGuid);
  if (printer_id) {
    id = *printer_id;
  } else if (printer_guid) {
    id = *printer_guid;
  } else {
    LOG(WARNING) << "Record id required";
    return nullptr;
  }

  auto printer = std::make_unique<Printer>(id);
  if (!DictionaryToPrinter(pref, printer.get())) {
    LOG(WARNING) << "Failed to parse policy printer.";
    return nullptr;
  }

  printer->set_source(Printer::SRC_POLICY);

  const base::Value::Dict* ppd = pref.FindDict(kPpdResource);
  if (ppd) {
    Printer::PpdReference* ppd_reference = printer->mutable_ppd_reference();
    const std::string* make_and_model = ppd->FindString(kEffectiveModel);
    if (make_and_model)
      ppd_reference->effective_make_and_model = *make_and_model;
    std::optional<bool> autoconf = ppd->FindBool(kAutoconf);
    if (autoconf.has_value())
      ppd_reference->autoconf = *autoconf;
  }
  if (!printer->ppd_reference().autoconf &&
      printer->ppd_reference().effective_make_and_model.empty()) {
    // Either autoconf flag or make and model is mandatory.
    LOG(WARNING)
        << "Missing autoconf flag and model information for policy printer.";
    return nullptr;
  }
  if (printer->ppd_reference().autoconf &&
      !printer->ppd_reference().effective_make_and_model.empty()) {
    // PPD reference can't contain both autoconf and make and model.
    LOG(WARNING) << "Autoconf flag is set together with model information for "
                    "policy printer.";
    return nullptr;
  }

  return printer;
}

base::Value::Dict GetCupsPrinterInfo(const Printer& printer) {
  base::Value::Dict printer_info = CreateEmptyPrinterInfo();

  printer_info.Set("isManaged",
                   printer.source() == Printer::Source::SRC_POLICY);
  printer_info.Set("printerId", printer.id());
  printer_info.Set("printerName", printer.display_name());
  printer_info.Set("printerDescription", printer.description());
  printer_info.Set("printerMakeAndModel", printer.make_and_model());
  // NOTE: This assumes the the function IsIppEverywhere() simply returns
  // |printer.ppd_reference_.autoconf|. If the implementation of
  // IsIppEverywhere() changes this will need to be changed as well.
  printer_info.SetByDottedPath("printerPpdReference.autoconf",
                               printer.IsIppEverywhere());
  printer_info.Set("printerPPDPath",
                   printer.ppd_reference().user_supplied_ppd_url);
  printer_info.Set("printServerUri", printer.print_server_uri());
  printer_info.Set("printerStatus", printer.printer_status().ConvertToValue());

  if (!printer.HasUri()) {
    // Uri is invalid so we set default values.
    LOG(WARNING) << "Could not parse uri.  Defaulting values";
    printer_info.Set("printerAddress", "");
    printer_info.Set("printerQueue", "");
    printer_info.Set("printerProtocol", "ipp");  // IPP is our default protocol.
    return printer_info;
  }

  if (printer.IsUsbProtocol())
    printer_info.Set("ppdManufacturer", printer.usb_printer_manufacturer());
  printer_info.Set("printerProtocol", printer.uri().GetScheme());
  printer_info.Set("printerAddress", PrinterAddress(printer.uri()));
  std::string printer_queue = printer.uri().GetPathEncodedAsString();
  if (!printer_queue.empty())
    printer_queue = printer_queue.substr(1);  // removes the leading '/'
  if (!printer.uri().GetQueryEncodedAsString().empty())
    printer_queue += "?" + printer.uri().GetQueryEncodedAsString();
  printer_info.Set("printerQueue", printer_queue);

  return printer_info;
}

}  // namespace chromeos
