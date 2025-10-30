// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"

namespace autofill {

ActorSuggestion::ActorSuggestion() = default;
ActorSuggestion::ActorSuggestion(const ActorSuggestion&) = default;
ActorSuggestion& ActorSuggestion::operator=(const ActorSuggestion&) = default;
ActorSuggestion::~ActorSuggestion() = default;

ActorFormFillingRequest::ActorFormFillingRequest() = default;
ActorFormFillingRequest::ActorFormFillingRequest(
    const ActorFormFillingRequest&) = default;
ActorFormFillingRequest& ActorFormFillingRequest::operator=(
    const ActorFormFillingRequest&) = default;
ActorFormFillingRequest::~ActorFormFillingRequest() = default;

}  // namespace autofill
