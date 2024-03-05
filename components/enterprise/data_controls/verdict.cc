// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/verdict.h"

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
Verdict Verdict::Merge(Verdict verdict_1, Verdict verdict_2) {
  if (verdict_2.level() > verdict_1.level()) {
    verdict_1.level_ = verdict_2.level();
  }

  for (auto& id_and_name : verdict_2.triggered_rules_) {
    if (verdict_1.triggered_rules_.count(id_and_name.first)) {
      continue;
    }
    verdict_1.triggered_rules_[id_and_name.first] =
        std::move(id_and_name.second);
  }

  return verdict_1;
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
