// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_metadata_parser.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace {

// Attempts to
// 1. parse |input| as a Value having Type::DICTIONARY and
// 2. return Value of |key| having a given |target_type| from the same.
//
// Additionally,
// *  this function never returns empty Value objects and
// *  |target_type| must appear in the switch statement below.
absl::optional<base::Value> ParseJsonAndUnnestKey(
    base::StringPiece input,
    base::StringPiece key,
    base::Value::Type target_type) {
  absl::optional<base::Value> parsed = base::JSONReader::Read(input);
  if (!parsed || !parsed->is_dict()) {
    return absl::nullopt;
  }

  absl::optional<base::Value> unnested = parsed->ExtractKey(key);
  if (!unnested || unnested->type() != target_type) {
    return absl::nullopt;
  }

  bool unnested_is_empty = true;
  switch (target_type) {
    case base::Value::Type::LIST:
      unnested_is_empty = unnested->GetList().empty();
      break;
    case base::Value::Type::DICTIONARY:
      unnested_is_empty = unnested->DictEmpty();
      break;
    default:
      NOTREACHED();
      break;
  }

  if (unnested_is_empty) {
    return absl::nullopt;
  }
  return unnested;
}

// Returns a Restrictions struct from a dictionary |value|.
Restrictions ParseRestrictionsFromValue(const base::Value& value) {
  Restrictions restrictions;
  auto min_as_double = value.FindDoubleKey("minMilestone");
  auto max_as_double = value.FindDoubleKey("maxMilestone");

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

// Returns a ParsedPrinter from a leaf |value| from Printers metadata.
absl::optional<ParsedPrinter> ParsePrinterFromValue(const base::Value& value) {
  const std::string* const effective_make_and_model =
      value.FindStringKey("emm");
  const std::string* const name = value.FindStringKey("name");
  if (!effective_make_and_model || effective_make_and_model->empty() || !name ||
      name->empty()) {
    return absl::nullopt;
  }
  ParsedPrinter printer;
  printer.effective_make_and_model = *effective_make_and_model;
  printer.user_visible_printer_name = *name;

  const base::Value* const restrictions_value =
      value.FindDictKey("restriction");
  if (restrictions_value) {
    printer.restrictions = ParseRestrictionsFromValue(*restrictions_value);
  }
  return printer;
}

// Returns a ParsedIndexLeaf from |value|.
absl::optional<ParsedIndexLeaf> ParsedIndexLeafFrom(const base::Value& value) {
  if (!value.is_dict()) {
    return absl::nullopt;
  }

  ParsedIndexLeaf leaf;

  const std::string* const ppd_basename = value.FindStringKey("name");
  if (!ppd_basename) {
    return absl::nullopt;
  }
  leaf.ppd_basename = *ppd_basename;

  const base::Value* const restrictions_value =
      value.FindDictKey("restriction");
  if (restrictions_value) {
    leaf.restrictions = ParseRestrictionsFromValue(*restrictions_value);
  }

  const std::string* const ppd_license = value.FindStringKey("license");
  if (ppd_license && !ppd_license->empty()) {
    leaf.license = *ppd_license;
  }

  return leaf;
}

// Returns a ParsedIndexValues from a |value| extracted from a forward
// index.
absl::optional<ParsedIndexValues> UnnestPpdMetadata(const base::Value& value) {
  if (!value.is_dict()) {
    return absl::nullopt;
  }
  const base::Value* const ppd_metadata_list = value.FindListKey("ppdMetadata");
  if (!ppd_metadata_list || ppd_metadata_list->GetList().empty()) {
    return absl::nullopt;
  }

  ParsedIndexValues parsed_index_values;
  for (const base::Value& v : ppd_metadata_list->GetList()) {
    absl::optional<ParsedIndexLeaf> parsed_index_leaf = ParsedIndexLeafFrom(v);
    if (parsed_index_leaf.has_value()) {
      parsed_index_values.values.push_back(parsed_index_leaf.value());
    }
  }

  if (parsed_index_values.values.empty()) {
    return absl::nullopt;
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

absl::optional<std::vector<std::string>> ParseLocales(
    base::StringPiece locales_json) {
  const auto as_value =
      ParseJsonAndUnnestKey(locales_json, "locales", base::Value::Type::LIST);
  if (!as_value.has_value()) {
    return absl::nullopt;
  }

  std::vector<std::string> locales;
  for (const auto& iter : as_value.value().GetList()) {
    if (!iter.is_string())
      continue;
    locales.push_back(iter.GetString());
  }

  if (locales.empty()) {
    return absl::nullopt;
  }
  return locales;
}

absl::optional<ParsedManufacturers> ParseManufacturers(
    base::StringPiece manufacturers_json) {
  const auto as_value = ParseJsonAndUnnestKey(manufacturers_json, "filesMap",
                                              base::Value::Type::DICTIONARY);
  if (!as_value.has_value()) {
    return absl::nullopt;
  }
  ParsedManufacturers manufacturers;
  for (const auto iter : as_value.value().DictItems()) {
    if (!iter.second.is_string())
      continue;
    manufacturers[iter.first] = iter.second.GetString();
  }
  return manufacturers.empty() ? absl::nullopt
                               : absl::make_optional(manufacturers);
}

absl::optional<ParsedIndex> ParseForwardIndex(
    base::StringPiece forward_index_json) {
  // Firstly, we unnest the dictionary keyed by "ppdIndex."
  absl::optional<base::Value> ppd_index = ParseJsonAndUnnestKey(
      forward_index_json, "ppdIndex", base::Value::Type::DICTIONARY);
  if (!ppd_index.has_value()) {
    return absl::nullopt;
  }

  ParsedIndex parsed_index;

  // Secondly, we iterate on the key-value pairs of the ppdIndex.
  // This yields a list of leaf values (dictionaries).
  for (const auto kv : ppd_index->DictItems()) {
    absl::optional<ParsedIndexValues> values = UnnestPpdMetadata(kv.second);
    if (values.has_value()) {
      parsed_index.insert_or_assign(kv.first, values.value());
    }
  }

  if (parsed_index.empty()) {
    return absl::nullopt;
  }
  return parsed_index;
}

absl::optional<ParsedUsbIndex> ParseUsbIndex(base::StringPiece usb_index_json) {
  absl::optional<base::Value> usb_index = ParseJsonAndUnnestKey(
      usb_index_json, "usbIndex", base::Value::Type::DICTIONARY);
  if (!usb_index.has_value()) {
    return absl::nullopt;
  }

  ParsedUsbIndex parsed_usb_index;
  for (const auto kv : usb_index->DictItems()) {
    int product_id;
    if (!base::StringToInt(kv.first, &product_id)) {
      continue;
    }

    const std::string* effective_make_and_model =
        kv.second.FindStringKey("effectiveMakeAndModel");
    if (!effective_make_and_model || effective_make_and_model->empty()) {
      continue;
    }

    parsed_usb_index.insert_or_assign(product_id, *effective_make_and_model);
  }
  if (parsed_usb_index.empty()) {
    return absl::nullopt;
  }
  return parsed_usb_index;
}

absl::optional<ParsedUsbVendorIdMap> ParseUsbVendorIdMap(
    base::StringPiece usb_vendor_id_map_json) {
  absl::optional<base::Value> as_value = ParseJsonAndUnnestKey(
      usb_vendor_id_map_json, "entries", base::Value::Type::LIST);
  if (!as_value.has_value()) {
    return absl::nullopt;
  }

  ParsedUsbVendorIdMap usb_vendor_ids;
  for (const auto& usb_vendor_description : as_value->GetList()) {
    if (!usb_vendor_description.is_dict()) {
      continue;
    }

    absl::optional<int> vendor_id =
        usb_vendor_description.FindIntKey("vendorId");
    const std::string* const vendor_name =
        usb_vendor_description.FindStringKey("vendorName");
    if (!vendor_id.has_value() || !vendor_name || vendor_name->empty()) {
      continue;
    }
    usb_vendor_ids.insert_or_assign(vendor_id.value(), *vendor_name);
  }

  if (usb_vendor_ids.empty()) {
    return absl::nullopt;
  }
  return usb_vendor_ids;
}

absl::optional<ParsedPrinters> ParsePrinters(base::StringPiece printers_json) {
  const auto as_value =
      ParseJsonAndUnnestKey(printers_json, "printers", base::Value::Type::LIST);
  if (!as_value.has_value()) {
    return absl::nullopt;
  }

  ParsedPrinters printers;
  for (const auto& printer_value : as_value->GetList()) {
    if (!printer_value.is_dict()) {
      continue;
    }
    absl::optional<ParsedPrinter> printer =
        ParsePrinterFromValue(printer_value);
    if (!printer.has_value()) {
      continue;
    }
    printers.push_back(printer.value());
  }
  if (printers.empty()) {
    return absl::nullopt;
  }
  return printers;
}

absl::optional<ParsedReverseIndex> ParseReverseIndex(
    base::StringPiece reverse_index_json) {
  const absl::optional<base::Value> makes_and_models = ParseJsonAndUnnestKey(
      reverse_index_json, "reverseIndex", base::Value::Type::DICTIONARY);
  if (!makes_and_models.has_value()) {
    return absl::nullopt;
  }

  ParsedReverseIndex parsed;
  for (const auto kv : makes_and_models->DictItems()) {
    if (!kv.second.is_dict()) {
      continue;
    }

    const std::string* manufacturer = kv.second.FindStringKey("manufacturer");
    const std::string* model = kv.second.FindStringKey("model");
    if (manufacturer && model && !manufacturer->empty() && !model->empty()) {
      parsed.insert_or_assign(kv.first,
                              ReverseIndexLeaf{*manufacturer, *model});
    }
  }

  if (parsed.empty()) {
    return absl::nullopt;
  }
  return parsed;
}

}  // namespace chromeos
