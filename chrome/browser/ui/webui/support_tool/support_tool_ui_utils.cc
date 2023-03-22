// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui_utils.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

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
const char kEAP[] = "EAP Network Authentication Information";

const char kPiiItemDescriptionKey[] = "piiTypeDescription";
const char kPiiItemDetectedDataKey[] = "detectedData";
const char kPiiItemPIITypeKey[] = "piiType";
const char kPiiItemCountKey[] = "count";
const char kPiiItemKeepKey[] = "keep";

const char kSupportCaseIDQuery[] = "case_id";
const char kModuleQuery[] = "module";

const char kDataCollectorName[] = "name";
const char kDataCollectorProtoEnum[] = "protoEnum";
const char kDataCollectorIncluded[] = "isIncluded";

const char kUrlGenerationResultSuccess[] = "success";
const char kUrlGenerationResultUrl[] = "url";
const char kUrlGenerationResultErrorMessage[] = "errorMessage";

}  // namespace support_tool_ui

namespace {

// Returns the human readable name corresponding to `data_collector_type`.
std::string GetPIITypeDescription(redaction::PIIType type_enum) {
  // This function will return translatable strings in future. For now, return
  // string constants until we have the translatable strings ready.
  switch (type_enum) {
    case redaction::PIIType::kAndroidAppStoragePath:
      // App storage path is part of information about an Android app.
      return support_tool_ui::kAndroidAppInfo;
    case redaction::PIIType::kEmail:
      return support_tool_ui::kEmail;
    case redaction::PIIType::kGaiaID:
      return support_tool_ui::kGAIA;
    case redaction::PIIType::kIPPAddress:
      return support_tool_ui::kIPPAddress;
    case redaction::PIIType::kIPAddress:
      return support_tool_ui::kIPAddress;
    case redaction::PIIType::kLocationInfo:
      return support_tool_ui::kLocationInfo;
    case redaction::PIIType::kMACAddress:
      return support_tool_ui::kMACAddress;
    case redaction::PIIType::kUIHierarchyWindowTitles:
      return support_tool_ui::kWindowTitle;
    case redaction::PIIType::kURL:
      return support_tool_ui::kURL;
    case redaction::PIIType::kSerial:
      return support_tool_ui::kSerial;
    case redaction::PIIType::kSSID:
      return support_tool_ui::kSSID;
    case redaction::PIIType::kStableIdentifier:
      return support_tool_ui::kStableIdentifier;
    case redaction::PIIType::kVolumeLabel:
      // Volume labels are a part of removable storage paths in various logs.
      return support_tool_ui::kRemovableStorage;
    case redaction::PIIType::kEAP:
      return support_tool_ui::kEAP;
    default:
      return "Error: Undefined";
  }
}

// Returns the human readable name corresponding to `data_collector_type`.
std::string GetDataCollectorName(
    support_tool::DataCollectorType data_collector_type) {
  // This function will return translatable strings in future. For now, return
  // string constants until we have the translatable strings ready.
  switch (data_collector_type) {
    case support_tool::CHROME_INTERNAL:
      return "Chrome System Information";
    case support_tool::CRASH_IDS:
      return "Crash IDs";
    case support_tool::MEMORY_DETAILS:
      return "Memory Details";
    case support_tool::CHROMEOS_UI_HIERARCHY:
      return "UI Hierarchy";
    case support_tool::CHROMEOS_COMMAND_LINE:
      return "Additional Chrome OS Platform Logs";
    case support_tool::CHROMEOS_DEVICE_EVENT:
      return "Device Event";
    case support_tool::CHROMEOS_IWL_WIFI_DUMP:
      return "Intel WiFi NICs Debug Dump";
    case support_tool::CHROMEOS_TOUCH_EVENTS:
      return "Touch Events";
    case support_tool::CHROMEOS_CROS_API:
      return "LaCrOS System Information";
    case support_tool::CHROMEOS_LACROS:
      return "LaCrOS";
    case support_tool::CHROMEOS_REVEN:
      return "Chrome OS Flex Logs";
    case support_tool::CHROMEOS_DBUS:
      return "DBus Details";
    case support_tool::CHROMEOS_NETWORK_ROUTES:
      return "Chrome OS Network Routes";
    case support_tool::CHROMEOS_SHILL:
      return "Chrome OS Shill (Connection Manager) Logs";
    case support_tool::POLICIES:
      return "Policies";
    case support_tool::CHROMEOS_SYSTEM_STATE:
      return "Chrome OS System State and Logs";
    case support_tool::CHROMEOS_SYSTEM_LOGS:
      return "ChromeOS System Logs";
    case support_tool::CHROMEOS_CHROME_USER_LOGS:
      return "Chrome OS Chrome User Logs";
    case support_tool::CHROMEOS_BLUETOOTH_FLOSS:
      return "ChromeOS Bluetooth Floss";
    case support_tool::CHROMEOS_CONNECTED_INPUT_DEVICES:
      return "ChromeOS Connected Input Devices";
    case support_tool::CHROMEOS_TRAFFIC_COUNTERS:
      return "ChromeOS Traffic Counters";
    case support_tool::CHROMEOS_VIRTUAL_KEYBOARD:
      return "ChromeOS Virtual Keyboard";
    case support_tool::CHROMEOS_NETWORK_HEALTH:
      return "ChromeOS Network Health";
    default:
      return "Error: Undefined";
  }
}

// Decodes `module_query` string and initializes contents of `module`.
void InitDataCollectionModuleFromURLQuery(
    support_tool::DataCollectionModule* module,
    const std::string& module_query) {
  std::string query_decoded;
  if (!module_query.empty() &&
      base::Base64UrlDecode(module_query,
                            base::Base64UrlDecodePolicy::IGNORE_PADDING,
                            &query_decoded)) {
    module->ParseFromString(query_decoded);
  }
}

// Returns data collector item for `type`. Sets isIncluded field true if
// `module` contains `type`.
base::Value::Dict GetDataCollectorItemForType(
    const support_tool::DataCollectionModule& module,
    const support_tool::DataCollectorType& type) {
  base::Value::Dict dict;
  dict.Set(support_tool_ui::kDataCollectorName, GetDataCollectorName(type));
  dict.Set(support_tool_ui::kDataCollectorProtoEnum, type);
  dict.Set(support_tool_ui::kDataCollectorIncluded,
           base::Contains(module.included_data_collectors(), type));
  return dict;
}

// Returns data collector item for `type`. Sets isIncluded to false for all data
// collector items.
base::Value::Dict GetDataCollectorItemForType(
    const support_tool::DataCollectorType& type) {
  base::Value::Dict dict;
  dict.Set(support_tool_ui::kDataCollectorName, GetDataCollectorName(type));
  dict.Set(support_tool_ui::kDataCollectorProtoEnum, type);
  dict.Set(support_tool_ui::kDataCollectorIncluded, false);
  return dict;
}

std::string GetDataCollectionModuleQuery(
    std::set<support_tool::DataCollectorType> included_data_collectors) {
  support_tool::DataCollectionModule module;
  for (const auto& data_collector : included_data_collectors) {
    module.add_included_data_collectors(data_collector);
  }
  std::string module_serialized;
  module.SerializeToString(&module_serialized);
  std::string data_collection_url_query;
  base::Base64UrlEncode(module_serialized,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &data_collection_url_query);
  return data_collection_url_query;
}

// Returns a URL generation result in the type Support Tool UI expects.
// type UrlGenerationResult = {
//   success: boolean,
//   url: string,
//   errorMessage: string,
// }
base::Value::Dict GetURLGenerationResult(bool success,
                                         std::string url,
                                         std::string error_message) {
  base::Value::Dict url_generation_response;
  url_generation_response.Set(support_tool_ui::kUrlGenerationResultSuccess,
                              success);
  url_generation_response.Set(support_tool_ui::kUrlGenerationResultUrl, url);
  url_generation_response.Set(support_tool_ui::kUrlGenerationResultErrorMessage,
                              error_message);
  return url_generation_response;
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
    pii_data_item.Set(support_tool_ui::kPiiItemKeepKey, true);
    detected_pii_data_items.Append(std::move(pii_data_item));
  }
  return detected_pii_data_items;
}

