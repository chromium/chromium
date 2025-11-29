// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/profile_report_generator.h"

#include <utility>

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
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

void ProfileReportGenerator::MaybeGenerate(
    const base::FilePath& path,
    ReportType report_type,
    SecuritySignalsMode signals_mode,
    base::OnceCallback<void(std::unique_ptr<em::ChromeUserProfileInfo>)>
        callback) {
  if (!delegate_->Init(path)) {
    std::move(callback).Run(nullptr);
    return;
  }

  report_ = std::make_unique<em::ChromeUserProfileInfo>();

#if !BUILDFLAG(IS_CHROMEOS)
  delegate_->GetAffiliationInfo(report_.get());
#endif

  switch (report_type) {
    // TODO(crbug.com/441536805): Rename report type `kFull` to `kBrowser`.
    case ReportType::kFull:
      report_->set_id(path.AsUTF8Unsafe());
      break;
    case ReportType::kProfileReport:
      if (report_->has_affiliation() &&
          report_->affiliation().has_is_affiliated() &&
          report_->affiliation().is_affiliated()) {
        report_->set_id(path.AsUTF8Unsafe());
      } else {
        report_->set_id(ObfuscateFilePath(path.AsUTF8Unsafe()));
      }
      break;
    case ReportType::kBrowserVersion:
      NOTREACHED();
  }

  report_->set_is_detail_available(true);

  delegate_->GetSigninUserInfo(report_.get());
  delegate_->GetProfileName(report_.get());

  if (signals_mode != SecuritySignalsMode::kSignalsOnly &&
      extensions_enabled_ &&
      (!extensions_enabled_callback_ || extensions_enabled_callback_.Run())) {
    delegate_->GetExtensionInfo(report_.get());
  }

  if (signals_mode != SecuritySignalsMode::kSignalsOnly && is_machine_scope_) {
    delegate_->GetExtensionRequest(report_.get());

    // For profile reporting, the profile id is already in the &reportid=
    // query param. Only set the proto field for browser reports.
    delegate_->GetProfileId(report_.get());
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

  std::move(callback).Run(std::move(report_));
}

void ProfileReportGenerator::GetChromePolicyInfo() {
  AppendChromePolicyInfoIntoProfileReport(policies_, report_.get());
}

void ProfileReportGenerator::GetExtensionPolicyInfo() {
  AppendExtensionPolicyInfoIntoProfileReport(policies_, report_.get());
}

void ProfileReportGenerator::GetPolicyFetchTimestampInfo() {
#if !BUILDFLAG(IS_CHROMEOS)
  AppendCloudPolicyFetchTimestamp(
      report_.get(), delegate_->GetCloudPolicyManager(is_machine_scope_));
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

}  // namespace enterprise_reporting
