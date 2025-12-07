// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_GLIC_ACTOR_FORM_FILLING_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_GLIC_ACTOR_FORM_FILLING_TYPES_H_

#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/types/id_type.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "ui/gfx/image/image.h"

namespace autofill {

// Describes errors that can error either during suggestion generation or
// during form filling by an actor.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// There is intentionally no entry valued 0 here because 0 is emitted to UMA
// histograms in the success case.
// LINT.IfChange(ActorFormFillingError)
enum class ActorFormFillingError {
  // kSuccess = 0,
  // Any other reason that the form could not be filled.
  kOther = 1,
  // Autofill is not available on this page.
  kAutofillNotAvailable = 2,
  // The form to be filled was not found.
  kNoForm = 3,
  // There are no suggestions.
  kNoSuggestions = 4,
  kMaxValue = kNoSuggestions,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:ActorFormFillingOutcome)

std::ostream& operator<<(std::ostream& os, ActorFormFillingError error);

using ActorSuggestionId = base::IdTypeU32<class ActorSuggestionIdMarker>;

// An autofill suggestion for actor form filling.
struct ActorSuggestion {
  ActorSuggestion();
  ActorSuggestion(const ActorSuggestion&);
  ActorSuggestion& operator=(const ActorSuggestion&);
  ActorSuggestion(ActorSuggestion&&);
  ActorSuggestion& operator=(ActorSuggestion&&);
  ~ActorSuggestion();

  // A unique identifier for this suggestion.
  ActorSuggestionId id;
  // The title of the suggestion.
  std::string title;
  // The details of the suggestion.
  std::string details;
  // The optional icon for the suggestion.
  std::optional<gfx::Image> icon;
};

std::ostream& operator<<(std::ostream& os, const ActorSuggestion& suggestion);

// A request to fill a form, containing the requested data type and available
// suggestions.
struct ActorFormFillingRequest {
  ActorFormFillingRequest();
  ActorFormFillingRequest(const ActorFormFillingRequest&);
  ActorFormFillingRequest& operator=(const ActorFormFillingRequest&);
  ActorFormFillingRequest(ActorFormFillingRequest&&);
  ActorFormFillingRequest& operator=(ActorFormFillingRequest&&);
  ~ActorFormFillingRequest();

  // See the FormFillingRequest.RequestedData enum in actions_data.proto.
  using RequestedData =
      optimization_guide::proto::FormFillingRequest_RequestedData;
  RequestedData requested_data;
  std::vector<ActorSuggestion> suggestions;
};

std::ostream& operator<<(std::ostream& os,
                         const ActorFormFillingRequest& request);

// Represents the suggestion that the user selected to be filled.
struct ActorFormFillingSelection {
  ActorFormFillingSelection();
  explicit ActorFormFillingSelection(ActorSuggestionId id);
  ActorFormFillingSelection(const ActorFormFillingSelection&);
  ActorFormFillingSelection& operator=(const ActorFormFillingSelection&);
  ActorFormFillingSelection(ActorFormFillingSelection&&);
  ActorFormFillingSelection& operator=(ActorFormFillingSelection&&);
  ~ActorFormFillingSelection();
  bool operator==(const ActorFormFillingSelection&) const;

  ActorSuggestionId selected_suggestion_id;

  // TODO(crbug.com/455587407): Some credit cards do not have their CVC stored,
  // in these cases the CVC should be provided when selecting the credit card:
  // Consider adding an optional string field here for the CVC.
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_GLIC_ACTOR_FORM_FILLING_TYPES_H_
