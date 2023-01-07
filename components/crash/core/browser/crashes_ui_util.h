// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_BROWSER_CRASHES_UI_UTIL_H_
#define COMPONENTS_CRASH_CORE_BROWSER_CRASHES_UI_UTIL_H_

#include <stddef.h>

#include "base/values.h"

class UploadList;

namespace crash_reporter {

// Mapping between a WebUI resource (identified by |name|) and a GRIT resource
// (identified by |resource_id|).
struct CrashesUILocalizedString {
  const char* name;
  int resource_id;
};

// List of localized strings that must be added to the WebUI.
extern const CrashesUILocalizedString kCrashesUILocalizedStrings[];
extern const size_t kCrashesUILocalizedStringsCount;

// Strings used by the WebUI resources.
// Must match the constants used in the resource files.
extern const char kCrashesUICrashesJS[];
extern const char kCrashesUICrashesCSS[];
extern const char kCrashesUISadTabSVG[];
extern const char kCrashesUIRequestCrashList[];
extern const char kCrashesUIRequestCrashUpload[];
extern const char kCrashesUIShortProductName[];
extern const char kCrashesUIUpdateCrashList[];
extern const char kCrashesUIRequestSingleCrashUpload[];

// Converts and appends the most recent uploads to |out_value|.
void UploadListToValue(UploadList* upload_list, base::Value::List* out_value);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_BROWSER_CRASHES_UI_UTIL_H_
