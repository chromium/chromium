// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_cache.h"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"

namespace autofill {

namespace {

// Determines whether the form is interesting enough to be sent to the browser
// for further operations. This is the case if any of the below holds:
// (1) At least one form field is not-checkable. (See crbug.com/1489075.)
// (2) At least one field has a non-empty autocomplete attribute.
// (3) There is at least one iframe.
// TODO(crbug.com/40283901): Remove check for radio buttons and checkboxes when
// we they're not extracted anymore.
bool IsFormInteresting(const FormData& form) {
  auto is_checkable = [](FormControlType type) {
    return type == FormControlType::kInputCheckbox ||
           type == FormControlType::kInputRadio;
  };
  return !form.child_frames().empty() ||
         std::ranges::any_of(form.fields(), std::not_fn(is_checkable),
                             &FormFieldData::form_control_type) ||
         std::ranges::any_of(form.fields(), std::not_fn(&std::string::empty),
                             &FormFieldData::autocomplete_attribute);
}

}  // namespace

FormCache::UpdateFormCacheResult::UpdateFormCacheResult() = default;
FormCache::UpdateFormCacheResult::UpdateFormCacheResult(
    UpdateFormCacheResult&&) = default;
FormCache::UpdateFormCacheResult& FormCache::UpdateFormCacheResult::operator=(
    UpdateFormCacheResult&&) = default;
FormCache::UpdateFormCacheResult::~UpdateFormCacheResult() = default;

FormCache::FormCache(AutofillAgent* owner) : agent_(CHECK_DEREF(owner)) {}
FormCache::~FormCache() = default;

void FormCache::Reset() {
  extracted_forms_.clear();
}

FormCache::UpdateFormCacheResult FormCache::UpdateFormCache(
    const FieldDataManager& field_data_manager,
    const CallTimerState& timer_state) {
  constexpr auto kUpdateFormCache = CallTimerState::CallSite::kUpdateFormCache;
  ScopedCallTimer timer("UpdateFormCache", timer_state);

  // |extracted_forms_| is re-populated below in ProcessForm().
  std::map<FormRendererId, std::unique_ptr<FormData>> old_extracted_forms =
      std::move(extracted_forms_);
  extracted_forms_.clear();

  UpdateFormCacheResult r;
  r.removed_forms = base::MakeFlatSet<FormRendererId>(
      old_extracted_forms, {},
      &std::pair<const FormRendererId, std::unique_ptr<FormData>>::first);

  for (const auto& [id, form] : old_extracted_forms) {
    if (!form) {
      r.removed_forms.erase(id);
    }
  }

  size_t num_fields_seen = 0;
  size_t num_frames_seen = 0;

  // Helper function that stores new autofillable forms in |forms|. Returns
  // false iff the total number of fields exceeds |kMaxExtractableFields|.
  // Clears |form|'s FormData::child_frames if the total number of frames
  // exceeds |kMaxExtractableChildFrames|.
  auto ProcessForm = [&](FormData form) {
    num_fields_seen += form.fields().size();
    num_frames_seen += form.child_frames().size();

    // Enforce the kMaxExtractableFields limit: ignore all forms after this
    // limit has been reached (i.e., abort parsing).
    if (num_fields_seen > kMaxExtractableFields) {
      return false;
    }

    // Enforce the kMaxExtractableChildFrames limit: ignore the iframes, but
    // do not ignore the fields (i.e., continue parsing).
    if (num_frames_seen > kMaxExtractableChildFrames) {
      form.set_child_frames({});
    }

    // Store only forms that contain iframes or fields.
    if (IsFormInteresting(form)) {
      FormRendererId form_id = form.renderer_id();
      auto it = old_extracted_forms.find(form_id);
      if (it == old_extracted_forms.end() || !it->second ||
          !FormData::IdenticalAndEquivalentDomElements(
              *it->second, form, {FormFieldData::Exclusion::kValue})) {
        r.updated_forms.push_back(form);
      }
      r.removed_forms.erase(form_id);
      extracted_forms_[form_id] = std::make_unique<FormData>(std::move(form));
    }
    return true;
  };

  blink::WebDocument document = agent_->GetDocument();
  if (!document) {
    return r;
  }
  std::vector<blink::WebFormElement> form_elements =
      document.GetTopLevelForms();
  // Add a null WebFormElement to account for the form of unowned elements.
  form_elements.emplace_back();

  bool stop_extracting_forms = false;
  for (const blink::WebFormElement& form_element : form_elements) {
    extracted_forms_[form_util::GetFormRendererId(form_element)] = nullptr;
    if (stop_extracting_forms) {
      continue;
    }
    if (std::optional<FormData> form = form_util::ExtractFormData(
            document, form_element, field_data_manager,
            agent_->GetCallTimerState(kUpdateFormCache),
            agent_->button_titles_cache())) {
      if (!ProcessForm(std::move(*form))) {
        stop_extracting_forms = true;
      }
    }
  }
  return r;
}

}  // namespace autofill
