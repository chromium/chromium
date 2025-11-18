// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_LENS_SUGGEST_INPUTS_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_LENS_SUGGEST_INPUTS_UTILS_H_

#include "base/functional/callback.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"

// A callback that returns the LensOverlaySuggestInputs when they are ready if
// they exist. Returns std::nullopt if they do not exist.
using LensOverlaySuggestInputsCallback = base::OnceCallback<void(
  std::optional<lens::proto::LensOverlaySuggestInputs>)>;

inline bool AreLensSuggestInputsReady(
    const std::optional<lens::proto::LensOverlaySuggestInputs>&
        suggest_inputs) {
  return suggest_inputs.has_value() &&
         suggest_inputs->has_search_session_id() &&
         suggest_inputs->has_contextual_visual_input_type() &&
         suggest_inputs->has_encoded_request_id();
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PaywallSignal)
enum class PaywallSignal {
  // Whether the paywall signal was unavailable because the page content was
  // not available.
  kUnknown = 0,
  // Whether the paywall signal was present on the current page.
  kSignalPresent = 1,
  // Whether the paywall signal was not present on the current page.
  kSignalNotPresent = 2,
  kMaxValue = kSignalNotPresent,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:PaywallSignal)

#endif  // COMPONENTS_OMNIBOX_BROWSER_LENS_SUGGEST_INPUTS_UTILS_H_
