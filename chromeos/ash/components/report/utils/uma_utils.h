// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_UMA_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_UMA_UTILS_H_

namespace ash::report::utils {

// Enum class represents the "PsmRequest" <variants> in histograms.xml.
// It is used to generate histogram record names, so must stay in sync
// with the UMA variants definition.
enum class PsmRequest { kImport = 0, kOprf = 1, kQuery = 2 };

// Enum class represents the "PsmUseCase" <variants> in histograms.xml.
// It is used to generate histogram record names, so must stay in sync
// with the UMA variants definition.
enum class PsmUseCase { k1DA = 0, k28DA = 1, kCohort = 2, kObservation = 3 };

// Records UMA histogram for different failed check membership cases.
enum class CheckMembershipResponseCases {
  kUnknown = 0,
  kCreateOprfRequestFailed = 1,
  kOprfResponseBodyFailed = 2,
  kNotHasRlweOprfResponse = 3,
  kCreateQueryRequestFailed = 4,
  kQueryResponseBodyFailed = 5,
  kNotHasRlweQueryResponse = 6,
  kProcessQueryResponseFailed = 7,
  kMembershipResponsesSizeIsNotOne = 8,
  kIsNotPsmIdMember = 9,
  kSuccessfullySetLocalState = 10,
  kMaxValue = kSuccessfullySetLocalState,
};

// Record UMA boolean for whether a ping is required for a given use case.
void RecordIsDevicePingRequired(PsmUseCase use_case, bool is_ping_required);

// Record UMA net error code histogram for a given use case and request.
void RecordNetErrorCode(PsmUseCase use_case, PsmRequest request, int net_code);

// Record UMA check membership response enum for the PSM use case.
void RecordCheckMembershipCases(PsmUseCase use_case,
                                CheckMembershipResponseCases response_case);

}  // namespace ash::report::utils

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_UMA_UTILS_H_
