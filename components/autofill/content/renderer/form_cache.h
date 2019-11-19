// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_H_

#include <stddef.h>

#include <map>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/form_data.h"

namespace blink {
class WebFormControlElement;
class WebLocalFrame;
}  // namespace blink

namespace autofill {

struct FormData;
struct FormDataPredictions;

// Manages the forms in a single RenderFrame.
class FormCache {
 public:
  explicit FormCache(blink::WebLocalFrame* frame);
  ~FormCache();

  // Scans the DOM in |frame_| extracting and storing forms that have not been
  // seen before. Returns the extracted forms. Note that modified forms are
  // considered new forms.
  std::vector<FormData> ExtractNewForms();

  // Resets the forms.
  void Reset();

  // Clears the values of all input elements in the section of the form that
  // contains |element|.  Returns false if the form is not found.
  bool ClearSectionWithElement(const blink::WebFormControlElement& element);

  // For each field in the |form|, if |attach_predictions_to_dom| is true, sets
  // the title to include the field's heuristic type, server type, and
  // signature; as well as the form's signature and the experiment id for the
  // server predictions. In all cases, may emit console warnings regarding the
  // use of autocomplete attributes.
  bool ShowPredictions(const FormDataPredictions& form,
                       bool attach_predictions_to_dom);

 private:
  FRIEND_TEST_ALL_PREFIXES(FormCacheTest,
                           ShouldShowAutocompleteConsoleWarnings_Enabled);
  FRIEND_TEST_ALL_PREFIXES(FormCacheTest,
                           ShouldShowAutocompleteConsoleWarnings_Disabled);
  FRIEND_TEST_ALL_PREFIXES(FormCacheBrowserTest, FreeDataOnElementRemoval);

  // Scans |control_elements| and returns the number of editable elements.
  // Also logs warning messages for deprecated attribute if
  // |log_deprecation_messages| is set.
  size_t ScanFormControlElements(
      const std::vector<blink::WebFormControlElement>& control_elements,
      bool log_deprecation_messages);

  // Saves initial state of checkbox and select elements.
  void SaveInitialValues(
      const std::vector<blink::WebFormControlElement>& control_elements);

  // Returns whether we should show a console warning related to a wrong
  // autocomplete attribute. We will show a warning if (1) there is no
  // autocomplete attribute and we have a guess for one or (2) we recognize the
  // autocomplete attribute but it appears to be the wrong one.
  bool ShouldShowAutocompleteConsoleWarnings(
      const std::string& predicted_autocomplete,
      const std::string& actual_autocomplete);

  // Clears all entries from |initial_select_values_| and
  // |initial_checked_state_| whose keys not contained in |ids_to_retain|.
  void PruneInitialValueCaches(const std::set<uint32_t>& ids_to_retain);

  // The frame this FormCache is associated with. Weak reference.
  blink::WebLocalFrame* frame_;

  // The cached forms. Used to prevent re-extraction of forms.
  // TODO(crbug/896689) Move to std::map<unique_rederer_id, FormData>.
  std::set<FormData, FormData::IdentityComparator> parsed_forms_;

  // The synthetic FormData is for all the fieldsets in the document without a
  // form owner.
  FormData synthetic_form_;

  // The cached initial values for <select> elements. Entries are keyed by
  // unique_renderer_form_control_id of the WebSelectElements.
  std::map<uint32_t, base::string16> initial_select_values_;

  // The cached initial values for checkable <input> elements. Entries are
  // keyed by the unique_renderer_form_control_id of the WebInputElements.
  std::map<uint32_t, bool> initial_checked_state_;

  DISALLOW_COPY_AND_ASSIGN(FormCache);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_CACHE_H_
