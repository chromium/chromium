// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/synchronous_form_cache.h"

#include <optional>

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
  if (!cache_.empty()) {
    if (FormRendererId form_id = form_util::GetFormRendererId(form_element);
        cache_.contains(form_id)) {
      auto it = cache_.find(form_id);
      // Even if the cache returns a null form, we do not try to extract because
      // this means extraction happened synchronously before and failed, meaning
      // that it would fail again if we do it now.
      return it->second ? std::optional(*it->second) : std::nullopt;
    }
#if !BUILDFLAG(IS_ANDROID)
    // This codepath should not be reached, as it would mean that we populated
    // the cache with wrong forms before passing it around methods. We do not
    // crash the renderer because this wouldn't break anything since we can
    // always re-extract.
    // TODO(crbug.com/40947729): This is currently disabled on Android because
    // of pending work in order to minimize renderer logic at suggestion time.
    // Add this back when the work is done.
    base::debug::DumpWithoutCrashing(FROM_HERE);
#endif  // !BUILDFLAG(IS_ANDROID)
  }
  return form_util::ExtractFormData(document, form_element, field_data_manager,
                                    timer_state, button_titles_cache);
}

void SynchronousFormCache::Insert(FormRendererId form_id,
                                  const FormData* form) {
  cache_.emplace(form_id, form);
}

}  // namespace autofill
