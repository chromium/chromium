// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/synchronous_form_cache.h"

#include <optional>
#include <utility>

#include "base/containers/map_util.h"
#include "base/types/pass_key.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

SynchronousFormCache::SynchronousFormCache() = default;
SynchronousFormCache::SynchronousFormCache(FormData form) {
  FormRendererId form_id = form.renderer_id();
  cache_.emplace(form_id, std::move(form));
}
SynchronousFormCache::SynchronousFormCache(FormRendererId form_id,
                                           std::optional<FormData> form) {
  cache_.emplace(form_id, std::move(form));
}
SynchronousFormCache::SynchronousFormCache(const FormCache& form_cache)
    : form_cache_(form_cache.GetWeakPtr({})) {}

SynchronousFormCache::~SynchronousFormCache() = default;

std::optional<FormData> SynchronousFormCache::GetOrExtractForm(
    const blink::WebDocument& document,
    FormRendererId form_id,
    const FieldDataManager& field_data_manager,
    const CallTimerState& timer_state,
    form_util::ButtonTitlesCache* button_titles_cache) const {
  // If `cached_form` is non-null, its value may be `std::nullopt` in case the
  // previous extraction failed. In this case, the form is not extracted again,
  // because it would fail again.
  // TODO(crbug.com/40947729): Try caching forms that were not found.
  if (const std::optional<FormData>* cached_form =
          base::FindOrNull(cache_, form_id)) {
    return *cached_form;
  }
  if (form_cache_) {
    if (const std::unique_ptr<FormData>* cached_form = base::FindOrNull(
            form_cache_->extracted_forms_unsafe(/*pass_key=*/{}), form_id);
        cached_form && *cached_form) {
      return **cached_form;
    }
  }
  return form_util::ExtractFormData(document, form_id, field_data_manager,
                                    timer_state, button_titles_cache);
}

void SynchronousFormCache::Invalidate() {
  cache_.clear();
  form_cache_.reset();
}

}  // namespace autofill
