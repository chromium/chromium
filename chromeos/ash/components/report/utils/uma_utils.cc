// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/uma_utils.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace ash::report::utils {

namespace {

// Convert |PsmRequest| to associated histogram variant name.
std::string PsmRequestHistogramVariant(PsmRequest psm_request) {
  switch (psm_request) {
    case PsmRequest::kImport:
      return "Import";
    case PsmRequest::kOprf:
      return "Oprf";
    case PsmRequest::kQuery:
      return "Query";
  }
}

// Convert |PsmUseCase| to associated histogram variant name.
std::string PsmUseCaseHistogramVariant(PsmUseCase psm_use_case) {
  switch (psm_use_case) {
    case PsmUseCase::k1DA:
      return "1DA";
    case PsmUseCase::k28DA:
      return "28DA";
    case PsmUseCase::kCohort:
      return "Cohort";
    case PsmUseCase::kObservation:
      return "Observation";
  }
}

}  // namespace

void RecordIsDevicePingRequired(PsmUseCase use_case, bool is_ping_required) {
  std::string variant_name =
      base::StrCat({"Ash.Report.", PsmUseCaseHistogramVariant(use_case),
                    ".IsDevicePingRequired"});
  base::UmaHistogramBoolean(variant_name, is_ping_required);
}

void RecordNetErrorCode(PsmUseCase use_case, PsmRequest request, int net_code) {
  std::string variant_name = base::StrCat(
      {"Ash.Report.Psm", PsmUseCaseHistogramVariant(use_case),
       PsmRequestHistogramVariant(request), "ResponseNetErrorCode"});
  base::UmaHistogramSparse(variant_name, net_code);
}

void RecordCheckMembershipCases(PsmUseCase use_case,
                                CheckMembershipResponseCases response_case) {
  std::string variant_name =
      base::StrCat({"Ash.Report.", PsmUseCaseHistogramVariant(use_case),
                    "CheckMembershipCases"});
  base::UmaHistogramEnumeration(variant_name, response_case);
}

}  // namespace ash::report::utils
