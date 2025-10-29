// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_GLIC_ACTOR_FORM_FILLING_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_GLIC_ACTOR_FORM_FILLING_TYPES_H_

#include <optional>
#include <string>
#include <vector>

#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "ui/gfx/image/image.h"

namespace autofill {

// An autofill suggestion for actor form filling.
struct ActorSuggestion {
  ActorSuggestion();
  ActorSuggestion(const ActorSuggestion&);
  ActorSuggestion& operator=(const ActorSuggestion&);
  ~ActorSuggestion();

  // A unique identifier for this suggestion.
  std::string id;
  // The title of the suggestion.
  std::string title;
  // The details of the suggestion.
  std::string details;
  // The optional icon for the suggestion.
  std::optional<gfx::Image> icon;
};

// A request to fill a form, containing the requested data type and available
// suggestions.
struct ActorFormFillingRequest {
  ActorFormFillingRequest();
  ~ActorFormFillingRequest();
  ActorFormFillingRequest(const ActorFormFillingRequest&);
  ActorFormFillingRequest& operator=(const ActorFormFillingRequest&);
  ActorFormFillingRequest(ActorFormFillingRequest&&);
  ActorFormFillingRequest& operator=(ActorFormFillingRequest&&);

  // See the FormFillingRequest.RequestedData enum in actions_data.proto.
  optimization_guide::proto::FormFillingRequest_RequestedData requested_data;
  std::vector<ActorSuggestion> suggestions;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_GLIC_ACTOR_FORM_FILLING_TYPES_H_
