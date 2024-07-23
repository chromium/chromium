// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/verdict.h"

#include <utility>

namespace data_controls {

// static
Verdict Verdict::NotSet() {
  return Verdict(Rule::Level::kNotSet, {});
}

// static
Verdict Verdict::Report(TriggeredRules triggered_rules) {
  return Verdict(Rule::Level::kReport, triggered_rules);
}

// static
Verdict Verdict::Warn(TriggeredRules triggered_rules) {
  return Verdict(Rule::Level::kWarn, triggered_rules);
}

// static
Verdict Verdict::Block(TriggeredRules triggered_rules) {
  return Verdict(Rule::Level::kBlock, triggered_rules);
}

// static
Verdict Verdict::Allow() {
  return Verdict(Rule::Level::kAllow, {});
}

// static
Verdict Verdict::MergePasteVerdicts(Verdict source_verdict,
                                    Verdict destination_verdict) {
  if (source_verdict.level() > destination_verdict.level()) {
    destination_verdict.level_ = source_verdict.level();
  }

  return destination_verdict;
}

// static
Verdict Verdict::MergeCopyWarningVerdicts(Verdict source_only_verdict,
                                          Verdict os_clipboard_verdict) {
  CHECK_NE(source_only_verdict.level(), Rule::Level::kBlock);
  CHECK(source_only_verdict.level() == Rule::Level::kWarn ||
        os_clipboard_verdict.level() == Rule::Level::kWarn);

  // For OS clipboard verdicts, only the "warn" level actually triggers a
  // dialog (`kBlock` means "don't share data with the OS clipboard" and
  // `kReport` is unsupported), so the triggered rules of `os_clipboard_verdict`
  // shouldn't be merged into `source_only_verdict` for non-`kWarn` values.
  if (os_clipboard_verdict.level() == Rule::Level::kWarn) {
    source_only_verdict.triggered_rules_.insert(
        os_clipboard_verdict.triggered_rules().begin(),
        os_clipboard_verdict.triggered_rules().end());
  }

  source_only_verdict.level_ = Rule::Level::kWarn;
  return source_only_verdict;
}

Verdict::Verdict(Rule::Level level, TriggeredRules triggered_rules)
    : level_(level), triggered_rules_(std::move(triggered_rules)) {}

Verdict::~Verdict() = default;
Verdict::Verdict(Verdict&& other) = default;
Verdict& Verdict::operator=(Verdict&& other) = default;

Rule::Level Verdict::level() const {
  return level_;
}

const Verdict::TriggeredRules& Verdict::triggered_rules() const {
  return triggered_rules_;
}

}  // namespace data_controls
