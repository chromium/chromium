// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_UTILS_H_

#include <set>

#include "base/values.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/pii_types.h"

namespace support_tool_ui {

// Strings that contain the human readable description of feedback::PIIType
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

// String keys of the fields of PIIDataItem dictionary that Support Tool UI
// stores the detected PII to display it to user.
extern const char kPiiItemDescriptionKey[];
extern const char kPiiItemPIITypeKey[];
extern const char kPiiItemDetectedDataKey[];
extern const char kPiiItemCountKey[];
extern const char kPiiItemKeepKey[];

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
std::set<feedback::PIIType> GetPIITypesToKeep(
    const base::Value::List* pii_items);

#endif  // CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_UTILS_H_
