// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_metadata_parser.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace chromeos {

namespace {

// Attempts to
// 1. parse |input| as a Value having Type::DICT and
// 2. return Value of |key| having a given |target_type| from the same.
//
// Additionally,
// *  this function never returns empty Value objects and
// *  |target_type| must appear in the switch statement below.
std::optional<base::Value> ParseJsonAndUnnestKey(
    std::string_view input,
    std::string_view key,
    base::Value::Type target_type) {
  std::optional<base::Value> parsed = base::JSONReader::Read(input);
  if (!parsed || !parsed->is_dict()) {
    return std::nullopt;
  }

  std::optional<base::Value> unnested = parsed->GetDict().Extract(key);
  if (!unnested || unnested->type() != target_type) {
    return std::nullopt;
  }

  bool unnested_is_empty = true;
  switch (target_type) {
    case base::Value::Type::LIST:
      unnested_is_empty = unnested->GetList().empty();
      break;
    case base::Value::Type::DICT:
      unnested_is_empty = unnested->GetDict().empty();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (unnested_is_empty) {
    return std::nullopt;
  }
  return unnested;
}

// Returns a Restrictions struct from a dictionary `dict`.
Restrictions ParseRestrictionsFromDict(const base::Value::Dict& dict) {
  Restrictions restrictions;
  auto min_as_double = dict.FindDouble("minMilestone");
  auto max_as_double = dict.FindDouble("maxMilestone");

  if (min_as_double.has_value()) {
    base::Version min_milestone = base::Version(
        base::NumberToString(static_cast<int>(min_as_double.value())));
    if (min_milestone.IsValid()) {
      restrictions.min_milestone = min_milestone;
    }
  }
  if (max_as_double.has_value()) {
    base::Version max_milestone = base::Version(
        base::NumberToString(static_cast<int>(max_as_double.value())));
    if (max_milestone.IsValid()) {
      restrictions.max_milestone = max_milestone;
    }
  }
  return restrictions;
}

// Returns a ParsedPrinter from a leaf `dict` from Printers metadata.
std::optional<ParsedPrinter> ParsePrinterFromDict(
    const base::Value::Dict& dict) {
  const std::string* const effective_make_and_model = dict.FindString("emm");
  const std::string* const name = dict.FindString("name");
  if (!effective_make_and_model || effective_make_and_model->empty() || !name ||
      name->empty()) {
    return std::nullopt;
  }
  ParsedPrinter printer;
  printer.effective_make_and_model = *effective_make_and_model;
  printer.user_visible_printer_name = *name;

  const base::Value::Dict* const restrictions_dict =
      dict.FindDict("restriction");
  if (restrictions_dict) {
    printer.restrictions = ParseRestrictionsFromDict(*restrictions_dict);
  }
  return printer;
}

// Returns a ParsedIndexLeaf from |value|.
std::optional<ParsedIndexLeaf> ParsedIndexLeafFrom(const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }

  const base::Value::Dict& dict = value.GetDict();
  ParsedIndexLeaf leaf;

  const std::string* const ppd_basename = dict.FindString("name");
  if (!ppd_basename) {
    return std::nullopt;
  }
  leaf.ppd_basename = *ppd_basename;

  const base::Value::Dict* const restrictions_dict =
      dict.FindDict("restriction");
  if (restrictions_dict) {
    leaf.restrictions = ParseRestrictionsFromDict(*restrictions_dict);
  }

  const std::string* const ppd_license = dict.FindString("license");
  if (ppd_license && !ppd_license->empty()) {
    leaf.license = *ppd_license;
  }

  return leaf;
}

// Returns a ParsedIndexValues from a |value| extracted from a forward
// index.
std::optional<ParsedIndexValues> UnnestPpdMetadata(const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }
  const base::Value::List* const ppd_metadata_list =
      value.GetDict().FindList("ppdMetadata");
  if (!ppd_metadata_list || ppd_metadata_list->empty()) {
    return std::nullopt;
  }

  ParsedIndexValues parsed_index_values;
  for (const base::Value& v : *ppd_metadata_list) {
    std::optional<ParsedIndexLeaf> parsed_index_leaf = ParsedIndexLeafFrom(v);
    if (parsed_index_leaf.has_value()) {
      parsed_index_values.values.push_back(parsed_index_leaf.value());
    }
  }

  if (parsed_index_values.values.empty()) {
    return std::nullopt;
  }
  return parsed_index_values;
}

}  // namespace

Restrictions::Restrictions() = default;
Restrictions::~Restrictions() = default;
Restrictions::Restrictions(const Restrictions&) = default;
Restrictions& Restrictions::operator=(const Restrictions&) = default;

ParsedPrinter::ParsedPrinter() = default;
ParsedPrinter::~ParsedPrinter() = default;
ParsedPrinter::ParsedPrinter(const ParsedPrinter&) = default;
ParsedPrinter& ParsedPrinter::operator=(const ParsedPrinter&) = default;

ParsedIndexLeaf::ParsedIndexLeaf() = default;
ParsedIndexLeaf::~ParsedIndexLeaf() = default;
ParsedIndexLeaf::ParsedIndexLeaf(const ParsedIndexLeaf&) = default;
ParsedIndexLeaf& ParsedIndexLeaf::operator=(const ParsedIndexLeaf&) = default;

ParsedIndexValues::ParsedIndexValues() = default;
ParsedIndexValues::~ParsedIndexValues() = default;
ParsedIndexValues::ParsedIndexValues(const ParsedIndexValues&) = default;
ParsedIndexValues& ParsedIndexValues::operator=(const ParsedIndexValues&) =
    default;

