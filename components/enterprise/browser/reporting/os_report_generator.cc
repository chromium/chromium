// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/os_report_generator.h"

#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif  // BUILDFLAG(IS_WIN)

namespace enterprise_reporting {

namespace em = enterprise_management;

#if BUILDFLAG(IS_WIN)
namespace {

em::OSReport::VersionType ConvertVersionType(
    base::win::VersionType version_type) {
  switch (version_type) {
    case base::win::VersionType::SUITE_HOME:
      return em::OSReport::HOME;
    case base::win::VersionType::SUITE_PROFESSIONAL:
      return em::OSReport::PROFESSIONAL;
    case base::win::VersionType::SUITE_SERVER:
      return em::OSReport::SERVER;
    case base::win::VersionType::SUITE_ENTERPRISE:
      return em::OSReport::ENTERPRISE;
    case base::win::VersionType::SUITE_EDUCATION:
      return em::OSReport::EDUCATION;
    case base::win::VersionType::SUITE_EDUCATION_PRO:
      return em::OSReport::EDUCATION_PRO;
    case base::win::VersionType::SUITE_LAST:
      return em::OSReport::UNKNOWN;
  }
}
}  // namespace
#endif  // BUILDFLAG(IS_WIN)

std::unique_ptr<em::OSReport> GetOSReport() {
  auto report = std::make_unique<em::OSReport>();
  report->set_name(policy::GetOSPlatform());
  report->set_arch(policy::GetOSArchitecture());
  report->set_version(policy::GetOSVersion());

#if BUILDFLAG(IS_WIN)
  report->set_version_type(
      ConvertVersionType(base::win::OSInfo::GetInstance()->version_type()));
#endif  // BUILDFLAG(IS_WIN)
  return report;
}

}  // namespace enterprise_reporting
