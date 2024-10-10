// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_

#include <stddef.h>

#include <deque>
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
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/language_code.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class TimeTicks;
}

namespace autofill {

class LogBuffer;
class LogManager;
struct ParsingContext;

// The structure of forms and fields, represented by their signatures, on a
// page. These are sequence containers to reflect their order in the DOM.
using FormAndFieldSignatures =
    std::vector<std::pair<FormSignature, std::vector<FieldSignature>>>;
using FieldSuggestion = AutofillQueryResponse::FormSuggestion::FieldSuggestion;

class FormData;
struct FormDataPredictions;

class RandomizedEncoder;

// FormStructure stores a single HTML form together with the values entered
// in the fields along with additional information needed by Autofill.
class FormStructure {
 public:
  explicit FormStructure(const FormData& form);

  FormStructure(const FormStructure&) = delete;
  FormStructure& operator=(const FormStructure&) = delete;

  virtual ~FormStructure();

  // Runs several heuristics against the form fields to determine their possible
  // types.
  void DetermineHeuristicTypes(
      const GeoIpCountryCode& client_country,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      LogManager* log_manager);

  // Returns predictions using the details from the given |form_structures| and
  // their fields' predicted types.
  static std::vector<FormDataPredictions> GetFieldTypePredictions(
      const std::vector<raw_ptr<FormStructure, VectorExperimental>>&
          form_structures);

  // Creates FormStructure that has bare minimum information for uploading
  // votes, namely form and field signatures. Warning: do not use for Autofill
  // code, since it is likely missing some fields.
  static std::unique_ptr<FormStructure> CreateForPasswordManagerUpload(
      FormSignature form_signature,
      const std::vector<FieldSignature>& field_signatures);

  // Return the form signature as string.
  std::string FormSignatureAsStr() const;

  // Runs a quick heuristic to rule out forms that are obviously not
  // auto-fillable, like google/yahoo/msn search, etc.
  bool IsAutofillable() const;

  // Returns whether |this| form represents a complete Credit Card form, which
  // consists in having at least a credit card number field and an expiration
  // field.
  bool IsCompleteCreditCardForm() const;

  // Resets |autofill_count_| and counts the number of auto-fillable fields.
  // This is used when we receive server data for form fields.  At that time,
  // we may have more known fields than just the number of fields we matched
  // heuristically.
  void UpdateAutofillCount();

  // Returns true if this form matches the structural requirements for Autofill.
  [[nodiscard]] bool ShouldBeParsed(LogManager* log_manager = nullptr) const {
    return ShouldBeParsed({}, log_manager);
  }

  // Returns true if heuristic autofill type detection should be attempted for
  // this form.
  bool ShouldRunHeuristics() const;

  // Returns true if autofill's heuristic field type detection should be
  // attempted for this form given that |kMinRequiredFieldsForHeuristics| is not
  // met.
  bool ShouldRunHeuristicsForSingleFieldForms() const;

  // Returns true if we should query the crowd-sourcing server to determine this
  // form's field types. If the form includes author-specified types, this will
  // return false unless there are password fields in the form. If there are no
  // password fields the assumption is that the author has expressed their
  // intent and crowdsourced data should not be used to override this. Password
  // fields are different because there is no way to specify password generation
  // directly.
  bool ShouldBeQueried() const;

  // Returns true if we should upload Autofill votes for this form to the
  // crowd-sourcing server. It is not applied for Password Manager votes.
  bool ShouldBeUploaded() const;

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
    // renaming the
    // enum constants or, better yet, removing the entire enum then.
    kFormImport,
  };

  // Assumes that `*this` is FormStructure which was freshly created from a
  // FormData object that the renderer sent to the browser and copies relevant
  // information from a `cached_form` to `*this`. Depending on the passed
  // `reason`, a different subset of data can be copied.
  void RetrieveFromCache(const FormStructure& cached_form,
                         RetrieveFromCacheReason reason);

  void LogDetermineHeuristicTypesMetrics();

  // Sets each field's `html_type` and `html_mode` based on the field's
  // `parsed_autocomplete` member.
  // Sets `has_author_specified_types_` to `true` iff the `parsed_autocomplete`
  // is available for at least one field.
  void SetFieldTypesFromAutocompleteAttribute();

  // Resets each field's section and sets it based on the `parsed_autocomplete`
  // member when available.
  // Returns whether at least one field's `parsed_autocomplete` section is
  // correctly defined by the web developer.
  bool SetSectionsFromAutocompleteOrReset();

  // Returns the values that can be filled into the form structure for the
  // given type. For example, there's no way to fill in a value of "The Moon"
  // into ADDRESS_HOME_STATE if the form only has a
  // <select autocomplete="region"> with no "The Moon" option. Returns an
  // empty set if the form doesn't reference the given type or if all inputs
  // are accepted (e.g., <input type="text" autocomplete="region">).
  // All returned values are standardized to upper case.
  std::set<std::u16string> PossibleValues(FieldType type);

