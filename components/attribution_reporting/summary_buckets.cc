// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/summary_buckets.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/summary_operator.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SummaryOperator;

bool AreSummaryBucketsValid(const base::flat_set<uint32_t>& starts) {
  return !starts.empty() &&
         base::MakeStrictNum(starts.size()) <=
             static_cast<int>(MaxEventLevelReports::Max()) &&
         *starts.begin() > 0;
}

}  // namespace

base::expected<SummaryOperator, SourceRegistrationError> ParseSummaryOperator(
    const base::Value::Dict& dict) {
  const base::Value* value = dict.Find(kSummaryOperator);
  if (!value) {
    return SummaryOperator::kCount;
  }

  const std::string* str = value->GetIfString();
  if (!str) {
    return base::unexpected(
        SourceRegistrationError::kSummaryOperatorValueInvalid);
  } else if (*str == kSummaryOperatorCount) {
    return SummaryOperator::kCount;
  } else if (*str == kSummaryOperatorValueSum) {
    return SummaryOperator::kValueSum;
  } else {
    return base::unexpected(
        SourceRegistrationError::kSummaryOperatorValueInvalid);
  }
}

void Serialize(SummaryOperator op, base::Value::Dict& dict) {
  switch (op) {
    case SummaryOperator::kCount:
      dict.Set(kSummaryOperator, kSummaryOperatorCount);
      break;
    case SummaryOperator::kValueSum:
      dict.Set(kSummaryOperator, kSummaryOperatorValueSum);
      break;
  }
}

// static
base::expected<SummaryBuckets, SourceRegistrationError> SummaryBuckets::Parse(
    const base::Value::Dict& dict,
    MaxEventLevelReports max_event_level_reports) {
  const base::Value* value = dict.Find(kSummaryBuckets);
  if (!value) {
    return SummaryBuckets(max_event_level_reports);
  }

  const base::Value::List* list = value->GetIfList();
  if (!list || list->empty() ||
      base::MakeStrictNum(list->size()) >
          static_cast<int>(max_event_level_reports)) {
    return base::unexpected(
        SourceRegistrationError::kSummaryBucketsListInvalid);
  }

  std::vector<uint32_t> starts;
  starts.reserve(list->size());

  uint32_t prev = 0;

  for (const base::Value& item : *list) {
    ASSIGN_OR_RETURN(uint32_t start, ParseUint32(item), [](ParseError) {
      return SourceRegistrationError::kSummaryBucketsValueInvalid;
    });

    if (start <= prev) {
      return base::unexpected(
          SourceRegistrationError::kSummaryBucketsValueInvalid);
    }

    starts.push_back(start);
    prev = start;
  }

  return SummaryBuckets(
      base::flat_set<uint32_t>(base::sorted_unique, std::move(starts)));
}

SummaryBuckets::SummaryBuckets(
    const MaxEventLevelReports max_event_level_reports) {
  std::vector<uint32_t> starts;
  starts.reserve(max_event_level_reports);
  for (int i = 1; i <= max_event_level_reports; ++i) {
    starts.push_back(i);
  }
  starts_.replace(std::move(starts));
  CHECK(AreSummaryBucketsValid(starts_));
}

SummaryBuckets::SummaryBuckets(base::flat_set<uint32_t> starts)
    : starts_(std::move(starts)) {
  CHECK(AreSummaryBucketsValid(starts_));
}

SummaryBuckets::~SummaryBuckets() = default;

SummaryBuckets::SummaryBuckets(const SummaryBuckets&) = default;

SummaryBuckets& SummaryBuckets::operator=(const SummaryBuckets&) = default;

SummaryBuckets::SummaryBuckets(SummaryBuckets&&) = default;

SummaryBuckets& SummaryBuckets::operator=(SummaryBuckets&&) = default;

void SummaryBuckets::Serialize(base::Value::Dict& dict) const {
  auto list = base::Value::List::with_capacity(starts_.size());
  for (uint32_t start : starts_) {
    list.Append(Uint32ToJson(start));
  }
  dict.Set(kSummaryBuckets, std::move(list));
}

}  // namespace attribution_reporting
