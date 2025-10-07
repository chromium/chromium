// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/language_code.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class TimeTicks;
}

namespace autofill {

// The structure of forms and fields, represented by their signatures, on a
// page. These are sequence containers to reflect their order in the DOM.
using FormAndFieldSignatures =
    std::vector<std::pair<FormSignature, std::vector<FieldSignature>>>;
using FieldSuggestion = AutofillQueryResponse::FormSuggestion::FieldSuggestion;

struct AutofillServerPrediction;
class FormData;
struct FormDataPredictions;
class LogBuffer;
class LogManager;

// FormStructure stores a single HTML form together with the values entered
// in the fields along with additional information needed by Autofill.
class FormStructure {
 public:
  explicit FormStructure(const FormData& form);

  FormStructure(const FormStructure&) = delete;
  FormStructure& operator=(const FormStructure&) = delete;

  virtual ~FormStructure();

  // Runs rationalization and sectioning. This is to be run after the field
  // types change.
  //
  // For historical reasons, the order of rationalization and sectioning is
  // context-dependent: Usually, sectioning comes first. But for server
  // predictions (or `legacy_order` is true), parts of the rationalization
  // happens before sectioning.
  // TODO(crbug.com/408497919): Make the order consistent.
  void RationalizeAndAssignSections(const GeoIpCountryCode& client_country,
                                    const LanguageCode& current_page_language,
                                    LogManager* log_manager);

  // Returns predictions that can be sent to the renderer process for debugging.
  FormDataPredictions GetFieldTypePredictions() const;

  // Creates FormStructure that has bare minimum information for uploading
  // votes, namely form and field signatures. Warning: do not use for Autofill
  // code, since it is likely missing some fields.
  static std::unique_ptr<FormStructure> CreateForPasswordManagerUpload(
      FormSignature form_signature,
      const std::vector<FieldSignature>& field_signatures);

  // Return the form signature as string.
  std::string FormSignatureAsStr() const;

  // This enum defines two different states of completeness for a credit card
  // form, each used for a distinct purpose to check if the required credit card
  // fields exist.
  enum class CreditCardFormCompleteness {
    // This represents a minimal complete credit card form which has at least a
    // credit card number field and an expiration date field.
    kCompleteCreditCardForm,
    // This represents a credit card form which has a CVC field and a cardholder
    // name field in addition to the credit card number field and the expiration
    // date field. For example, this level is required for offering `Save and
    // Fill`.
    kCompleteCreditCardFormIncludingCvcAndName,
  };

  // Returns whether |this| form represents a complete Credit Card form, as
  // defined by the given CreditCardFormCompleteness level.
  bool IsCompleteCreditCardForm(
      CreditCardFormCompleteness credit_card_form_completeness) const;

  // This enum defines the behavior of RetrieveFromCache, which needs to adapt
  // to the reason for retrieving data from the cache.
  enum class RetrieveFromCacheReason {
    // kFormCacheUpdateWithoutParsing and kFormCacheUpdateAfterParsing refer to
    // the process of parsing the form and/or storing the result in
    // AutofillManager's form cache when the renderer sends a new or potentially
    // updated FormData object. Form parsing is the process of assigning field
    // types to fields when the renderer notifies the browser about a new,
    // modified or interacted with form.
    //
    // When the browser receives a FormData object from the renderer, it is
    // converted to a FormStructure object. After that, the FormStructure is
    // parsed if it changed significantly since the last parse.
    // RetrieveFromCache is responsible for retaining information from the
    // history of the fields in the form (e.g. information about previous fill
    // operations):
    //
    // - The `is_autofilled` and similar members of a field are copied from the
    //   cached form so that a field that was once labeled as autofilled remains
    //   autofilled.
    //
    // - The `value` of a field is copied from the cache as it represents the
    //   initial value of a field during page load time and must not be updated
    //   if a form is parsed a second time.
    //   TODO: crbug.com/40227496 - Update documentation about `value` when
    //   kAutofillFixValueSemantics is launched.
    //
    // - Server predictions are also preserved.
    //
    // - Heuristic predictions are preserved only for
    //   kFormCacheUpdateWithoutParsing. They are discarded for
    //   kFormCacheUpdateAfterParsing because they are generated during parsing.
    kFormCacheUpdateWithoutParsing,
    kFormCacheUpdateAfterParsing,

