// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SUGGESTION_FLOW_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SUGGESTION_FLOW_H_

#include "base/i18n/rtl.h"
#include "components/autofill/core/common/unique_ids.h"
#include "ui/gfx/geometry/rect_f.h"

namespace password_manager {

// Represents an abstraction to encapsulate suggestion generation, displaying
// and handling.
class PasswordSuggestionFlow {
 public:
  PasswordSuggestionFlow() = default;
  virtual ~PasswordSuggestionFlow() = default;
  PasswordSuggestionFlow(const PasswordSuggestionFlow&) = delete;
  PasswordSuggestionFlow& operator=(const PasswordSuggestionFlow&) = delete;

  // Invokes the flow by collecting necessary data and displaying password
  // suggestions to the user.
  virtual void RunFlow(autofill::FieldRendererId field_id,
                       const gfx::RectF& bounds,
                       base::i18n::TextDirection text_direction) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SUGGESTION_FLOW_H_
