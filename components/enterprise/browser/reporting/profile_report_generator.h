// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_PROFILE_REPORT_GENERATOR_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_PROFILE_REPORT_GENERATOR_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class FilePath;
}

namespace policy {
class CloudPolicyManager;
}

namespace enterprise_reporting {

class ReportingDelegateFactory;

/**
 * A report generator that collects Profile related information that is selected
 * by policies.
 */
class ProfileReportGenerator {
 public:
  using ExtensionsEnabledCallback = base::RepeatingCallback<bool()>;

  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Fetch the profile for the given path, and store it for report generation.
    // Returns false if the profile can't be retrieved.
    virtual bool Init(const base::FilePath& path) = 0;

    // Sets sign-in information in the report, including email and gaia id.
    virtual void GetSigninUserInfo(
        enterprise_management::ChromeUserProfileInfo* report) = 0;
    // Set affiliation information in the report.
    virtual void GetAffiliationInfo(
        enterprise_management::ChromeUserProfileInfo* report) = 0;
    // Sets installed extension information in the report.
    virtual void GetExtensionInfo(
        enterprise_management::ChromeUserProfileInfo* report) = 0;
    // Sets extension requests information in the report.
    virtual void GetExtensionRequest(
        enterprise_management::ChromeUserProfileInfo* report) = 0;

    // Returns a new platform-specific policy conversions client.
    virtual std::unique_ptr<policy::PolicyConversionsClient>
    MakePolicyConversionsClient(bool is_machine_scope) = 0;
    // Get a pointer to the current platform's cloud policy manager.
    virtual policy::CloudPolicyManager* GetCloudPolicyManager(
        bool is_machine_scope) = 0;
  };

  explicit ProfileReportGenerator(ReportingDelegateFactory* delegate_factory);
  ProfileReportGenerator(const ProfileReportGenerator&) = delete;
  ProfileReportGenerator& operator=(const ProfileReportGenerator&) = delete;
  ~ProfileReportGenerator();

  void set_extensions_enabled(bool enabled);
  void set_policies_enabled(bool enabled);
  void set_is_machine_scope(bool is_machine);

  // Pass a callback to enable/disable extension report with dynamic condition.
  void SetExtensionsEnabledCallback(ExtensionsEnabledCallback callback);

  // Generates a report for the profile associated with |path| and |name| if
  // it's activated, and returns the report. The report is null if it can't be
  // generated.
  std::unique_ptr<enterprise_management::ChromeUserProfileInfo> MaybeGenerate(
      const base::FilePath& path,
      const std::string& name,
      ReportType report_type);

 protected:
  void GetChromePolicyInfo();
  void GetExtensionPolicyInfo();
  void GetPolicyFetchTimestampInfo();

 private:
  std::unique_ptr<Delegate> delegate_;
  base::Value::Dict policies_;

  bool extensions_enabled_ = true;
  bool policies_enabled_ = true;
  bool is_machine_scope_ = true;

  base::RepeatingCallback<bool()> extensions_enabled_callback_;

  std::unique_ptr<enterprise_management::ChromeUserProfileInfo> report_;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_PROFILE_REPORT_GENERATOR_H_
