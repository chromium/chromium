// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"

namespace enterprise_reporting {

RealTimeReportGenerator::Delegate::Delegate() = default;
RealTimeReportGenerator::Delegate::~Delegate() = default;

RealTimeReportGenerator::RealTimeReportGenerator(
    ReportingDelegateFactory* delegate_factory)
    : delegate_(delegate_factory->GetRealTimeReportGeneratorDelegate()) {}
RealTimeReportGenerator::~RealTimeReportGenerator() = default;

std::vector<std::unique_ptr<google::protobuf::MessageLite>>
RealTimeReportGenerator::Generate(RealTimeReportType type, const Data& data) {
  if (!delegate_)
    return std::vector<std::unique_ptr<google::protobuf::MessageLite>>();
  return delegate_->Generate(type, data);
}

}  // namespace enterprise_reporting
