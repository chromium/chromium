// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_TEST_API_H_

#include <map>
#include <memory>

#include "base/memory/raw_ref.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// Exposes private members of FormCache for testing.
class FormCacheTestApi {
 public:
  explicit FormCacheTestApi(const FormCache* form_cache)
      : form_cache_(*form_cache) {}

  const std::map<FormRendererId, std::unique_ptr<FormData>>& extracted_forms()
      const {
    return form_cache_->extracted_forms_;
  }

 private:
  const raw_ref<const FormCache> form_cache_;
};

inline FormCacheTestApi test_api(const FormCache& form_cache) {
  return FormCacheTestApi(&form_cache);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_TEST_API_H_
