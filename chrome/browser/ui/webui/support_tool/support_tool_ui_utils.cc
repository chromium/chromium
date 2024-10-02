// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui_utils.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/to_value_list.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace support_tool_ui {

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

const char kSupportTokenGenerationResultSuccess[] = "success";
const char kSupportTokenGenerationResultToken[] = "token";
const char kSupportTokenGenerationResultErrorMessage[] = "errorMessage";

}  // namespace support_tool_ui

namespace {

// Returns the human readable name corresponding to `data_collector_type`.
std::string GetDataCollectorName(
    support_tool::DataCollectorType data_collector_type) {
  switch (data_collector_type) {
    case support_tool::CHROME_INTERNAL:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_CHROME_SYSTEM_INFO);
    case support_tool::CRASH_IDS:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_CRASH_IDS);
    case support_tool::MEMORY_DETAILS:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_MEMORY_DETAILS);
    case support_tool::CHROMEOS_UI_HIERARCHY:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_UI_HIEARCHY);
    case support_tool::CHROMEOS_COMMAND_LINE:
      return l10n_util::GetStringUTF8(
          IDS_SUPPORT_TOOL_ADDITIONAL_CROS_PLATFROM_LOGS);
    case support_tool::CHROMEOS_DEVICE_EVENT:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_DEVICE_EVENT);
    case support_tool::CHROMEOS_IWL_WIFI_DUMP:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_INTEL_WIFI_DEBUG_DUMP);
    case support_tool::CHROMEOS_TOUCH_EVENTS:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_TOUCH_EVENTS);
    case support_tool::CHROMEOS_REVEN:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_CHROMEOS_FLEX_LOGS);
    case support_tool::CHROMEOS_DBUS:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_DBUS_DETAILS);
    case support_tool::CHROMEOS_NETWORK_ROUTES:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_CHROMEOS_NETWORK_ROUTES);
    case support_tool::CHROMEOS_SHILL:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_CHROMEOS_SHILL_LOGS);
    case support_tool::POLICIES:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_POLICIES);
    case support_tool::CHROMEOS_SYSTEM_STATE:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_CHROMEOS_SYSTEM_STATE);
    case support_tool::CHROMEOS_SYSTEM_LOGS:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_CHROMEOS_SYSTEM_LOGS);
    case support_tool::CHROMEOS_CHROME_USER_LOGS:
      return l10n_util::GetStringUTF8(
          IDS_SUPPORT_TOOL_CHROMEOS_CHROME_USER_LOGS);
    case support_tool::CHROMEOS_BLUETOOTH_FLOSS:
      return l10n_util::GetStringUTF8(
          IDS_SUPPORT_TOOL_CHROMEOS_BLUETOOTH_FLOSS);
    case support_tool::CHROMEOS_CONNECTED_INPUT_DEVICES:
      return l10n_util::GetStringUTF8(
          IDS_SUPPORT_TOOL_CHROMEOS_CONNECTED_INPUT_DEVICES);
    case support_tool::CHROMEOS_TRAFFIC_COUNTERS:
      return l10n_util::GetStringUTF8(
          IDS_SUPPORT_TOOL_CHROMEOS_TRAFFIC_COUNTERS);
    case support_tool::CHROMEOS_VIRTUAL_KEYBOARD:
      return l10n_util::GetStringUTF8(
          IDS_SUPPORT_TOOL_CHROMEOS_VIRTUAL_KEYBOARD);
    case support_tool::CHROMEOS_NETWORK_HEALTH:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_CHROMEOS_NETWORK_HEALTH);
    case support_tool::SIGN_IN_STATE:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_SIGN_IN);
    case support_tool::PERFORMANCE:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_PERFORMANCE);
    case support_tool::CHROMEOS_APP_SERVICE:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_CHROMEOS_APP_SERVICE);
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
// type SupportTokenGenerationResult = {
//   success: boolean,
//   token: string,
//   errorMessage: string,
// }
base::Value::Dict GetSupportTokenGenerationResult(bool success,
                                                  std::string result,
                                                  std::string error_message) {
  base::Value::Dict url_generation_response;
  url_generation_response.Set(
      support_tool_ui::kSupportTokenGenerationResultSuccess, success);
  url_generation_response.Set(
      support_tool_ui::kSupportTokenGenerationResultToken, result);
  url_generation_response.Set(
      support_tool_ui::kSupportTokenGenerationResultErrorMessage,
      error_message);
  return url_generation_response;
}

}  // namespace

