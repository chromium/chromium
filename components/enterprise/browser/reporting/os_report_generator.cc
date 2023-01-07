// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/os_report_generator.h"

#include "components/policy/core/common/cloud/cloud_policy_util.h"

namespace enterprise_reporting {

namespace em = enterprise_management;

std::unique_ptr<em::OSReport> GetOSReport() {
  auto report = std::make_unique<em::OSReport>();
  report->set_name(policy::GetOSPlatform());
  report->set_arch(policy::GetOSArchitecture());
  report->set_version(policy::GetOSVersion());
  return report;
}

}  // namespace enterprise_reporting