std::set<redaction::PIIType> GetPIITypesToKeep(
    const base::Value::List* pii_items) {
  std::set<redaction::PIIType> pii_to_keep;
  for (const auto& item : *pii_items) {
    const base::Value::Dict* item_as_dict = item.GetIfDict();
    DCHECK(item_as_dict);
    absl::optional<bool> keep =
        item_as_dict->FindBool(support_tool_ui::kPiiItemKeepKey);
    if (keep && keep.value()) {
      pii_to_keep.insert(static_cast<redaction::PIIType>(
          item_as_dict->FindInt(support_tool_ui::kPiiItemPIITypeKey).value()));
    }
  }
  return pii_to_keep;
}

std::string GetSupportCaseIDFromURL(const GURL& url) {
  std::string support_case_id;
  if (url.has_query()) {
    net::GetValueForKeyInQuery(url, support_tool_ui::kSupportCaseIDQuery,
                               &support_case_id);
  }
  return support_case_id;
}

base::Value::List GetDataCollectorItemsInQuery(std::string module_query) {
  base::Value::List data_collector_list;
  support_tool::DataCollectionModule module;
  InitDataCollectionModuleFromURLQuery(&module, module_query);
  for (const auto& type : GetAllAvailableDataCollectorsOnDevice()) {
    data_collector_list.Append(GetDataCollectorItemForType(module, type));
  }
  return data_collector_list;
}

