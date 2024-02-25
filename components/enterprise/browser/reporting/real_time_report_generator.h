// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_GENERATOR_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_GENERATOR_H_

#include <memory>
#include <vector>

#include "components/enterprise/browser/reporting/real_time_report_type.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace enterprise_reporting {

class ReportingDelegateFactory;

// Generator of reports that is uploaded with the ERP (Encrypted Reporting
// Pipeline). The reports generated here should be relatively small and can be
// uploaded much more frequently than the CBCM status report.
class RealTimeReportGenerator {
 public:
  struct Data {
    bool operator==(const Data&) const = default;
  };
  // Delegate class that is used to collect information and generate reports
  // outside the //components. For example, RealTimeReportGeneratorDesktop
  // actual_report chrome/browser/enterprise/reporting.
  class Delegate {
   public:
    Delegate();
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate();

    virtual std::vector<std::unique_ptr<google::protobuf::MessageLite>>
    Generate(RealTimeReportType type, const Data& data) = 0;
  };

  explicit RealTimeReportGenerator(ReportingDelegateFactory* delegate_factory);
  RealTimeReportGenerator(const RealTimeReportGenerator&) = delete;
  RealTimeReportGenerator& operator=(const RealTimeReportGenerator&) = delete;
  virtual ~RealTimeReportGenerator();

  // Generates and returns reports for |type|. Multiple reports can be generated
  // together in case of previous events are not generated successfully.
  virtual std::vector<std::unique_ptr<google::protobuf::MessageLite>> Generate(
      RealTimeReportType type,
      const Data& data);

 private:
  std::unique_ptr<Delegate> delegate_;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REAL_TIME_REPORT_GENERATOR_H_
