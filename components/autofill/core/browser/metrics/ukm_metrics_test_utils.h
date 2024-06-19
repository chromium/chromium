// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_UKM_METRICS_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_UKM_METRICS_TEST_UTILS_H_

#include <utility>
#include <vector>

#include "base/metrics/metrics_hashes.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"

namespace autofill::autofill_metrics {

struct ExpectedUkmMetricsPair : public std::pair<std::string, int64_t> {
  using std::pair<std::string, int64_t>::pair;
  ExpectedUkmMetricsPair(std::string str, HtmlFieldMode mode)
      : ExpectedUkmMetricsPair(str, static_cast<int64_t>(mode)) {}
  ExpectedUkmMetricsPair(std::string str, HtmlFieldType type)
      : ExpectedUkmMetricsPair(str, static_cast<int64_t>(type)) {}

  friend std::ostream& operator<<(std::ostream& os,
                                  const ExpectedUkmMetricsPair& ukm_pair) {
    return os << "(metric_name=" << ukm_pair.first
              << ", hash=" << base::HashMetricName(ukm_pair.first)
              << ", value=" << ukm_pair.second << ")";
  }
};

void VerifyUkm(
    const ukm::TestUkmRecorder* ukm_recorder,
    const FormData& form,
    const char* event_name,
    const std::vector<std::vector<ExpectedUkmMetricsPair>>& expected_metrics);

void VerifyDeveloperEngagementUkm(
    const ukm::TestUkmRecorder* ukm_recorder,
    const FormData& form,
    const bool is_for_credit_card,
    const DenseSet<FormTypeNameForLogging>& form_types,
    const std::vector<int64_t>& expected_metric_values);

void AppendFieldFillStatusUkm(
    const FormData& form,
    std::vector<std::vector<ExpectedUkmMetricsPair>>* expected_metrics);

void AppendFieldTypeUkm(
    const FormData& form,
    const std::vector<FieldType>& heuristic_types,
    const std::vector<FieldType>& server_types,
    const std::vector<FieldType>& actual_types,
    std::vector<std::vector<ExpectedUkmMetricsPair>>* expected_metrics);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_UKM_METRICS_TEST_UTILS_H_
