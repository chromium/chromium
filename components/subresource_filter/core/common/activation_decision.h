// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_DECISION_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_DECISION_H_

namespace subresource_filter {

// NOTE: ActivationDecision backs a UMA histogram, so it is append-only.
enum class ActivationDecision : int {
  // The activation decision is unknown, or not known yet.
  UNKNOWN = 0,

  // Subresource filtering was activated.
  ACTIVATED = 1,

  // Did not activate because subresource filtering was disabled by the
  // highest priority configuration whose activation conditions were met.
  ACTIVATION_DISABLED = 2,

  // Did not activate because although there was a configuration whose
  // activation conditions were met, the root frame URL was allowlisted.
  URL_ALLOWLISTED = 4,

  // Did not activate because the root frame document URL did not match the
  // activation conditions of any of enabled configurations.
  ACTIVATION_CONDITIONS_NOT_MET = 5,

  // Activation was forced on the client (e.g. via devtools).
  FORCED_ACTIVATION = 6,

  // Max value for enum.
  ACTIVATION_DECISION_MAX = 7
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_DECISION_H_
