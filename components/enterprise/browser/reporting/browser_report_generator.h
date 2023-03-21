// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_REPORT_GENERATOR_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_REPORT_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/enterprise/browser/reporting/report_type.h"

namespace enterprise_management {
class BrowserReport;
}  // namespace enterprise_management

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace enterprise_reporting {

class ReportingDelegateFactory;

// A report generator that collects Browser related information.
class BrowserReportGenerator {
 public:
  using ReportCallback = base::OnceCallback<void(
      std::unique_ptr<enterprise_management::BrowserReport>)>;

  struct ReportedProfileData {
    std::string id;
    std::string name;
  };

  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    virtual std::string GetExecutablePath() = 0;
    virtual version_info::Channel GetChannel() = 0;
    virtual std::vector<ReportedProfileData> GetReportedProfiles() = 0;
    virtual bool IsExtendedStableChannel() = 0;
    virtual void GenerateBuildStateInfo(
        enterprise_management::BrowserReport* report) = 0;
  };

  explicit BrowserReportGenerator(ReportingDelegateFactory* delegate_factory);
  BrowserReportGenerator(const BrowserReportGenerator&) = delete;
  BrowserReportGenerator& operator=(const BrowserReportGenerator&) = delete;
  ~BrowserReportGenerator();

  // Generates a BrowserReport with the following fields:
  // - browser_version, channel, executable_path
  // - user profiles: id, name, is_detail_available (always be false).
  // - plugins: name, version, filename, description.
  void Generate(ReportType report_type, ReportCallback callback);

  // Generates user profiles info in the given report instance.
  void GenerateProfileInfo(enterprise_management::BrowserReport* report);

 private:
  std::unique_ptr<Delegate> delegate_;

  // Generates browser_version, channel, executable_path info in the given
  // report instance.
  void GenerateBasicInfo(enterprise_management::BrowserReport* report,
                         ReportType report_type);
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_REPORT_GENERATOR_H_
