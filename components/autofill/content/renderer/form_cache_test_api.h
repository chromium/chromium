// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_TEST_API_H_

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "third_party/blink/public/web/web_form_control_element.h"

namespace autofill {

// Exposes some testing operations for FormCache.
class FormCacheTestApi {
 public:
  explicit FormCacheTestApi(FormCache* form_cache) : form_cache_(*form_cache) {}

  size_t num_extracted_forms() const {
    return form_cache_->extracted_forms_.size();
  }

 private:
  const raw_ref<FormCache> form_cache_;
};

inline FormCacheTestApi test_api(FormCache& form_cache) {
  return FormCacheTestApi(&form_cache);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_TEST_API_H_
