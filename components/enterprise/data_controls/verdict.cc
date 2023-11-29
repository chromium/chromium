// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/verdict.h"

#include <utility>

namespace data_controls {

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

Verdict::Verdict(Rule::Level level,
                 base::OnceClosure initial_report_closure,
                 base::OnceClosure bypass_report_closure)
    : level_(level),
      initial_report_closure_(std::move(initial_report_closure)),
      bypass_report_closure_(std::move(bypass_report_closure)) {
  DCHECK_EQ(level == Rule::Level::kNotSet || level == Rule::Level::kAllow,
            initial_report_closure_.is_null());
  DCHECK_EQ(level == Rule::Level::kWarn, !bypass_report_closure_.is_null());
}

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
