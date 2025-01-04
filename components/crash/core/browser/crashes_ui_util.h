// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_BROWSER_CRASHES_UI_UTIL_H_
#define COMPONENTS_CRASH_CORE_BROWSER_CRASHES_UI_UTIL_H_

#include <stddef.h>

#include "base/values.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"

class UploadList;

namespace crash_reporter {

// Mapping between a WebUI resource (identified by |name|) and a GRIT resource
// (identified by |resource_id|).
//
// This could be a `webui::LocalizedString`, which would allow us to
// call `AddLocalizedStrings()` rather than rolling our own loop.
// However, `components/core/` isn't allowed to depend on `ui/`.
//
// TODO(crbug.com/379886427): move this out of `core/` to let it take
// on the `ui/` dependency.
struct CrashesUILocalizedString {
  const char* name;
  int resource_id;
};

// List of localized strings that must be added to the WebUI.
inline constexpr CrashesUILocalizedString kCrashesUILocalizedStrings[] = {
    {"bugLinkText", IDS_CRASH_BUG_LINK_LABEL},
    {"crashCaptureTimeFormat", IDS_CRASH_CAPTURE_TIME_FORMAT},
    {"crashCountFormat", IDS_CRASH_CRASH_COUNT_BANNER_FORMAT},
    {"crashStatus", IDS_CRASH_REPORT_STATUS},
    {"crashStatusNotUploaded", IDS_CRASH_REPORT_STATUS_NOT_UPLOADED},
    {"crashStatusPending", IDS_CRASH_REPORT_STATUS_PENDING},
    {"crashStatusPendingUserRequested",
     IDS_CRASH_REPORT_STATUS_PENDING_USER_REQUESTED},
    {"crashStatusUploaded", IDS_CRASH_REPORT_STATUS_UPLOADED},
    {"crashesTitle", IDS_CRASH_TITLE},
    {"disabledHeader", IDS_CRASH_DISABLED_HEADER},
    {"disabledMessage", IDS_CRASH_DISABLED_MESSAGE},
    {"fileSize", IDS_CRASH_REPORT_FILE_SIZE},
    {"localId", IDS_CRASH_REPORT_LOCAL_ID},
    {"noCrashesMessage", IDS_CRASH_NO_CRASHES_MESSAGE},
    {"showDeveloperDetails", IDS_CRASH_SHOW_DEVELOPER_DETAILS},
    {"uploadCrashesLinkText", IDS_CRASH_UPLOAD_MESSAGE},
    {"uploadId", IDS_CRASH_REPORT_UPLOADED_ID},
    {"uploadNowLinkText", IDS_CRASH_UPLOAD_NOW_LINK_TEXT},
    {"uploadTime", IDS_CRASH_REPORT_UPLOADED_TIME},
};

// Strings used by the WebUI resources.
// Must match the constants used in the resource files.
inline constexpr char kCrashesUIRequestCrashList[] = "requestCrashList";
inline constexpr char kCrashesUIRequestCrashUpload[] = "requestCrashUpload";
inline constexpr char kCrashesUIShortProductName[] = "shortProductName";
inline constexpr char kCrashesUIUpdateCrashList[] = "update-crash-list";
inline constexpr char kCrashesUIRequestSingleCrashUpload[] =
    "requestSingleCrashUpload";

// Converts and appends the most recent uploads to |out_value|.
void UploadListToValue(UploadList* upload_list, base::Value::List* out_value);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_BROWSER_CRASHES_UI_UTIL_H_