    // kFormImport refers to the process of importing address profiles / credit
    // cards from user-filled forms after a form submission.
    //
    // During form import, the browser receives a FormData object from the
    // renderer that is first converted to a FormStructure object.
    // RetrieveFromCache is responsible for processing the FormData so that the
    // FormStructure contains the right information that facilitate importing.
    // Therefore, similar work happens as for kFormCacheUpdateAfterParse,
    // except:
    //
    // - During form import, we want to copy field type information from
    //   previous parse operations as these tell which information to save.
    //
    // - The `value` of a FormStructure's field typically represents the
    //   initially observed value of a field during page load. So during
    //   kFormCacheUpdateAfterParse the value is persisted. During import,
    //   however, we want to store the last observed value. Furthermore, if the
    //   submitted value of a field has never been changed, we ignore the
    //   previous value from import (unless it's a state or country as websites
    //   can find meaningful default values via GeoIP).
    //   TODO: crbug.com/40227496 - Update documentation about `value` when
    //   kAutofillFixValueSemantics is launched.
    //
    // TODO: crbug.com/40227496 - When kAutofillFixValueSemantics is launched,
    // kFormImport behaves identical to kFormCacheUpdateWithoutParsing. Consider
    // renaming the enum constants or, better yet, removing the entire enum
    // then.
    kFormImport,
  };

  // Assumes that `*this` is FormStructure which was freshly created from a
  // FormData object that the renderer sent to the browser and copies relevant
  // information from a `cached_form` to `*this`. Depending on the passed
  // `reason`, a different subset of data can be copied.
  void RetrieveFromCache(const FormStructure& cached_form,
                         RetrieveFromCacheReason reason);

  // Returns the FieldGlobalIds of the |fields_| that are eligible for manual
  // filling on form interaction.
  static std::vector<FieldGlobalId> FindFieldsEligibleForManualFilling(
      const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms);

  // See FormFieldData::fields.
  const std::vector<std::unique_ptr<AutofillField>>& fields() const {
    return fields_;
  }
  const AutofillField* field(size_t index) const;
  AutofillField* field(size_t index);
  size_t field_count() const;

  const AutofillField* GetFieldById(FieldGlobalId field_id) const;
  AutofillField* GetFieldById(FieldGlobalId field_id);

  // Used for iterating over the fields.
  std::vector<std::unique_ptr<AutofillField>>::const_iterator begin() const {
    return fields_.begin();
  }

  std::vector<std::unique_ptr<AutofillField>>::const_iterator end() const {
    return fields_.end();
  }

  const std::u16string& form_name() const { return form_name_; }

  const std::u16string& id_attribute() const { return id_attribute_; }

  const std::u16string& name_attribute() const { return name_attribute_; }

  const GURL& source_url() const { return source_url_; }

  const GURL& full_source_url() const { return full_source_url_; }

  const GURL& target_url() const { return target_url_; }

  const url::Origin& main_frame_origin() const { return main_frame_origin_; }

  const ButtonTitleList& button_titles() const { return button_titles_; }

  // Returns whether the form comes from an HTML form with a <form> tag.
  bool is_form_element() const;

  base::TimeTicks form_parsed_timestamp() const {
    return form_parsed_timestamp_;
  }

  std::optional<base::TimeTicks> last_filling_timestamp() const {
    return last_filling_timestamp_;
  }
  void set_last_filling_timestamp(base::TimeTicks last_filling_timestamp) {
    last_filling_timestamp_ = last_filling_timestamp;
  }

  bool may_run_autofill_ai_model() const { return may_run_autofill_ai_model_; }

  void set_may_run_autofill_ai_model(bool may_run_autofill_ai_model) {
    may_run_autofill_ai_model_ = may_run_autofill_ai_model;
  }

  FormSignature form_signature() const { return form_signature_; }

  void set_form_signature(FormSignature signature) {
    form_signature_ = signature;
  }

  FormSignature alternative_form_signature() const {
    return alternative_form_signature_;
  }

  void set_alternative_form_signature(FormSignature signature) {
    alternative_form_signature_ = signature;
  }

  FormSignature structural_form_signature() const {
    return structural_form_signature_;
  }

  void set_structural_form_signature(FormSignature signature) {
    structural_form_signature_ = signature;
  }

  // Returns a FormData containing the data this form structure knows about.
  FormData ToFormData() const;

  // Returns the possible form types.
  DenseSet<FormType> GetFormTypes() const;

  mojom::SubmissionSource submission_source() const {
    return submission_source_;
  }
  void set_submission_source(mojom::SubmissionSource submission_source) {
    submission_source_ = submission_source;
  }

  FormGlobalId global_id() const { return {host_frame_, renderer_id_}; }

