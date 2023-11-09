// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_UKM_METRICS_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_UKM_METRICS_TEST_UTILS_H_

#include <utility>
#include <vector>

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
    const DenseSet<FormType>& form_types,
    const std::vector<int64_t>& expected_metric_values);

void VerifySubmitFormUkm(
    const ukm::TestUkmRecorder* ukm_recorder,
    const FormData& form,
    AutofillMetrics::AutofillFormSubmittedState state,
    bool is_for_credit_card,
    bool has_upi_vpa_field,
    const DenseSet<FormType>& form_types,
    const FormInteractionCounts& form_interaction_counts = {});

void AppendFieldFillStatusUkm(
    const FormData& form,
    std::vector<std::vector<ExpectedUkmMetricsPair>>* expected_metrics);

void AppendFieldTypeUkm(
    const FormData& form,
    const std::vector<ServerFieldType>& heuristic_types,
    const std::vector<ServerFieldType>& server_types,
    const std::vector<ServerFieldType>& actual_types,
    std::vector<std::vector<ExpectedUkmMetricsPair>>* expected_metrics);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_UKM_METRICS_TEST_UTILS_H_