  // Rationalize phone number fields in a given section, that is only fill
  // the fields that are considered composing a first complete phone number.
  void RationalizePhoneNumbersInSection(const Section& section);

  // Rationalize the form's autocomplete attributes, repeated fields and field
  // type predictions.
  void RationalizeFormStructure(
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      LogManager* log_manager);

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

  void AddSingleUsernameData(
      AutofillUploadContents::SingleUsernameData single_username_data) {
    single_username_data_.push_back(single_username_data);
  }

  // Returns the number of fields that are part of the form signature and that
  // are included in queries to the Autofill server.
  size_t active_field_count() const;

  // Returns the number of fields that are able to be autofilled.
  size_t autofill_count() const { return autofill_count_; }

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

  bool has_author_specified_types() const {
    return has_author_specified_types_;
  }

  bool has_password_field() const { return has_password_field_; }

  // Returns whether the form comes from an HTML form with a <form> tag.
  bool is_form_element() const;

  void set_submission_event(mojom::SubmissionIndicatorEvent submission_event) {
    submission_event_ = submission_event;
  }

  mojom::SubmissionIndicatorEvent submission_event() const {
    return submission_event_;
  }

  base::TimeTicks form_parsed_timestamp() const {
    return form_parsed_timestamp_;
  }

  std::optional<base::TimeTicks> last_filling_timestamp() const {
    return last_filling_timestamp_;
  }
  void set_last_filling_timestamp(base::TimeTicks last_filling_timestamp) {
    last_filling_timestamp_ = last_filling_timestamp;
  }

  bool all_fields_are_passwords() const { return all_fields_are_passwords_; }

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

  int developer_engagement_metrics() const {
    return developer_engagement_metrics_;
  }

  void set_randomized_encoder(std::unique_ptr<RandomizedEncoder> encoder);

  base::optional_ref<const RandomizedEncoder> randomized_encoder() const {
    if (randomized_encoder_) {
      return randomized_encoder_.get();
    }
    return std::nullopt;
  }

  const LanguageCode& current_page_language() const {
    return current_page_language_;
  }

  void set_current_page_language(LanguageCode language) {
    current_page_language_ = std::move(language);
  }

  FormGlobalId global_id() const { return {host_frame_, renderer_id_}; }

  FormVersion version() const { return version_; }

  const GeoIpCountryCode& client_country() const { return client_country_; }

  std::vector<AutofillUploadContents::SingleUsernameData> single_username_data()
      const {
    return single_username_data_;
  }

  // The signatures of forms recently submitted on the same origin within a
  // small period of time.
  struct FormAssociations {
    std::optional<FormSignature> last_address_form_submitted;
    std::optional<FormSignature> second_last_address_form_submitted;
    std::optional<FormSignature> last_credit_card_form_submitted;
  };

  void set_form_associations(FormAssociations associations) {
    form_associations_ = associations;
  }

  FormAssociations form_associations() const { return form_associations_; }

 private:
  friend class FormStructureTestApi;

  // Sets the rank of each field in the form.
  void DetermineFieldRanks();

  // Considers all `GetNonActiveHeuristicSources()` and computes predictions
  // for the PatternSources among them. If some of them match the
  // `active_predictions`, applying the regexes is skipped entirely.
  // `active_predictions` is nullopt if the active HeuristicSource is not a
  // PatternSource.
  // Reuses the `context` used to compute the main predictions for caching.
  void DetermineNonActiveHeuristicTypes(
      std::optional<FieldCandidatesMap> active_predictions,
      ParsingContext& context);

  // Classifies each field using the regular expressions. The classifications
  // are returned, but not assigned to the `fields_` yet. Use
  // `AssignBestFieldTypes()` to do so.
  [[nodiscard]] FieldCandidatesMap ParseFieldTypesWithPatterns(
      ParsingContext& context) const;

  // Assigns the best heuristic types from the `field_type_map` to the heuristic
  // types of the corresponding fields for the `pattern_source`.
  void AssignBestFieldTypes(const FieldCandidatesMap& field_type_map,
                            HeuristicSource heuristic_source);

  // Production code only uses the default parameters.
  // Unit tests also test other parameters.
  struct ShouldBeParsedParams {
    size_t min_required_fields =
        std::min({kMinRequiredFieldsForHeuristics, kMinRequiredFieldsForQuery,
                  kMinRequiredFieldsForUpload});
    size_t required_fields_for_forms_with_only_password_fields =
        kRequiredFieldsForFormsWithOnlyPasswordFields;
  };

  FormStructure(FormSignature form_signature,
                const std::vector<FieldSignature>& field_signatures);

  [[nodiscard]] bool ShouldBeParsed(ShouldBeParsedParams params,
                                    LogManager* log_manager = nullptr) const;

