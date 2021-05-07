// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/common_pref_names.h"

namespace enterprise_reporting {

#if !defined(OS_ANDROID)
// Boolean that indicates whether Chrome enterprise cloud reporting is enabled
// or not.
const char kCloudReportingEnabled[] =
    "enterprise_reporting.chrome_cloud_reporting";
#endif

// The timestamp of the last enterprise report upload.
const char kLastUploadTimestamp[] =
    "enterprise_reporting.last_upload_timestamp";

// The timestamp of the last enterprise report upload is succeeded.
const char kLastUploadSucceededTimestamp[] =
    "enterprise_reporting.last_upload_succeeded_timestamp";

}  // namespace enterprise_reporting
