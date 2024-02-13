// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/verdict.h"

#include <utility>

namespace data_controls {

namespace {

base::OnceClosure MergedReportClosure(
    base::OnceClosure source_report_closure,
    base::OnceClosure destination_report_closure) {
  if (source_report_closure) {
    if (destination_report_closure) {
      return base::BindOnce(
          [](base::OnceClosure source_report_closure,
             base::OnceClosure destination_report_closure) {
            std::move(source_report_closure).Run();
            std::move(destination_report_closure).Run();
          },
          std::move(source_report_closure),
          std::move(destination_report_closure));
    }
    return source_report_closure;
  }
  return destination_report_closure;
}

}  // namespace

// static
Verdict Verdict::NotSet() {
  return Verdict(Rule::Level::kNotSet);
}

// static
Verdict Verdict::Report(base::OnceClosure initial_report_closure) {
  return Verdict(Rule::Level::kReport, std::move(initial_report_closure));
}

// static
Verdict Verdict::Warn(base::OnceClosure initial_report_closure,
                      base::OnceClosure bypass_report_closure) {
  return Verdict(Rule::Level::kWarn, std::move(initial_report_closure),
                 std::move(bypass_report_closure));
}

// static
Verdict Verdict::Block(base::OnceClosure initial_report_closure) {
  return Verdict(Rule::Level::kBlock, std::move(initial_report_closure));
}

// static
Verdict Verdict::Allow() {
  return Verdict(Rule::Level::kAllow);
}

// static
Verdict Verdict::Merge(Verdict source_profile_verdict,
                       Verdict destination_profile_verdict) {
  Rule::Level level = source_profile_verdict.level();
  if (destination_profile_verdict.level() > level) {
    level = destination_profile_verdict.level();
  }

  return Verdict(level,
                 MergedReportClosure(
                     source_profile_verdict.TakeInitialReportClosure(),
                     destination_profile_verdict.TakeInitialReportClosure()),
                 MergedReportClosure(
                     source_profile_verdict.TakeBypassReportClosure(),
                     destination_profile_verdict.TakeBypassReportClosure()));
}

Verdict::Verdict(Rule::Level level,
                 base::OnceClosure initial_report_closure,
                 base::OnceClosure bypass_report_closure)
    : level_(level),
      initial_report_closure_(std::move(initial_report_closure)),
      bypass_report_closure_(std::move(bypass_report_closure)) {}

Verdict::~Verdict() = default;
Verdict::Verdict(Verdict&& other) = default;
Verdict& Verdict::operator=(Verdict&& other) = default;

Rule::Level Verdict::level() const {
  return level_;
}

base::OnceClosure Verdict::TakeInitialReportClosure() {
  return std::move(initial_report_closure_);
}

base::OnceClosure Verdict::TakeBypassReportClosure() {
  return std::move(bypass_report_closure_);
}

}  // namespace data_controls
