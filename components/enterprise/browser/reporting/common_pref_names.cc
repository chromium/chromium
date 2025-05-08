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

// The timestamp of when the last security signals upload is attempted.
const char kLastSignalsUploadAttemptTimestamp[] =
    "enterprise_reporting.last_signals_upload_attempt_timestamp";

// The timestamp of when the last security signals upload is succeeded.
const char kLastSignalsUploadSucceededTimestamp[] =
    "enterprise_reporting.last_signals_upload_succeeded_timestamp";

// The configuration for the latest successful security signals upload.
const char kLastSignalsUploadSucceededConfig[] =
    "enterprise_reporting.last_signals_upload_succeeded_config";

// The report frequency
const char kCloudReportingUploadFrequency[] =
    "enterprise_reporting.upload_frequency";

// The state of the user security signals reporting feature.
const char kUserSecuritySignalsReporting[] =
    "enterprise_reporting.user_security_signals.enabled";

// Whether user security signal reports should be uploaded with cookies or not.
const char kUserSecurityAuthenticatedReporting[] =
    "enterprise_reporting.user_security_signals.authenticated";

}  // namespace enterprise_reporting
