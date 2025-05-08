// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_SYNCHRONOUS_FORM_CACHE_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_SYNCHRONOUS_FORM_CACHE_H_

#include <map>
#include <memory>

#include "base/types/optional_ref.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace blink {
class WebDocument;
class WebFormElement;
}  // namespace blink

namespace autofill {

struct CallTimerState;
class FieldDataManager;

// This class contains forms that have been previously extracted. The difference
// between this class and `FormCache` is that objects of this class are meant to
// be passed around between function in a synchronous manner so that it is
// always true that the forms they contain are up-to-date.
// This helps optimizing logic where function A and function B both need to
// extract a FormData from the same given FormElement, and A calls B
// synchronously. In that case, only A would extract the form and then B would
// use the cached version and save a redundant operation.
// When `AutofillOptimizeFormExtraction` is disabled, the cache should always be
// empty.
class SynchronousFormCache {
 public:
  SynchronousFormCache();
  SynchronousFormCache(const SynchronousFormCache&) = delete;
  SynchronousFormCache(SynchronousFormCache&&) = delete;
  // The two constructors below create a singleton cache (or cache with a single
  // form).
  explicit SynchronousFormCache(FormData& form);
  SynchronousFormCache(FormRendererId form_id,
                       base::optional_ref<const FormData> form);
  explicit SynchronousFormCache(
      const std::map<FormRendererId, std::unique_ptr<FormData>>& forms);
  ~SynchronousFormCache();

  // Tries to look for the extracted form corresponding to `form_element` in
  // `cache_` and if successful returns it, otherwise extracts the form from
  // scratch.
  std::optional<FormData> GetOrExtractForm(
      const blink::WebDocument& document,
      const blink::WebFormElement& form_element,
      const FieldDataManager& field_data_manager,
      const CallTimerState& timer_state,
      form_util::ButtonTitlesCache* button_titles_cache,
      DenseSet<form_util::ExtractOption> extract_options = {}) const;

 private:
  // Stores for a given FormRendererId the last result of trying to extract the
  // FormElement with the given ID. Note that this could be std::nullopt since
  // extraction might fail, and this would still be useful because knowing that
  // would allow avoiding a future failing attempt at extraction.
  void Insert(FormRendererId form_id, base::optional_ref<const FormData> form);

  // TODO(crbug.com/40947729): Convert to
  // base::flat_map<FormRendererId, std::unique_ptr<FormData>> for better memory
  // safety when the class stops being dependent on
  // `AutofillOptimizeFormExtraction`.
  std::map<FormRendererId, base::optional_ref<const FormData>> cache_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_SYNCHRONOUS_FORM_CACHE_H_
