// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_TEST_API_H_

#include <stddef.h>

#include "base/containers/contains.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "third_party/blink/public/web/web_form_control_element.h"

namespace autofill {

// Exposes some testing operations for FormCache.
class FormCacheTestApi {
 public:
  explicit FormCacheTestApi(FormCache* form_cache) : form_cache_(form_cache) {
    DCHECK(form_cache_);
  }

  // For a given |control_element| check whether it is eligible for manual
  // filling on form interaction.
  bool IsFormElementEligibleForManualFilling(
      const blink::WebFormControlElement& control_element) {
    return base::Contains(
        form_cache_->fields_eligible_for_manual_filling_,
        FieldRendererId(control_element.UniqueRendererFormControlId()));
  }

  size_t initial_select_values_size() {
    return form_cache_->initial_select_values_.size();
  }

  size_t initial_selectmenu_values_size() {
    return form_cache_->initial_selectmenu_values_.size();
  }

  size_t initial_checked_state_size() {
    return form_cache_->initial_checked_state_.size();
  }

  size_t parsed_forms_size() { return form_cache_->parsed_forms_.size(); }

 private:
  FormCache* form_cache_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_TEST_API_H_
