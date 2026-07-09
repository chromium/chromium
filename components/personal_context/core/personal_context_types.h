// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_TYPES_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_TYPES_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/proto/context_memory_service.pb.h"

namespace personal_context {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PersonalContextNonEligibilityReason)
enum class PersonalContextNonEligibilityReason {
  kNotSignedIn = 0,
  kNotConsumerAccount = 1,
  kNotAgeEligible = 2,
  kNotLocaleEnUS = 3,
  kNotGeoIpUS = 4,
  kNotOptedInToContext = 5,
  kNotPhotosAndWorkspaceAvailable = 6,
  kPersonalIntelligencePrefDisabled = 7,
  kNotG1Subscriber = 8,
  kNotAndroidPremiumDevice = 9,
  kEligible = 10,
  kMaxValue = kEligible
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:PersonalContextNonEligibilityReason)

// Tracks the global eligibility state of the feature for the current profile.
// Used by consuming features to determine both feature execution and UI
// entrypoint visibility.
enum class PersonalContextEligibilityState {
  kDisabledNotEligible = 0,  // Not eligible.
  kDisabledNeedsOptIn = 1,   // Not eligible, requires account opt-in.
  kEligible = 2              // Eligible.
};

// Defines the result of a PersonalContextService::FetchContext operation.
struct FetchContextResult {
  FetchContextResult();
  explicit FetchContextResult(
      base::expected<const proto::Any /*response_metadata*/, ContextMemoryError>
          response,
      std::string server_request_id = "");
  FetchContextResult(FetchContextResult&& other);
  ~FetchContextResult();

  // The server response, containing either the feature-specific metadata
  // (originally packed in an Any proto) or a ContextMemoryError.
  base::expected<const proto::Any /*response_metadata*/, ContextMemoryError>
      response;

  // The server request ID, used to identify the request in the logs.
  std::string server_request_id;
};

// Callback for receiving the result of a FetchContext call.
using FetchContextCallback = base::OnceCallback<void(FetchContextResult)>;

// Defines the result of a PersonalContextService::FetchPiiEntities operation.
struct FetchPiiEntitiesResult {
  FetchPiiEntitiesResult();
  explicit FetchPiiEntitiesResult(
      base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>
          response);
  FetchPiiEntitiesResult(FetchPiiEntitiesResult&& other);
  ~FetchPiiEntitiesResult();

  base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>
      response;
};

// Callback for receiving the result of a FetchPiiEntities call.
using FetchPiiContextCallback =
    base::OnceCallback<void(FetchPiiEntitiesResult)>;

// Optional parameters for PersonalContextService::FetchContext
struct ContextMemoryRequestOptions {
  // Sets the X-Server-Timeout header of the HTTP request
  std::optional<base::TimeDelta> request_timeout;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_TYPES_H_
