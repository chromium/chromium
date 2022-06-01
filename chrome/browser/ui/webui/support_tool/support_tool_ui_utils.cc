// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui_utils.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/pii_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace support_tool_ui {

const char kAndroidAppInfo[] = "Android App Information";
const char kSSID[] = "WiFi SSID";
const char kLocationInfo[] = "Location Info";
const char kEmail[] = "Email Address";
const char kGAIA[] = "Google Account ID";
const char kStableIdentifier[] =
    "Other Stable Identifiers (e.g., Hashes or UUIDs)";
const char kIPPAddress[] = "Printing IPP Address";
const char kIPAddress[] = "IP Address";
const char kMACAddress[] = "Network MAC Address";
const char kWindowTitle[] = "Window Titles";
const char kURL[] = "URLs";
const char kSerial[] = "Device & Component Serial Numbers";
const char kRemovableStorage[] = "Removable Storage Names";

const char kPiiItemDescriptionKey[] = "piiTypeDescription";
const char kPiiItemDetectedDataKey[] = "detectedData";
const char kPiiItemPIITypeKey[] = "piiType";
const char kPiiItemCountKey[] = "count";
const char kPiiItemKeepKey[] = "keep";

}  // namespace support_tool_ui

namespace {

// Returns the human readable name corresponding to `data_collector_type`.
std::string GetPIITypeDescription(feedback::PIIType type_enum) {
  // This function will return translatable strings in future. For now, return
  // string constants until we have the translatable strings ready.
  switch (type_enum) {
    case feedback::PIIType::kAndroidAppStoragePath:
      // App storage path is part of information about an Android app.
      return support_tool_ui::kAndroidAppInfo;
    case feedback::PIIType::kEmail:
      return support_tool_ui::kEmail;
    case feedback::PIIType::kGaiaID:
      return support_tool_ui::kGAIA;
    case feedback::PIIType::kIPPAddress:
      return support_tool_ui::kIPPAddress;
    case feedback::PIIType::kIPAddress:
      return support_tool_ui::kIPAddress;
    case feedback::PIIType::kLocationInfo:
      return support_tool_ui::kLocationInfo;
    case feedback::PIIType::kMACAddress:
      return support_tool_ui::kMACAddress;
    case feedback::PIIType::kUIHierarchyWindowTitles:
      return support_tool_ui::kWindowTitle;
    case feedback::PIIType::kURL:
      return support_tool_ui::kURL;
    case feedback::PIIType::kSerial:
      return support_tool_ui::kSerial;
    case feedback::PIIType::kSSID:
      return support_tool_ui::kSSID;
    case feedback::PIIType::kStableIdentifier:
      return support_tool_ui::kStableIdentifier;
    case feedback::PIIType::kVolumeLabel:
      // Volume labels are a part of removable storage paths in various logs.
      return support_tool_ui::kRemovableStorage;
    default:
      return "Error: Undefined";
  }
}

}  // namespace

// type PIIDataItem = {
//   piiTypeDescription: string,
//   piiType: number,
//   detectedData: string,
//   count: number,
//   keep: boolean,
// }
base::Value::List GetDetectedPIIDataItems(const PIIMap& detected_pii) {
  base::Value::List detected_pii_data_items;
  for (const auto& pii_entry : detected_pii) {
    base::Value::Dict pii_data_item;
    pii_data_item.Set(support_tool_ui::kPiiItemDescriptionKey,
                      GetPIITypeDescription(pii_entry.first));
    pii_data_item.Set(support_tool_ui::kPiiItemPIITypeKey,
                      static_cast<int>(pii_entry.first));
    pii_data_item.Set(
        support_tool_ui::kPiiItemDetectedDataKey,
        base::JoinString(
            std::vector<base::StringPiece>(pii_entry.second.begin(),
                                           pii_entry.second.end()),
            // Join the PII strings with a comma in between them when displaying
            // to the user to make it more easily readable.
            ", "));
    pii_data_item.Set(support_tool_ui::kPiiItemCountKey,
                      static_cast<int>(pii_entry.second.size()));
    // TODO(b/200511640): Set `keep` field to the value we'll get from URL's
    // pii_masking_on query if it exists.
    pii_data_item.Set(support_tool_ui::kPiiItemKeepKey, false);
    detected_pii_data_items.Append(base::Value(std::move(pii_data_item)));
  }
  return detected_pii_data_items;
}

std::set<feedback::PIIType> GetPIITypesToKeep(
    const base::Value::List* pii_items) {
  std::set<feedback::PIIType> pii_to_keep;
  for (const auto& item : *pii_items) {
    const base::Value::Dict* item_as_dict = item.GetIfDict();
    DCHECK(item_as_dict);
    absl::optional<bool> keep =
        item_as_dict->FindBool(support_tool_ui::kPiiItemKeepKey);
    if (keep && keep.value()) {
      pii_to_keep.insert(static_cast<feedback::PIIType>(
          item_as_dict->FindInt(support_tool_ui::kPiiItemPIITypeKey).value()));
    }
  }
  return pii_to_keep;
}
