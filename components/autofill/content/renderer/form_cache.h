// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_H_

#include <map>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillAgent;

// Manages the forms in a single RenderFrame.
class FormCache {
 public:
  struct UpdateFormCacheResult {
    UpdateFormCacheResult();
    UpdateFormCacheResult(UpdateFormCacheResult&&);
    UpdateFormCacheResult& operator=(UpdateFormCacheResult&&);
    ~UpdateFormCacheResult();

    // The updated forms are those forms that are new or are still present and
    // have changed.
    std::vector<FormData> updated_forms;

    // The forms that have been removed from the DOM.
    base::flat_set<FormRendererId> removed_forms;
  };

  explicit FormCache(AutofillAgent* owner);

  FormCache(const FormCache&) = delete;
  FormCache& operator=(const FormCache&) = delete;

  ~FormCache();

  void Reset();

  // Resets all FormData pointers in `extracted_forms_` to nullptr while keeping
  // the keys. This causes subsequent calls to `UpdateFormCache()` to treat all
  // currently present forms as updated forms.
  void ClearCache();

  // Returns the diff of forms since the last call to UpdateFormCache(): the new
  // forms, the still present but changed forms, and the removed forms.
  //
  // A form is *new* / *still present* / *removed* if if its renderer ID
  // - is      / is  / is not in the current DOM and considered interesting, and
  // - was not / was / was    in the previous DOM and considered interesting,
  // where the
  // - current DOM is the DOM at the time of UpdateFormCacheResult()
  // - previous DOM is the DOM at the time of the last UpdateFormCacheResult()
  //   call
  // - a form is interesting if it contains an editable field or an iframe
  //   (see IsFormInteresting() for details), and its fields and iframes do
  //   not exceed the limits (see below).
  //
  // A form has *changed* if it differs from the previous
  // form of the same renderer IDs according to FormData::DeepEqual().
  //
  // To reduce the computational cost, we limit the number of fields and frames
  // summed over all forms, in addition to the per-form limits in
  // form_util::FormOrFieldsetsToFormData():
  // - if the number of fields over all forms exceeds |kMaxExtractableFields|,
  //   only a subset of forms is returned which does not exceed the limit;
  // - if the number of frames over all forms exceeds |kMaxExtractableFrames|,
  //   all forms are returned but only a subset of them have non-empty
  //   FormData::child_frames.
  // In either case, the subset is chosen so that the returned list of forms
  // does not exceed the limits of fields and frames.
  //
  // Updates |extracted_forms_| to contain the forms that are currently in the
  // DOM.
  UpdateFormCacheResult UpdateFormCache(
      const FieldDataManager& field_data_manager,
      const CallTimerState& timer_state);

  // This function is labeled "unsafe" because it may only be called after
  // AutofillAgent::ExtractFormsUnthrottled has retrieved updated forms from
  // the page, or if the caller is aware that the FormData values may be
  // wiped by ClearCache(). It is not possible to distinguish a null entry
  // in the `extracted_forms_` that was created by clearing the cache from a
  // null entry that was created because the form extraction failed.
  const std::map<FormRendererId, std::unique_ptr<FormData>>&
  extracted_forms_unsafe() const {
    return extracted_forms_;
  }

 private:
  friend class FormCacheTestApi;

  // The owning AutofillAgent.
  const raw_ref<AutofillAgent> agent_;

  // The cached forms. This is used to figure out which forms changed between
  // two parsing operations so that only the changed forms are potentially
  // reparsed later and not all forms. A null form means that for a given web
  // form (either a WebFormElement or the form of unowned elements), extraction
  // failed (see `form_util::ExtractFormData()` for reasons of failing
  // extractions) OR the cache has been reset. These null pointers must be kept
  // so that UpdateFormCache() can report them as removed forms. Any user of
  // the cache must be aware that it's not possible to distinguish null entries
  // from a failed extraction and null entries from a cleared cache.
  std::map<FormRendererId, std::unique_ptr<FormData>> extracted_forms_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_H_
