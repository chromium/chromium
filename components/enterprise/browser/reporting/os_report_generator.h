// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_OS_REPORT_GENERATOR_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_OS_REPORT_GENERATOR_H_

#include <memory>

#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_reporting {

// Returns an OS report contains basic OS information includes OS name, OS
// architecture and OS version.
std::unique_ptr<enterprise_management::OSReport> GetOSReport();

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_OS_REPORT_GENERATOR_H_
