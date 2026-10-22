// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_SYNCHRONOUS_FORM_CACHE_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_SYNCHRONOUS_FORM_CACHE_H_

#include <map>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace blink {
class WebDocument;
}  // namespace blink

namespace autofill {

struct CallTimerState;
class FieldDataManager;
class FormCache;

// This class contains forms that have been previously extracted. The difference
// between this class and `FormCache` is that objects of this class are meant to
// be passed around between function in a synchronous manner so that it is
// always true that the forms they contain are up-to-date.
// This helps optimizing logic where function A and function B both need to
// extract a FormData from the same given FormElement, and A calls B
// synchronously. In that case, only A would extract the form and then B would
// use the cached version and save a redundant operation.
class SynchronousFormCache {
 public:
  SynchronousFormCache();
  SynchronousFormCache(const SynchronousFormCache&) = delete;
  SynchronousFormCache(SynchronousFormCache&&) = delete;
  // The two constructors below create a singleton cache (or cache with a single
  // form).
  explicit SynchronousFormCache(FormData form);
  SynchronousFormCache(FormRendererId form_id, std::optional<FormData> form);
  // Creates a cache backed by `form_cache`.
  explicit SynchronousFormCache(const FormCache& form_cache);
  ~SynchronousFormCache();

  // Tries to look for the extracted form corresponding to `form_id` in
  // `cache_` / `form_cache_` and if successful returns it, otherwise extracts
  // the form from scratch.
  std::optional<FormData> GetOrExtractForm(
      const blink::WebDocument& document,
      FormRendererId form_id,
      const FieldDataManager& field_data_manager,
      const CallTimerState& timer_state,
      form_util::ButtonTitlesCache* button_titles_cache) const;

  // Clears all cached forms. Must be called when the DOM may have been mutated
  // (e.g., via `WebFormControlElement::SetAutofillValue()` dispatching
  // synchronous JS events) while the cache is live.
  void Invalidate();

 private:
  // Stores for a given FormRendererId the last result of trying to extract the
  // FormElement with the given ID. Note that this could be std::nullopt since
  // extraction might fail, and this would still be useful because knowing that
  // would allow avoiding a future failing attempt at extraction.
  std::map<FormRendererId, std::optional<FormData>> cache_;

  base::WeakPtr<const FormCache> form_cache_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_SYNCHRONOUS_FORM_CACHE_H_
