// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_types.h"

#include "base/notreached.h"

namespace personal_context {

std::string_view PersonalContextNonEligibilityReasonToString(
    PersonalContextNonEligibilityReason reason) {
  switch (reason) {
    case PersonalContextNonEligibilityReason::kNotSignedIn:
      return "Not signed in";
    case PersonalContextNonEligibilityReason::kNotConsumerAccount:
      return "Not consumer account";
    case PersonalContextNonEligibilityReason::kNotAgeEligible:
      return "Not age eligible";
    case PersonalContextNonEligibilityReason::kNotLocaleEnUS:
      return "Not locale en-US";
    case PersonalContextNonEligibilityReason::kNotGeoIpUS:
      return "Not GeoIP US";
    case PersonalContextNonEligibilityReason::kNotOptedInToContext:
      return "Not opted in to context";
    case PersonalContextNonEligibilityReason::kNotPhotosAndWorkspaceAvailable:
      return "Photos and Workspace context unavailable";
    case PersonalContextNonEligibilityReason::kPersonalIntelligencePrefDisabled:
      return "Personal Intelligence preferences disabled";
    case PersonalContextNonEligibilityReason::kNotGlicFirstRun:
      return "Glic first run not complete";
    case PersonalContextNonEligibilityReason::
        kFindAndFillWithGeminiSettingsDisabled:
      return "Find and Fill with Gemini settings disabled";
    case PersonalContextNonEligibilityReason::
        kNotG1SubscriberOrAndroidPremiumDevice:
      return "Not G1 subscriber or Android premium device";
    case PersonalContextNonEligibilityReason::kEligible:
      return "Eligible";
  }
  NOTREACHED();
}

FetchContextResult::FetchContextResult() = default;

FetchContextResult::FetchContextResult(
    base::expected<const proto::Any /*response_metadata*/, ContextMemoryError>
        response,
    std::string server_request_id)
    : response(std::move(response)),
      server_request_id(std::move(server_request_id)) {}

FetchContextResult::FetchContextResult(FetchContextResult&& other) = default;

FetchContextResult::~FetchContextResult() = default;

FetchPiiEntitiesResult::FetchPiiEntitiesResult() = default;

FetchPiiEntitiesResult::FetchPiiEntitiesResult(
    base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>
        response)
    : response(std::move(response)) {}

FetchPiiEntitiesResult::FetchPiiEntitiesResult(FetchPiiEntitiesResult&& other) =
    default;

FetchPiiEntitiesResult::~FetchPiiEntitiesResult() = default;

}  // namespace personal_context