  FormVersion version() const { return version_; }

  // The signatures of forms recently submitted on the same origin within a
  // small period of time.
  struct FormAssociations {
    std::optional<FormSignature> last_address_form_submitted;
    std::optional<FormSignature> second_last_address_form_submitted;
    std::optional<FormSignature> last_credit_card_form_submitted;
  };

  base::flat_map<FieldGlobalId, AutofillServerPrediction> GetServerPredictions(
      const std::vector<FieldGlobalId>& field_ids) const;

  base::flat_map<FieldGlobalId, FieldType> GetHeuristicPredictions(
      HeuristicSource source,
      const std::vector<FieldGlobalId>& field_ids) const;

 private:
  friend class FormStructureTestApi;

  // Sets the rank of each field in the form.
  void DetermineFieldRanks();

  // Sets each field's `html_type` and `html_mode` based on the field's
  // `parsed_autocomplete` member.
  void SetFieldTypesFromAutocompleteAttribute();

  FormStructure(FormSignature form_signature,
                const std::vector<FieldSignature>& field_signatures);

  // Extracts the parseable field name by removing a common affix.
  void ExtractParseableFieldNames();

  // The id attribute of the form.
  std::u16string id_attribute_;

  // The name attribute of the form.
  std::u16string name_attribute_;

  // The name of the form.
  std::u16string form_name_;

  // The titles of form's buttons.
  ButtonTitleList button_titles_;

  // The source URL (excluding the query parameters and fragment identifiers).
  GURL source_url_;

  // The full source URL including query parameters and fragment identifiers.
  // If `kAutofillIncludeUrlInCrowdsourcing` is disabled, this value should only
  // be set for password forms.
  GURL full_source_url_;

  // The target URL.
  GURL target_url_;

  // The origin of the main frame of this form.
  // |main_frame_origin| represents the main frame (not necessarily primary
  // main frame) of the form's frame tree as described by MPArch nested frame
  // trees. For details, see RenderFrameHost::GetMainFrame().
  url::Origin main_frame_origin_;

  // A vector of all the input fields in the form.
  // See FormFieldData::fields.
  std::vector<std::unique_ptr<AutofillField>> fields_;

  // Indicates whether the client may run the AutofillAI model for this form.
  bool may_run_autofill_ai_model_ = false;

  // The unique signature for this form, composed of the target url domain,
  // the form name, and the form field names in a 64-bit hash.
  FormSignature form_signature_;

  // The alternative signature for this form which is more stable/generic than
  // `form_signature_`, used when signature is random/unstable at each reload.
  // It is a 64-bit hash of the target url domain, the fields' form control
  // types, and for forms with 1-2 fields, the first non-empty element of the
  // following: [path, reference, query].
  //
  // TODO(crbug.com/430889664): Update the comment once deprecated.
  // The alternative signature is currently only sent to server as part of Query
  // requests and is used for overrides by password manager.
  FormSignature alternative_form_signature_;

  // This form signature is equivalent to `alternative_form_signature_` for
  // forms with more than 2 fields. For forms with 2 fields or less, it is more
  // stable as doesn't depend on url path, reference, or query.
  //
  // TODO(crbug.com/427418538): Update the comment once the feature is launched.
  // This signature is currently sent to server as part of Upload requests and
  // will eventually be used together with three-bit hashes to fetch server
  // predictions. See go/autofill-stable-form-signature and
  // go/autofill-signatures-more-data for more details.
  FormSignature structural_form_signature_;

  // The timestamp (not wallclock time) when this form was initially parsed.
  base::TimeTicks form_parsed_timestamp_;

  // The timestamp when this form or one of its fields was last filled.
  std::optional<base::TimeTicks> last_filling_timestamp_;

  mojom::SubmissionSource submission_source_ = mojom::SubmissionSource::NONE;

  // True iff queries encoded from this form structure should include rich
  // form/field metadata.
  bool is_rich_query_enabled_ = false;

  // A unique identifier of the containing frame.
  // This value must not be leaked to other renderer processes.
  LocalFrameToken host_frame_;

  // A monotonically increasing counter that indicates the generation of the
  // form.
  FormVersion version_;

  // An identifier of the form that is unique among the forms from the same
  // frame.
  FormRendererId renderer_id_;

  // A vector of all iframes in the form.
  std::vector<FrameTokenWithPredecessor> child_frames_;
};

LogBuffer& operator<<(LogBuffer& buffer, const FormStructure& form);
std::ostream& operator<<(std::ostream& buffer, const FormStructure& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_
