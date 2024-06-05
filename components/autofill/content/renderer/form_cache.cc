// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_cache.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/page_form_analyser_logger.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/metrics/form_element_pii_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_select_element.h"
#include "ui/base/l10n/l10n_util.h"

using blink::WebAutofillState;
using blink::WebConsoleMessage;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {

using form_util::ExtractOption;

namespace {

blink::FormElementPiiType MapTypePredictionToFormElementPiiType(
    std::string_view type) {
  if (type == "NO_SERVER_DATA" || type == "UNKNOWN_TYPE" ||
      type == "EMPTY_TYPE" || type == "") {
    return blink::FormElementPiiType::kUnknown;
  }

  if (type.starts_with("EMAIL_")) {
    return blink::FormElementPiiType::kEmail;
  }
  if (type.starts_with("PHONE_")) {
    return blink::FormElementPiiType::kPhone;
  }
  return blink::FormElementPiiType::kOthers;
}

// Determines whether the form is interesting enough to be sent to the browser
// for further operations. This is the case if any of the below holds:
// (1) At least one form field is not-checkable. (See crbug.com/1489075.)
// (2) At least one field has a non-empty autocomplete attribute.
// (3) There is at least one iframe.
// TODO(crbug.com/40283901): Should an element that IsCheckableElement() also be
// IsAutofillableInputElement()?
bool IsFormInteresting(const FormData& form) {
  return !form.child_frames().empty() ||
         base::ranges::any_of(form.fields, std::not_fn(&form_util::IsCheckable),
                              &FormFieldData::form_control_type) ||
         base::ranges::any_of(form.fields, std::not_fn(&std::string::empty),
                              &FormFieldData::autocomplete_attribute);
}

std::string GetButtonTitlesString(const ButtonTitleList& titles_list) {
  std::vector<std::string> titles;
  titles.reserve(titles_list.size());
  std::transform(
      titles_list.cbegin(), titles_list.cend(), std::back_inserter(titles),
      [](const auto& list_item) { return base::UTF16ToUTF8(list_item.first); });
  return base::JoinString(titles, ",");
}

}  // namespace

FormCache::UpdateFormCacheResult::UpdateFormCacheResult() = default;
FormCache::UpdateFormCacheResult::UpdateFormCacheResult(
    UpdateFormCacheResult&&) = default;
FormCache::UpdateFormCacheResult& FormCache::UpdateFormCacheResult::operator=(
    UpdateFormCacheResult&&) = default;
FormCache::UpdateFormCacheResult::~UpdateFormCacheResult() = default;

FormCache::FormCache(WebLocalFrame* frame) : frame_(frame) {}
FormCache::~FormCache() = default;

FormCache::UpdateFormCacheResult FormCache::UpdateFormCache(
    const FieldDataManager& field_data_manager) {
  std::set<FieldRendererId> observed_renderer_ids;

  // |extracted_forms_| is re-populated below in ProcessForm().
  std::map<FormRendererId, FormData> old_extracted_forms =
      std::move(extracted_forms_);
  extracted_forms_.clear();

  UpdateFormCacheResult r;
  r.removed_forms = base::MakeFlatSet<FormRendererId>(
      old_extracted_forms, {},
      &std::pair<const FormRendererId, FormData>::first);

  size_t num_fields_seen = 0;
  size_t num_frames_seen = 0;

  // Helper function that stores new autofillable forms in |forms|. Returns
  // false iff the total number of fields exceeds |kMaxExtractableFields|.
  // Clears |form|'s FormData::child_frames if the total number of frames
  // exceeds |kMaxExtractableChildFrames|.
  auto ProcessForm = [&](FormData form) {
    for (const auto& field : form.fields) {
      observed_renderer_ids.insert(field.renderer_id());
    }

    num_fields_seen += form.fields.size();
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
      DCHECK(extracted_forms_.find(form_id) == extracted_forms_.end());
      auto it = old_extracted_forms.find(form_id);
      if (it == old_extracted_forms.end() ||
          !FormData::DeepEqual(std::move(it->second), form)) {
        r.updated_forms.push_back(form);
      }
      r.removed_forms.erase(form_id);
      extracted_forms_[form_id] = std::move(form);
    }
    return true;
  };

  constexpr DenseSet<ExtractOption> extract_options = {ExtractOption::kValue,
                                                       ExtractOption::kOptions};

  WebDocument document = frame_->GetDocument();
  if (document.IsNull())
    return r;

  for (const WebFormElement& form_element :
       base::FeatureList::IsEnabled(
           blink::features::kAutofillIncludeFormElementsInShadowDom)
           ? document.GetTopLevelForms()
           : document.Forms()) {
    if (std::optional<FormData> form = ExtractFormData(
            document, form_element, field_data_manager, extract_options)) {
      if (!ProcessForm(std::move(*form))) {
        return r;
      }
    }
  }

  // Look for more extractable fields outside of forms. Create a synthetic form
  // from them.
  std::optional<FormData> synthetic_form = ExtractFormData(
      document, WebFormElement(), field_data_manager, extract_options);
  if (synthetic_form) {
    ProcessForm(std::move(*synthetic_form));
  }
  return r;
}