  // Further processes the extracted |fields_|.
  void ProcessExtractedFields();

  // Extracts the parseable field name by removing a common affix.
  void ExtractParseableFieldNames();

  // Extract parseable field labels by potentially splitting labels between
  // adjacent fields.
  void ExtractParseableFieldLabels();

  // The country where the user is currently located. Used to introduce biases
  // in form parsing and understanding according to the user's location.
  GeoIpCountryCode client_country_;

  // The language detected for this form's page, before any translations
  // performed by Chrome.
  LanguageCode current_page_language_;

  // The id attribute of the form.
  std::u16string id_attribute_;

  // The name attribute of the form.
  std::u16string name_attribute_;

  // The name of the form.
  std::u16string form_name_;

  // The titles of form's buttons.
  ButtonTitleList button_titles_;

  // The type of the event that was taken as an indication that the form has
  // been successfully submitted.
  mojom::SubmissionIndicatorEvent submission_event_ =
      mojom::SubmissionIndicatorEvent::NONE;

  // The source URL (excluding the query parameters and fragment identifiers).
  GURL source_url_;

  // The full source URL including query parameters and fragment identifiers.
  // This value should be set only for password forms.
  GURL full_source_url_;

  // The target URL.
  GURL target_url_;

  // The origin of the main frame of this form.
  // |main_frame_origin| represents the main frame (not necessarily primary
  // main frame) of the form's frame tree as described by MPArch nested frame
  // trees. For details, see RenderFrameHost::GetMainFrame().
  url::Origin main_frame_origin_;

  // The number of fields able to be auto-filled.
  size_t autofill_count_ = 0;

  // A vector of all the input fields in the form.
  // See FormFieldData::fields.
  std::vector<std::unique_ptr<AutofillField>> fields_;

  // The number of fields that are part of the form signature and that are
  // included in queries to the Autofill server.
  size_t active_field_count_ = 0;

  // Whether the form includes any field types explicitly specified by the site
  // author, via the |autocompletetype| attribute.
  bool has_author_specified_types_ = false;

  // True if the form contains at least one password field.
  bool has_password_field_ = false;

  // True if all form fields are password fields.
  bool all_fields_are_passwords_ = false;

  // The unique signature for this form, composed of the target url domain,
  // the form name, and the form field names in a 64-bit hash.
  FormSignature form_signature_;

  // The alternative signature for this form which is more stable/generic than
  // `form_signature_`, used when signature is random/unstable at each reload.
  // It is composed of the target url domain, the fields' form control types,
  // and for forms with 1-2 fields, one of the following non-empty elements
  // ordered by preference: path, reference, or query in a 64-bit hash.
  FormSignature alternative_form_signature_;

  // The timestamp (not wallclock time) when this form was initially parsed.
  base::TimeTicks form_parsed_timestamp_;

  // The timestamp when this form or one of its fields was last filled.
  std::optional<base::TimeTicks> last_filling_timestamp_;

  // If phone number rationalization has been performed for a given section.
  std::set<Section> phone_rationalized_;

  // Used to record whether developer has used autocomplete markup or
  // UPI-VPA hints, This is a bitmask of DeveloperEngagementMetric and set in
  // DetermineHeuristicTypes().
  int developer_engagement_metrics_ = 0;

  mojom::SubmissionSource submission_source_ = mojom::SubmissionSource::NONE;

  // The randomized encoder to use to encode form metadata during upload.
  // If this is nullptr, no randomized metadata will be sent.
  std::unique_ptr<RandomizedEncoder> randomized_encoder_;

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

  // Single username details, if applicable.
  std::vector<AutofillUploadContents::SingleUsernameData> single_username_data_;

  // The signatures of forms recently submitted on the same origin within a
  // small period of time.
  // Only used for voting-purposes.
  FormAssociations form_associations_;
};

LogBuffer& operator<<(LogBuffer& buffer, const FormStructure& form);
std::ostream& operator<<(std::ostream& buffer, const FormStructure& form);

// Helper struct for `GetFormDataAndServerPredictions`.
struct FormDataAndServerPredictions {
  FormDataAndServerPredictions();
  FormDataAndServerPredictions(const FormDataAndServerPredictions&);
  FormDataAndServerPredictions& operator=(const FormDataAndServerPredictions&);
  FormDataAndServerPredictions(FormDataAndServerPredictions&&);
  FormDataAndServerPredictions& operator=(FormDataAndServerPredictions&&);
  ~FormDataAndServerPredictions();

  FormData form_data;
  base::flat_map<FieldGlobalId, AutofillType::ServerPrediction> predictions;
};

// Returns the `FormData` and `ServerPrediction` objects underlying `form`.
FormDataAndServerPredictions GetFormDataAndServerPredictions(
    const FormStructure& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_
