// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_TYPE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_TYPE_H_

namespace enterprise_reporting {

enum ReportType : uint32_t {
  kFull = 0,
  kBrowserVersion = 1u << 0,
  kExtensionRequest = 2u << 1,
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_TYPE_H_
