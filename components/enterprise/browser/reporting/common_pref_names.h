// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_COMMON_PREF_NAMES_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_COMMON_PREF_NAMES_H_

#include "build/build_config.h"

namespace enterprise_reporting {

extern const char kCloudReportingEnabled[];

extern const char kCloudProfileReportingEnabled[];

extern const char kLastUploadTimestamp[];

extern const char kLastUploadSucceededTimestamp[];

extern const char kCloudReportingUploadFrequency[];

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_COMMON_PREF_NAMES_H_
