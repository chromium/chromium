// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_TYPE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_TYPE_H_

namespace enterprise_reporting {

enum RealTimeReportType {
  kExtensionRequest = 0,
  kLegacyTech,
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_TYPE_H_