bool FormCache::ShowPredictions(const FormDataPredictions& form,
                                bool attach_predictions_to_dom) {
  DCHECK_EQ(form.data.fields.size(), form.fields.size());

  WebDocument document = frame_->GetDocument();
  WebFormElement form_element =
      form_util::GetFormByRendererId(form.data.renderer_id());
  std::vector<WebFormControlElement> control_elements =
      form_util::GetAutofillableFormControlElements(document, form_element);
  if (control_elements.size() != form.fields.size()) {
    // Keep things simple.  Don't show predictions for forms that were modified
    // between page load and the server's response to our query.
    return false;
  }

  PageFormAnalyserLogger logger(frame_);
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement& element = control_elements[i];

    const FormFieldData& field_data = form.data.fields[i];
    if (form_util::GetFieldRendererId(element) != field_data.renderer_id()) {
      continue;
    }
    const FormFieldDataPredictions& field = form.fields[i];

    element.SetFormElementPiiType(
        MapTypePredictionToFormElementPiiType(field.overall_type));

    // If the flag is enabled, attach the prediction to the field.
    if (attach_predictions_to_dom) {
      constexpr size_t kMaxLabelSize = 100;
      // TODO(crbug.com/40741721): Use `parseable_label()` once the feature is
      // launched.
      std::u16string truncated_label =
          field_data.label().substr(0, kMaxLabelSize);
      // The label may be derived from the placeholder attribute and may contain
      // line wraps which are normalized here.
      base::ReplaceChars(truncated_label, u"\n", u"|", &truncated_label);

      std::string form_id =
          base::NumberToString(form.data.renderer_id().value());
      std::string field_id_str =
          base::NumberToString(field_data.renderer_id().value());

      blink::LocalFrameToken frame_token;
      if (auto* frame = element.GetDocument().GetFrame())
        frame_token = frame->GetLocalFrameToken();

      std::string title = base::StrCat({
          "overall type: ",
          field.overall_type,
          "\nhtml type: ",
          field.html_type,
          "\nserver type: ",
          field.server_type.has_value() ? field.server_type.value()
                                        : "SERVER_RESPONSE_PENDING",
          "\nheuristic type: ",
          field.heuristic_type,
          "\nlabel: ",
          base::UTF16ToUTF8(truncated_label),
          "\nparseable name: ",
          field.parseable_name,
          "\nsection: ",
          field.section,
          "\nfield signature: ",
          field.signature,
          "\nform signature: ",
          form.signature,
          "\nform signature in host form: ",
          field.host_form_signature,
          "\nalternative form signature: ",
          form.alternative_signature,
          "\nform name: ",
          base::UTF16ToUTF8(form.data.name_attribute()),
          "\nform id: ",
          base::UTF16ToUTF8(form.data.id_attribute()),
          "\nform button titles: ",
          GetButtonTitlesString(form_util::GetButtonTitles(
              form_element, /*button_titles_cache=*/nullptr)),
          "\nfield frame token: ",
          frame_token.ToString(),
          "\nform renderer id: ",
          form_id,
          "\nfield renderer id: ",
          field_id_str,
          "\nvisible: ",
          field_data.is_visible() ? "true" : "false",
          "\nfocusable: ",
          field_data.IsFocusable() ? "true" : "false",
          "\nfield rank: ",
          base::NumberToString(field.rank),
          "\nfield rank in signature group: ",
          base::NumberToString(field.rank_in_signature_group),
          "\nfield rank in host form: ",
          base::NumberToString(field.rank_in_host_form),
          "\nfield rank in host form signature group: ",
          base::NumberToString(field.rank_in_host_form_signature_group),
      });

      WebString kAutocomplete = WebString::FromASCII("autocomplete");
      if (element.HasAttribute(kAutocomplete)) {
        title += "\nautocomplete: " +
                 element.GetAttribute(kAutocomplete).Utf8().substr(0, 100);
      }

      // Set the same debug string to an attribute that does not get mangled if
      // Google Translate is triggered for the site. This is useful for
      // automated processing of the data.
      element.SetAttribute("autofill-information", WebString::FromUTF8(title));

      //  If the field has password manager's annotation, add it as well.
      if (element.HasAttribute("pm_parser_annotation")) {
        title =
            base::StrCat({title, "\npm_parser_annotation: ",
                          element.GetAttribute("pm_parser_annotation").Utf8()});
      }

      // Set this debug string to the title so that a developer can easily debug
      // by hovering the mouse over the input field.
      element.SetAttribute("title", WebString::FromUTF8(title));

      element.SetAttribute("autofill-prediction",
                           WebString::FromUTF8(field.overall_type));
    }
  }
  logger.Flush();

  return true;
}

}  // namespace autofill
