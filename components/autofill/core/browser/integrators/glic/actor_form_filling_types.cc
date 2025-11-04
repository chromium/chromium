// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"

#include <ostream>

#include "base/notreached.h"

namespace autofill {

std::ostream& operator<<(std::ostream& os, ActorFormFillingError error) {
  switch (error) {
    case ActorFormFillingError::kOther:
      return os << "kOther";
    case ActorFormFillingError::kAutofillNotAvailable:
      return os << "kAutofillNotAvailable";
    case ActorFormFillingError::kNoForm:
      return os << "kNoForm";
    case ActorFormFillingError::kNoSuggestions:
      return os << "kNoSuggestions";
  }
  NOTREACHED();
}

ActorSuggestion::ActorSuggestion() = default;
ActorSuggestion::ActorSuggestion(const ActorSuggestion&) = default;
ActorSuggestion& ActorSuggestion::operator=(const ActorSuggestion&) = default;
ActorSuggestion::ActorSuggestion(ActorSuggestion&&) = default;
ActorSuggestion& ActorSuggestion::operator=(ActorSuggestion&&) = default;
ActorSuggestion::~ActorSuggestion() = default;

ActorFormFillingRequest::ActorFormFillingRequest() = default;
ActorFormFillingRequest::ActorFormFillingRequest(
    const ActorFormFillingRequest&) = default;
ActorFormFillingRequest& ActorFormFillingRequest::operator=(
    const ActorFormFillingRequest&) = default;
ActorFormFillingRequest::ActorFormFillingRequest(ActorFormFillingRequest&&) =
    default;
ActorFormFillingRequest& ActorFormFillingRequest::operator=(
    ActorFormFillingRequest&&) = default;
ActorFormFillingRequest::~ActorFormFillingRequest() = default;

ActorFormFillingSelection::ActorFormFillingSelection() = default;

ActorFormFillingSelection::ActorFormFillingSelection(ActorSuggestionId id)
    : selected_suggestion_id(id) {}

ActorFormFillingSelection::ActorFormFillingSelection(
    const ActorFormFillingSelection&) = default;
ActorFormFillingSelection& ActorFormFillingSelection::operator=(
    const ActorFormFillingSelection&) = default;
ActorFormFillingSelection::ActorFormFillingSelection(
    ActorFormFillingSelection&&) = default;
ActorFormFillingSelection& ActorFormFillingSelection::operator=(
    ActorFormFillingSelection&&) = default;
ActorFormFillingSelection::~ActorFormFillingSelection() = default;

}  // namespace autofill
