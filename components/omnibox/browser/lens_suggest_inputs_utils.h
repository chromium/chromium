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
  std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs) {
return suggest_inputs.has_value() &&
       suggest_inputs->has_search_session_id() &&
       suggest_inputs->has_contextual_visual_input_type() &&
       suggest_inputs->has_encoded_request_id();
}

#endif  // COMPONENTS_OMNIBOX_BROWSER_LENS_SUGGEST_INPUTS_UTILS_H_
