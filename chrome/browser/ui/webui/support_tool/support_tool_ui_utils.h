// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_UTILS_H_

#include <set>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "url/gurl.h"

namespace support_tool_ui {

// Strings that contain the human readable description of redaction::PIIType
// enums.
extern const char kAndroidAppInfo[];
extern const char kSSID[];
extern const char kLocationInfo[];
extern const char kEmail[];
extern const char kGAIA[];
extern const char kStableIdentifier[];
extern const char kIPPAddress[];
extern const char kIPAddress[];
extern const char kMACAddress[];
extern const char kWindowTitle[];
extern const char kURL[];
extern const char kSerial[];
extern const char kRemovableStorage[];
extern const char kEAP[];

// String keys of the fields of PIIDataItem dictionary that Support Tool UI
// stores the detected PII to display it to user.
extern const char kPiiItemDescriptionKey[];
extern const char kPiiItemPIITypeKey[];
extern const char kPiiItemDetectedDataKey[];
extern const char kPiiItemCountKey[];
extern const char kPiiItemKeepKey[];

// Support Tool URL query fields.
extern const char kModuleQuery[];

// String keys that Support Tool UI uses to store data collector items in
// dictionary.
extern const char kDataCollectorIncluded[];
extern const char kDataCollectorProtoEnum[];

// String keys of URL generation result that Support Tool UI accepts.
extern const char kUrlGenerationResultSuccess[];
extern const char kUrlGenerationResultUrl[];
extern const char kUrlGenerationResultErrorMessage[];

}  // namespace support_tool_ui

// Returns PIIDataItems in `detected_pii` where PIIDataItem is
// type PIIDataItem = {
//   piiTypeDescription: string,
//   piiTypes: number[],
//   detectedData: string,
//   count: number,
//   keep: boolean,
// }
base::Value::List GetDetectedPIIDataItems(const PIIMap& detected_pii);

// Returns the set of PIITypes that has their `keep` field true in `pii_items`.
std::set<redaction::PIIType> GetPIITypesToKeep(
    const base::Value::List* pii_items);

// Returns the support case ID that's extracted from `url` with query
// `kSupportCaseIDQuery`. Returns empty string if `url` doesn't contain support
// case ID.
std::string GetSupportCaseIDFromURL(const GURL& url);

// Creates base::Value::List according to the format Support Tool UI
// accepts and fills the contents with by decoding `module_query` to its
// support_tool.pb components. Support Tool UI requests data collector items in
// format:
// type DataCollectorItem = {
//  name: string,
//  isIncluded: boolean,
//  protoEnum: number,
// }
// Returns only the data collectors that are available for user's device.
// Sets `isIncluded` as false for all items if `module_query` is empty.
base::Value::List GetDataCollectorItemsInQuery(std::string module_query);

// Creates base::Value::List according to the format Support Tool UI
// accepts and fills the contents with all data collectors with isIncluded:
// false as a default choice. Support Tool UI requests data collector items in
// format:
// type DataCollectorItem = {
//  name: string,
//  isIncluded: boolean,
//  protoEnum: number,
// }
base::Value::List GetAllDataCollectorItems();

// Creates base::Value::List according to the format Support Tool UI
// accepts and fills the contents with all data collectors with isIncluded:
// false as a default choice. Only return data collectors available for caller's
// platform.
base::Value::List GetAllDataCollectorItemsForDeviceForTesting();

std::set<support_tool::DataCollectorType> GetIncludedDataCollectorTypes(
    const base::Value::List* data_collector_items);

// Returns start data collection result in a structure that Support Tool UI
// accepts. The returned type is as follow: type StartDataCollectionResult = {
//   success: boolean,
//   errorMessage: string,
// }
base::Value::Dict GetStartDataCollectionResult(bool success,
                                               std::string error_message);

base::FilePath GetDefaultFileToExport(base::FilePath suggested_path,
                                      const std::string& case_id,
                                      base::Time timestamp);

// Generates a customized chrome://support-tool URL from given `case_id` and
// `data_collector_items` and returns the result in a format Support Tool UI
// expects. Returns a result with error when there's no data collector selected
// in `data_collector_items`.
base::Value::Dict GenerateCustomizedURL(
    std::string case_id,
    const base::Value::List* data_collector_items);

#endif  // CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_UTILS_H_