// Returns the human readable name corresponding to `type_enum`.
std::string GetPIITypeDescription(redaction::PIIType type_enum) {
  switch (type_enum) {
    case redaction::PIIType::kAndroidAppStoragePath:
      // App storage path is part of information about an Android app.
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_ANDROID_APP_INFO);
    case redaction::PIIType::kEmail:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_EMAIL_ADDRESS);
    case redaction::PIIType::kGaiaID:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_GAIA_ID);
    case redaction::PIIType::kIPPAddress:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_PRINTING_IPP_ADDRESS);
    case redaction::PIIType::kIPAddress:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_IP_ADDRESS);
    case redaction::PIIType::kCellularLocationInfo:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_CELLULAR_LOCATION_INFO);
    case redaction::PIIType::kMACAddress:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_MAC_ADDRESS);
    case redaction::PIIType::kUIHierarchyWindowTitles:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_WINDOW_TITLES);
    case redaction::PIIType::kURL:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_URLS);
    case redaction::PIIType::kSerial:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_SERIAL_NUMBERS);
    case redaction::PIIType::kSSID:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_WIFI_SSID);
    case redaction::PIIType::kStableIdentifier:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_STABLE_IDENTIDIERS);
    case redaction::PIIType::kVolumeLabel:
      // Volume labels are a part of removable storage paths in various logs.
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_REMOVABLE_STORAGE_NAMES);
    case redaction::PIIType::kEAP:
      return l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_EAP);
    default:
      return "Error: Undefined";
  }
}

// type PIIDataItem = {
//   piiTypeDescription: string,
//   piiType: number,
//   detectedData: List<string>,
//   count: number,
//   keep: boolean,
// }
base::Value::List GetDetectedPIIDataItems(const PIIMap& detected_pii) {
  return base::ToValueList(detected_pii, [](const auto& detected_pii_entry) {
    const auto& [pii_key, pii_data] = detected_pii_entry;
    return base::Value::Dict()
        .Set(support_tool_ui::kPiiItemDescriptionKey,
             GetPIITypeDescription(pii_key))
        .Set(support_tool_ui::kPiiItemPIITypeKey, static_cast<int>(pii_key))
        .Set(support_tool_ui::kPiiItemDetectedDataKey,
             base::ToValueList(pii_data))
        .Set(support_tool_ui::kPiiItemCountKey,
             static_cast<int>(pii_data.size()))
        // TODO(b/200511640): Set `keep` field to the value we'll get from
        // URL's pii_masking_on query if it exists.
        .Set(support_tool_ui::kPiiItemKeepKey, true);
  });
}

std::set<redaction::PIIType> GetPIITypesToKeep(
    const base::Value::List* pii_items) {
  std::set<redaction::PIIType> pii_to_keep;
  for (const auto& item : *pii_items) {
    const base::Value::Dict* item_as_dict = item.GetIfDict();
    DCHECK(item_as_dict);
    std::optional<bool> keep =
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
    std::optional<bool> isIncluded = item_as_dict->FindBool("isIncluded");
    if (isIncluded && isIncluded.value()) {
      included_data_collectors.insert(
          static_cast<support_tool::DataCollectorType>(
              item_as_dict->FindInt("protoEnum").value()));
    }
  }
  return included_data_collectors;
}

base::Value::Dict GetStartDataCollectionResult(bool success,
                                               std::u16string error_message) {
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
    return GetSupportTokenGenerationResult(
        /*success=*/false, /*result=*/std::string(), /*error_message=*/
        l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_SELECT_DATA_COLLECTOR_ERROR));
  }
  GURL customized_url("chrome://support-tool");
  if (!case_id.empty()) {
    customized_url = net::AppendQueryParameter(
        customized_url, support_tool_ui::kSupportCaseIDQuery, case_id);
  }
  customized_url = net::AppendQueryParameter(
      customized_url, support_tool_ui::kModuleQuery,
      GetDataCollectionModuleQuery(included_data_collectors));
  return GetSupportTokenGenerationResult(/*success=*/true,
                                         /*result=*/customized_url.spec(),
                                         /*error_message=*/std::string());
}

base::Value::Dict GenerateSupportToken(
    const base::Value::List* data_collector_items) {
  base::Value::Dict url_generation_response;
  std::set<support_tool::DataCollectorType> included_data_collectors =
      GetIncludedDataCollectorTypes(data_collector_items);
  if (included_data_collectors.empty()) {
    // If there's no selected data collector to add, consider this as an error.
    return GetSupportTokenGenerationResult(
        /*success=*/false, /*result=*/std::string(), /*error_message=*/
        l10n_util::GetStringUTF8(IDS_SUPPORT_TOOL_SELECT_DATA_COLLECTOR_ERROR));
  }
  return GetSupportTokenGenerationResult(
      /*success=*/true, GetDataCollectionModuleQuery(included_data_collectors),
      /*error_message=*/std::string());
}
