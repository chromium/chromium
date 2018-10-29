// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_generation_util.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/common/autofill_features.h"
#include "ui/base/ui_base_features.h"

namespace autofill {
namespace password_generation {

PasswordGenerationActions::PasswordGenerationActions()
    : learn_more_visited(false),
      password_accepted(false),
      password_edited(false),
      password_regenerated(false) {
}

PasswordGenerationActions::~PasswordGenerationActions() {
}

PasswordGenerationUIData::PasswordGenerationUIData(
    const gfx::RectF& bounds,
    int max_length,
    const base::string16& generation_element,
    base::i18n::TextDirection text_direction,
    const autofill::PasswordForm& password_form)
    : bounds(bounds),
      max_length(max_length),
      generation_element(generation_element),
      text_direction(text_direction),
      password_form(password_form) {}

PasswordGenerationUIData::PasswordGenerationUIData() = default;

PasswordGenerationUIData::~PasswordGenerationUIData() = default;

void LogUserActions(PasswordGenerationActions actions) {
  UserAction action = IGNORE_FEATURE;
  if (actions.password_accepted) {
    if (actions.password_edited)
      action = ACCEPT_AFTER_EDITING;
    else
      action = ACCEPT_ORIGINAL_PASSWORD;
  } else if (actions.learn_more_visited) {
    action = LEARN_MORE;
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.UserActions",
                            action, ACTION_ENUM_COUNT);
}

void LogPasswordGenerationEvent(PasswordGenerationEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.Event",
                            event, EVENT_ENUM_COUNT);
}

bool IsPasswordGenerationEnabled() {
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutomaticPasswordGeneration))
    return true;

  if (base::FeatureList::IsEnabled(::features::kExperimentalUi))
    return true;

  return false;
}

}  // namespace password_generation
}  // namespace autofill
