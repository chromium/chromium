// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/synchronous_form_cache.h"

#include <optional>

#include "base/containers/map_util.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/types/optional_ref.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

SynchronousFormCache::SynchronousFormCache() = default;
SynchronousFormCache::SynchronousFormCache(
    const FormData& form LIFETIME_BOUND) {
  Insert(form.renderer_id(), &form);
}
SynchronousFormCache::SynchronousFormCache(
    FormRendererId form_id,
    base::optional_ref<const FormData> form) {
  Insert(form_id, form ? form.as_ptr() : nullptr);
}
SynchronousFormCache::SynchronousFormCache(
    const std::map<FormRendererId, std::unique_ptr<FormData>>& forms) {
  for (const auto& [id, form] : forms) {
    // A nullptr in the original form cache indicates that the forms need to be
    // reextracted.
    Insert(id, form.get());
  }
}

SynchronousFormCache::~SynchronousFormCache() = default;

std::optional<FormData> SynchronousFormCache::GetOrExtractForm(
    const blink::WebDocument& document,
    const blink::WebFormElement& form_element,
    const FieldDataManager& field_data_manager,
    const CallTimerState& timer_state,
    form_util::ButtonTitlesCache* button_titles_cache) const {
  const base::optional_ref<const FormData>* cache_entry =
      base::FindOrNull(cache_, form_util::GetFormRendererId(form_element));

  // If `cache_entry` is non-null, the value of the `cache_entry` may be a null
  // reference, in case the previous extraction failed. In this case, the form
  // is not extracted again, because it would fail again.
  // TODO(crbug.com/40947729): Try caching forms that were not found.
  return cache_entry ? cache_entry->CopyAsOptional()
                     : form_util::ExtractFormData(
                           document, form_element, field_data_manager,
                           timer_state, button_titles_cache);
}

void SynchronousFormCache::Insert(FormRendererId form_id,
                                  const FormData* form) {
  cache_.emplace(form_id, form);
}

}  // namespace autofill
