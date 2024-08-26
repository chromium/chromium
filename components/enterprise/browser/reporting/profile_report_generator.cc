// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/profile_report_generator.h"

#include <utility>

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/browser/reporting/policy_info.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "profile_report_generator.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

ProfileReportGenerator::ProfileReportGenerator(
    ReportingDelegateFactory* delegate_factory)
    : delegate_(delegate_factory->GetProfileReportGeneratorDelegate()) {}

ProfileReportGenerator::~ProfileReportGenerator() = default;

void ProfileReportGenerator::set_extensions_enabled(bool enabled) {
  extensions_enabled_ = enabled;
}

void ProfileReportGenerator::set_policies_enabled(bool enabled) {
  policies_enabled_ = enabled;
}

void ProfileReportGenerator::set_is_machine_scope(bool is_machine) {
  is_machine_scope_ = is_machine;
}

void ProfileReportGenerator::SetExtensionsEnabledCallback(
    ExtensionsEnabledCallback callback) {
  extensions_enabled_callback_ = std::move(callback);
}

std::unique_ptr<em::ChromeUserProfileInfo>
ProfileReportGenerator::MaybeGenerate(const base::FilePath& path,
                                      const std::string& name,
                                      ReportType report_type) {
  if (!delegate_->Init(path)) {
    return nullptr;
  }

  report_ = std::make_unique<em::ChromeUserProfileInfo>();

  switch (report_type) {
    case ReportType::kFull:
      report_->set_id(path.AsUTF8Unsafe());
      break;
    case ReportType::kProfileReport:
      report_->set_id(ObfuscateFilePath(path.AsUTF8Unsafe()));
      break;
    case ReportType::kBrowserVersion:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  report_->set_name(name);
  report_->set_is_detail_available(true);

  delegate_->GetSigninUserInfo(report_.get());

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  delegate_->GetAffiliationInfo(report_.get());
#endif

  if (extensions_enabled_ &&
      (!extensions_enabled_callback_ || extensions_enabled_callback_.Run())) {
    delegate_->GetExtensionInfo(report_.get());
  }

  if (is_machine_scope_) {
    delegate_->GetExtensionRequest(report_.get());
  }

  if (policies_enabled_) {
    // TODO(crbug.com/40635691): Upload policy error as their IDs.
    auto client = delegate_->MakePolicyConversionsClient(is_machine_scope_);
    // `client` may not be provided in unit test.
    if (client) {
      policies_ = policy::PolicyConversions(std::move(client))
                      .EnableConvertTypes(false)
                      .EnablePrettyPrint(false)
                      .ToValueDict();
      GetChromePolicyInfo();
      GetExtensionPolicyInfo();
      GetPolicyFetchTimestampInfo();
    } else {
      CHECK_IS_TEST();
    }
  }

  return std::move(report_);
}

void ProfileReportGenerator::GetChromePolicyInfo() {
  AppendChromePolicyInfoIntoProfileReport(policies_, report_.get());
}

void ProfileReportGenerator::GetExtensionPolicyInfo() {
  AppendExtensionPolicyInfoIntoProfileReport(policies_, report_.get());
}

void ProfileReportGenerator::GetPolicyFetchTimestampInfo() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  AppendCloudPolicyFetchTimestamp(
      report_.get(), delegate_->GetCloudPolicyManager(is_machine_scope_));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace enterprise_reporting
