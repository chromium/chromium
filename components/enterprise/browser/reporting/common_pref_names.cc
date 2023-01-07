// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/common_pref_names.h"

namespace enterprise_reporting {

// Boolean that indicates whether Chrome enterprise cloud reporting is enabled
// or not.
const char kCloudReportingEnabled[] =
    "enterprise_reporting.chrome_cloud_reporting";

// Boolean that indicates whether Chrome enterprise profile cloud reporting is
// enabled or not.
const char kCloudProfileReportingEnabled[] =
    "enterprise_reporting.chrome_profile_cloud_reporting";

// The timestamp of the last enterprise report upload.
const char kLastUploadTimestamp[] =
    "enterprise_reporting.last_upload_timestamp";

// The timestamp of the last enterprise report upload is succeeded.
const char kLastUploadSucceededTimestamp[] =
    "enterprise_reporting.last_upload_succeeded_timestamp";

// The report frequency
const char kCloudReportingUploadFrequency[] =
    "enterprise_reporting.upload_frequency";

}  // namespace enterprise_reporting
