// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_generation_util.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {
namespace password_generation {

PasswordGenerationUIData::PasswordGenerationUIData(
    const gfx::RectF& bounds,
    int max_length,
    const base::string16& generation_element,
    uint32_t generation_element_id,
    bool is_generation_element_password_type,
    base::i18n::TextDirection text_direction,
    const autofill::PasswordForm& password_form)
    : bounds(bounds),
      max_length(max_length),
      generation_element(generation_element),
      generation_element_id(generation_element_id),
      is_generation_element_password_type(is_generation_element_password_type),
      text_direction(text_direction),
      password_form(password_form) {}

PasswordGenerationUIData::PasswordGenerationUIData() = default;
PasswordGenerationUIData::PasswordGenerationUIData(
    const PasswordGenerationUIData& rhs) = default;
PasswordGenerationUIData::PasswordGenerationUIData(
    PasswordGenerationUIData&& rhs) = default;

PasswordGenerationUIData& PasswordGenerationUIData::operator=(
    const PasswordGenerationUIData& rhs) = default;
PasswordGenerationUIData& PasswordGenerationUIData::operator=(
    PasswordGenerationUIData&& rhs) = default;

PasswordGenerationUIData::~PasswordGenerationUIData() = default;

void LogPasswordGenerationEvent(PasswordGenerationEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.Event",
                            event, EVENT_ENUM_COUNT);
}

}  // namespace password_generation
}  // namespace autofill
