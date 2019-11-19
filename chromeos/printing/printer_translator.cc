// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_translator.h"

#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri_components.h"

using base::DictionaryValue;

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

// Returns true if the uri was retrieved, is valid, and was set on |printer|.
// Returns false otherwise.
bool SetUri(const DictionaryValue& dict, Printer* printer) {
  std::string uri;
  if (!dict.GetString(kUri, &uri)) {
    LOG(WARNING) << "Uri required";
    return false;
  }

  if (!chromeos::ParseUri(uri).has_value()) {
    LOG(WARNING) << "Uri is malformed";
    return false;
  }

  printer->set_uri(uri);
  return true;
}

// Populates the |printer| object with corresponding fields from |value|.
// Returns false if |value| is missing a required field.
bool DictionaryToPrinter(const DictionaryValue& value, Printer* printer) {
  // Mandatory fields
  std::string display_name;
  if (value.GetString(kDisplayName, &display_name)) {
    printer->set_display_name(display_name);
  } else {
    LOG(WARNING) << "Display name required";
    return false;
  }

  if (!SetUri(value, printer)) {
    return false;
  }

  // Optional fields
  std::string description;
  if (value.GetString(kDescription, &description))
    printer->set_description(description);

  std::string manufacturer;
  if (value.GetString(kManufacturer, &manufacturer))
    printer->set_manufacturer(manufacturer);

  std::string model;
  if (value.GetString(kModel, &model))
    printer->set_model(model);

  std::string make_and_model = manufacturer;
  if (!manufacturer.empty() && !model.empty())
    make_and_model.append(" ");
  make_and_model.append(model);
  printer->set_make_and_model(make_and_model);

  std::string uuid;
  if (value.GetString(kUUID, &uuid))
    printer->set_uuid(uuid);

  return true;
}

// Create an empty CupsPrinterInfo dictionary value. It should be consistent
// with the fields in js side. See cups_printers_browser_proxy.js for the
// definition of CupsPrintersInfo.
std::unique_ptr<base::DictionaryValue> CreateEmptyPrinterInfo() {
  std::unique_ptr<base::DictionaryValue> printer_info =
      std::make_unique<base::DictionaryValue>();
  printer_info->SetString("ppdManufacturer", "");
  printer_info->SetString("ppdModel", "");
  printer_info->SetString("printerAddress", "");
  printer_info->SetBoolean("printerPpdReference.autoconf", false);
  printer_info->SetString("printerDescription", "");
  printer_info->SetString("printerId", "");
  printer_info->SetString("printerManufacturer", "");
  printer_info->SetString("printerModel", "");
  printer_info->SetString("printerMakeAndModel", "");
  printer_info->SetString("printerName", "");
  printer_info->SetString("printerPPDPath", "");
  printer_info->SetString("printerProtocol", "ipp");
  printer_info->SetString("printerQueue", "");
  printer_info->SetString("printerStatus", "");
  return printer_info;
}

// Formats a host and port string. The |port| portion is omitted if it is
// unspecified or invalid.
std::string PrinterAddress(const std::string& host, int port) {
  if (port != url::PORT_UNSPECIFIED && port != url::PORT_INVALID) {
    return base::StringPrintf("%s:%d", host.c_str(), port);
  }

  return host;
}

}  // namespace

const char kPrinterId[] = "id";

std::unique_ptr<Printer> RecommendedPrinterToPrinter(
    const base::DictionaryValue& pref) {
  std::string id;
  // Printer id comes from the id or guid field depending on the source.
  if (!pref.GetString(kPrinterId, &id) && !pref.GetString(kGuid, &id)) {
    LOG(WARNING) << "Record id required";
    return nullptr;
  }

  auto printer = std::make_unique<Printer>(id);
  if (!DictionaryToPrinter(pref, printer.get())) {
    LOG(WARNING) << "Failed to parse policy printer.";
    return nullptr;
  }

  printer->set_source(Printer::SRC_POLICY);

  const DictionaryValue* ppd;
  if (pref.GetDictionary(kPpdResource, &ppd)) {
    Printer::PpdReference* ppd_reference = printer->mutable_ppd_reference();
    std::string make_and_model;
    if (ppd->GetString(kEffectiveModel, &make_and_model))
      ppd_reference->effective_make_and_model = make_and_model;
    bool autoconf;
    if (ppd->GetBoolean(kAutoconf, &autoconf))
      ppd_reference->autoconf = autoconf;
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

std::unique_ptr<base::DictionaryValue> GetCupsPrinterInfo(
    const Printer& printer) {
  std::unique_ptr<base::DictionaryValue> printer_info =
      CreateEmptyPrinterInfo();

  printer_info->SetString("printerId", printer.id());
  printer_info->SetString("printerName", printer.display_name());
  printer_info->SetString("printerDescription", printer.description());
  printer_info->SetString("printerManufacturer", printer.manufacturer());
  printer_info->SetString("printerModel", printer.model());
  printer_info->SetString("printerMakeAndModel", printer.make_and_model());
  // NOTE: This assumes the the function IsIppEverywhere() simply returns
  // |printer.ppd_reference_.autoconf|. If the implementation of
  // IsIppEverywhere() changes this will need to be changed as well.
  printer_info->SetBoolean("printerPpdReference.autoconf",
                           printer.IsIppEverywhere());
  printer_info->SetString("printerPPDPath",
                          printer.ppd_reference().user_supplied_ppd_url);

  auto optional = printer.GetUriComponents();
  if (!optional.has_value()) {
    // Uri is invalid so we set default values.
    LOG(WARNING) << "Could not parse uri.  Defaulting values";
    printer_info->SetString("printerAddress", "");
    printer_info->SetString("printerQueue", "");
    printer_info->SetString("printerProtocol",
                            "ipp");  // IPP is our default protocol.
    return printer_info;
  }

  UriComponents uri = optional.value();

  if (base::ToLowerASCII(uri.scheme()) == "usb") {
    // USB has URI path (and, maybe, query) components that aren't really
    // associated with a queue -- the mapping between printing semantics and URI
    // semantics breaks down a bit here.  From the user's point of view, the
    // entire host/path/query block is the printer address for USB.
    printer_info->SetString("printerAddress",
                            printer.uri().substr(strlen("usb://")));
    printer_info->SetString("ppdManufacturer", printer.manufacturer());
  } else {
    printer_info->SetString("printerAddress",
                            PrinterAddress(uri.host(), uri.port()));
    if (!uri.path().empty()) {
      printer_info->SetString("printerQueue", uri.path().substr(1));
    }
  }
  printer_info->SetString("printerProtocol", base::ToLowerASCII(uri.scheme()));

  return printer_info;
}

}  // namespace chromeos