std::optional<std::vector<std::string>> ParseLocales(
    std::string_view locales_json) {
  const auto as_value =
      ParseJsonAndUnnestKey(locales_json, "locales", base::Value::Type::LIST);
  if (!as_value.has_value()) {
    return std::nullopt;
  }

  std::vector<std::string> locales;
  for (const auto& iter : as_value->GetList()) {
    if (!iter.is_string())
      continue;
    locales.push_back(iter.GetString());
  }

  if (locales.empty()) {
    return std::nullopt;
  }
  return locales;
}

std::optional<ParsedManufacturers> ParseManufacturers(
    std::string_view manufacturers_json) {
  const auto as_value = ParseJsonAndUnnestKey(manufacturers_json, "filesMap",
                                              base::Value::Type::DICT);
  if (!as_value.has_value()) {
    return std::nullopt;
  }
  ParsedManufacturers manufacturers;
  for (const auto [key, value] : as_value->GetDict()) {
    if (!value.is_string()) {
      continue;
    }
    manufacturers[key] = value.GetString();
  }
  return manufacturers.empty() ? std::nullopt
                               : std::make_optional(manufacturers);
}

std::optional<ParsedIndex> ParseForwardIndex(
    std::string_view forward_index_json) {
  // Firstly, we unnest the dictionary keyed by "ppdIndex."
  std::optional<base::Value> ppd_index = ParseJsonAndUnnestKey(
      forward_index_json, "ppdIndex", base::Value::Type::DICT);
  if (!ppd_index.has_value()) {
    return std::nullopt;
  }

  ParsedIndex parsed_index;

  // Secondly, we iterate on the key-value pairs of the ppdIndex.
  // This yields a list of leaf values (dictionaries).
  for (const auto [key, value] : ppd_index->GetDict()) {
    std::optional<ParsedIndexValues> values = UnnestPpdMetadata(value);
    if (values.has_value()) {
      parsed_index.insert_or_assign(key, values.value());
    }
  }

  if (parsed_index.empty()) {
    return std::nullopt;
  }
  return parsed_index;
}

std::optional<ParsedUsbIndex> ParseUsbIndex(std::string_view usb_index_json) {
  std::optional<base::Value> usb_index = ParseJsonAndUnnestKey(
      usb_index_json, "usbIndex", base::Value::Type::DICT);
  if (!usb_index.has_value()) {
    return std::nullopt;
  }

  ParsedUsbIndex parsed_usb_index;
  for (const auto [key, value] : usb_index->GetDict()) {
    int product_id;
    if (!base::StringToInt(key, &product_id)) {
      continue;
    }
    if (!value.is_dict()) {
      continue;
    }

    const std::string* effective_make_and_model =
        value.GetDict().FindString("effectiveMakeAndModel");
    if (!effective_make_and_model || effective_make_and_model->empty()) {
      continue;
    }

    parsed_usb_index.insert_or_assign(product_id, *effective_make_and_model);
  }
  if (parsed_usb_index.empty()) {
    return std::nullopt;
  }
  return parsed_usb_index;
}

std::optional<ParsedUsbVendorIdMap> ParseUsbVendorIdMap(
    std::string_view usb_vendor_id_map_json) {
  std::optional<base::Value> as_value = ParseJsonAndUnnestKey(
      usb_vendor_id_map_json, "entries", base::Value::Type::LIST);
  if (!as_value.has_value()) {
    return std::nullopt;
  }

  ParsedUsbVendorIdMap usb_vendor_ids;
  for (const auto& usb_vendor_description : as_value->GetList()) {
    if (!usb_vendor_description.is_dict()) {
      continue;
    }

    std::optional<int> vendor_id =
        usb_vendor_description.GetDict().FindInt("vendorId");
    const std::string* const vendor_name =
        usb_vendor_description.GetDict().FindString("vendorName");
    if (!vendor_id.has_value() || !vendor_name || vendor_name->empty()) {
      continue;
    }
    usb_vendor_ids.insert_or_assign(vendor_id.value(), *vendor_name);
  }

  if (usb_vendor_ids.empty()) {
    return std::nullopt;
  }
  return usb_vendor_ids;
}

std::optional<ParsedPrinters> ParsePrinters(std::string_view printers_json) {
  const auto as_value =
      ParseJsonAndUnnestKey(printers_json, "printers", base::Value::Type::LIST);
  if (!as_value.has_value()) {
    return std::nullopt;
  }

  ParsedPrinters printers;
  for (const auto& printer_value : as_value->GetList()) {
    const base::Value::Dict* printer_dict = printer_value.GetIfDict();
    if (!printer_dict) {
      continue;
    }
    std::optional<ParsedPrinter> printer = ParsePrinterFromDict(*printer_dict);
    if (!printer.has_value()) {
      continue;
    }
    printers.push_back(printer.value());
  }
  if (printers.empty()) {
    return std::nullopt;
  }
  return printers;
}

std::optional<ParsedReverseIndex> ParseReverseIndex(
    std::string_view reverse_index_json) {
  const std::optional<base::Value> makes_and_models = ParseJsonAndUnnestKey(
      reverse_index_json, "reverseIndex", base::Value::Type::DICT);
  if (!makes_and_models.has_value()) {
    return std::nullopt;
  }

  ParsedReverseIndex parsed;
  for (const auto [key, value] : makes_and_models->GetDict()) {
    const base::Value::Dict* value_dict = value.GetIfDict();
    if (!value_dict) {
      continue;
    }

    const std::string* manufacturer = value_dict->FindString("manufacturer");
    const std::string* model = value_dict->FindString("model");
    if (manufacturer && model && !manufacturer->empty() && !model->empty()) {
      parsed.insert_or_assign(key, ReverseIndexLeaf{*manufacturer, *model});
    }
  }

  if (parsed.empty()) {
    return std::nullopt;
  }
  return parsed;
}

}  // namespace chromeos