base::Value::List GetAllDataCollectorItems() {
  base::Value::List data_collector_list;
  for (const auto& type : GetAllDataCollectors()) {
    data_collector_list.Append(GetDataCollectorItemForType(type));
  }
  return data_collector_list;
}

base::Value::List GetAllDataCollectorItemsForDeviceForTesting() {
  base::Value::List data_collector_list;
  for (const auto& type : GetAllAvailableDataCollectorsOnDevice()) {
    data_collector_list.Append(GetDataCollectorItemForType(type));
  }
  return data_collector_list;
}

std::set<support_tool::DataCollectorType> GetIncludedDataCollectorTypes(
    const base::Value::List* data_collector_items) {
  std::set<support_tool::DataCollectorType> included_data_collectors;
  for (const auto& item : *data_collector_items) {
    const base::Value::Dict* item_as_dict = item.GetIfDict();
    DCHECK(item_as_dict);
    absl::optional<bool> isIncluded = item_as_dict->FindBool("isIncluded");
    if (isIncluded && isIncluded.value()) {
      included_data_collectors.insert(
          static_cast<support_tool::DataCollectorType>(
              item_as_dict->FindInt("protoEnum").value()));
    }
  }
  return included_data_collectors;
}

base::Value::Dict GetStartDataCollectionResult(bool success,
                                               std::string error_message) {
  base::Value::Dict result;
  result.Set("success", success);
  result.Set("errorMessage", error_message);
  return result;
}

base::Value::Dict GenerateCustomizedURL(
    std::string case_id,
    const base::Value::List* data_collector_items) {
  base::Value::Dict url_generation_response;
  std::set<support_tool::DataCollectorType> included_data_collectors =
      GetIncludedDataCollectorTypes(data_collector_items);
  if (included_data_collectors.empty()) {
    // If there's no selected data collector to add, consider this as an error.
    return GetURLGenerationResult(
        /*success=*/false, /*url=*/std::string(), /*error_message=*/
        "No data collectors included. Please select a data collector.");
  }
  GURL customized_url("chrome://support-tool");
  if (!case_id.empty()) {
    customized_url = net::AppendQueryParameter(
        customized_url, support_tool_ui::kSupportCaseIDQuery, case_id);
  }
  customized_url = net::AppendQueryParameter(
      customized_url, support_tool_ui::kModuleQuery,
      GetDataCollectionModuleQuery(included_data_collectors));
  return GetURLGenerationResult(/*success=*/true, /*url=*/customized_url.spec(),
                                /*error_message=*/std::string());
}
