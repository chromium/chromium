// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_interactions_counter.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/language_code.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

enum UploadRequired { UPLOAD_NOT_REQUIRED, UPLOAD_REQUIRED, USE_UPLOAD_RATES };

namespace base {
class TimeTicks;
}

namespace autofill {

class LogBuffer;
class LogManager;

// Password attributes (whether a password has special symbols, numeric, etc.)
enum class PasswordAttribute {
  kHasLowercaseLetter,
  kHasSpecialSymbol,
  kPasswordAttributesCount
};

// The structure of forms and fields, represented by their signatures, on a
// page. These are sequence containers to reflect their order in the DOM.
using FormAndFieldSignatures =
    std::vector<std::pair<FormSignature, std::vector<FieldSignature>>>;

struct FormData;
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
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      LogManager* log_manager);

  // Encodes this FormStructure as a vector of protobufs.
  //
  // On success, the returned vector is non-empty. The first element encodes the
  // entire FormStructure. In some cases, a |login_form_signature| is included
  // as part of the upload. This field is empty when sending upload requests for
  // non-login forms.
  //
  // If the FormStructure is a frame-transcending form, there may be additional
  // AutofillUploadContents elements in the vector, which encode the renderer
  // forms (see below for an explanation). These elements omit the renderer
  // form's metadata because retrieving this would require significant plumbing
  // from ContentAutofillRouter.
  //
  // The renderer forms are the forms that constitute a frame-transcending form.
  // ContentAutofillRouter receives these forms from the renderer and flattens
  // them into a single fresh form. Only the latter form is exposed to the rest
  // of the browser process. For server predictions, however, we want to query
  // and upload also votes also for the signatures of the renderer forms. For
  // example, the frame-transcending form
  //   <form id=1>
  //     <input autocomplete="cc-name">
  //     <iframe>
  //       #document
  //         <form id=2>
  //           <input autocomplete="cc-number">
  //         </form>
  //     </iframe>
  //   </form>
  // is flattened into a single form that contains the cc-name and cc-number
  // fields. We want to vote for this flattened form as well as for the original
  // form signatures of forms 1 and 2.
  std::vector<AutofillUploadContents> EncodeUploadRequest(
      const ServerFieldTypeSet& available_field_types,
      bool form_was_autofilled,
      const base::StringPiece& login_form_signature,
      bool observed_submission,
      bool is_raw_metadata_uploading_enabled) const;

  // Encodes the proto |query| request for the list of |forms| and their fields
  // that are valid. The queried FormSignatures and FieldSignatures are stored
  // in |queried_form_signatures| in the same order as in |query|. In case
  // multiple FormStructures have the same FormSignature, only the first one is
  // included in |query| and |queried_form_signatures|.
  static bool EncodeQueryRequest(
      const std::vector<FormStructure*>& forms,
      AutofillPageQueryRequest* query,
      std::vector<FormSignature>* queried_form_signatures);

  // Parses `payload` as AutofillQueryResponse proto and calls
  // ProcessQueryResponse().
  static void ParseApiQueryResponse(
      base::StringPiece payload,
      const std::vector<FormStructure*>& forms,
      const std::vector<FormSignature>& queried_form_signatures,
      AutofillMetrics::FormInteractionsUkmLogger*,
      LogManager* log_manager);

  // Returns predictions using the details from the given |form_structures| and
  // their fields' predicted types.
  static std::vector<FormDataPredictions> GetFieldTypePredictions(
      const std::vector<FormStructure*>& form_structures);

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
  bool ShouldBeParsed(LogManager* log_manager = nullptr) const;

  // Returns true if heuristic autofill type detection should be attempted for
  // this form.
  bool ShouldRunHeuristics() const;

  // Returns true if heuristic autofill type detection for promo codes should be
  // attempted for this form.
  bool ShouldRunPromoCodeHeuristics() const;

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

  // Sets the field types to be those set for |cached_form|.
  void RetrieveFromCache(const FormStructure& cached_form,
                         const bool should_keep_cached_value,
                         const bool only_server_and_autofill_state);

  // Logs quality metrics for |this|, which should be a user-submitted form.
  // This method should only be called after the possible field types have been
  // set for each field.  |interaction_time| should be a timestamp corresponding
  // to the user's first interaction with the form.  |submission_time| should be
  // a timestamp corresponding to the form's submission. |observed_submission|
  // indicates whether this method is called as a result of observing a
  // submission event (otherwise, it may be that an upload was triggered after
  // a form was unfocused or a navigation occurred).
  // TODO(sebsg): We log more than quality metrics. Maybe rename or split
  // function?
  void LogQualityMetrics(
      const base::TimeTicks& load_time,
      const base::TimeTicks& interaction_time,
      const base::TimeTicks& submission_time,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      bool did_show_suggestions,
      bool observed_submission,
      const FormInteractionCounts& form_interaction_counts) const;

  // Log the quality of the heuristics and server predictions for this form
  // structure, if autocomplete attributes are present on the fields (they are
  // used as golden truths).
  void LogQualityMetricsBasedOnAutocomplete(
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger)
      const;

  void LogDetermineHeuristicTypesMetrics();

  // Classifies each field in |fields_| based upon its |autocomplete| attribute,
  // if the attribute is available.  The association is stored into the field's
  // |heuristic_type|.
  // Fills |has_author_specified_types_| with |true| if the attribute is
  // available and neither empty nor set to the special values "on" or "off" for
  // at least one field.
  // Fills |has_author_specified_sections_| with |true| if the attribute
  // specifies a section for at least one field.
  void ParseFieldTypesFromAutocompleteAttributes();

  // Classifies each field in |fields_| using the regular expressions.
  void ParseFieldTypesWithPatterns(PatternSource pattern_source,
                                   LogManager* log_manager);

  // Returns the values that can be filled into the form structure for the
  // given type. For example, there's no way to fill in a value of "The Moon"
  // into ADDRESS_HOME_STATE if the form only has a
  // <select autocomplete="region"> with no "The Moon" option. Returns an
  // empty set if the form doesn't reference the given type or if all inputs
  // are accepted (e.g., <input type="text" autocomplete="region">).
  // All returned values are standardized to upper case.
  std::set<std::u16string> PossibleValues(ServerFieldType type);

  // Rationalize phone number fields in a given section, that is only fill
  // the fields that are considered composing a first complete phone number.
  void RationalizePhoneNumbersInSection(const std::string& section);

  // Overrides server predictions with specific heuristic predictions:
  // * NAME_LAST_SECOND heuristic predictions are unconditionally used.
  void OverrideServerPredictionsWithHeuristics();

  // Returns the FieldGlobalIds of the |fields_| that are eligible for manual
  // filling on form interaction.
  static std::vector<FieldGlobalId> FindFieldsEligibleForManualFilling(
      const std::vector<FormStructure*>& forms);

  const AutofillField* field(size_t index) const;
  AutofillField* field(size_t index);
  size_t field_count() const;

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

  bool has_author_specified_upi_vpa_hint() const {
    return has_author_specified_upi_vpa_hint_;
  }

  bool has_password_field() const { return has_password_field_; }

  bool is_form_tag() const { return is_form_tag_; }

  void set_submission_event(mojom::SubmissionIndicatorEvent submission_event) {
    submission_event_ = submission_event;
  }

  void set_upload_required(UploadRequired required) {
    upload_required_ = required;
  }
  UploadRequired upload_required() const { return upload_required_; }

  base::TimeTicks form_parsed_timestamp() const {
    return form_parsed_timestamp_;
  }

  bool all_fields_are_passwords() const { return all_fields_are_passwords_; }

  const FormSignature form_signature() const { return form_signature_; }

  void set_form_signature(FormSignature signature) {
    form_signature_ = signature;
  }

  // Returns a FormData containing the data this form structure knows about.
  FormData ToFormData() const;

  // Returns the possible form types.
  DenseSet<FormType> GetFormTypes() const;

  bool passwords_were_revealed() const { return passwords_were_revealed_; }
  void set_passwords_were_revealed(bool passwords_were_revealed) {
    passwords_were_revealed_ = passwords_were_revealed;
  }

  void set_password_attributes_vote(
      const std::pair<PasswordAttribute, bool>& vote) {
    password_attributes_vote_ = vote;
  }

  absl::optional<std::pair<PasswordAttribute, bool>>
  get_password_attributes_vote() const {
    return password_attributes_vote_;
  }

  void set_password_length_vote(const size_t noisified_password_length) {
    DCHECK(password_attributes_vote_.has_value())
        << "|password_length_vote_| doesn't make sense if "
           "|password_attributes_vote_| has no value.";
    password_length_vote_ = noisified_password_length;
  }

  size_t get_password_length_vote() const {
    DCHECK(password_attributes_vote_.has_value())
        << "|password_length_vote_| doesn't make sense if "
           "|password_attributes_vote_| has no value.";
    return password_length_vote_;
  }

#if defined(UNIT_TEST)
  mojom::SubmissionIndicatorEvent get_submission_event_for_testing() const {
    return submission_event_;
  }

  // Identify sections for the |fields_| for testing purposes.
  void identify_sections_for_testing() {
    ParseFieldTypesFromAutocompleteAttributes();
    IdentifySections(has_author_specified_sections_);
  }

  // Set the Overall field type for |fields_[field_index]| to |type| for testing
  // purposes.
  void set_overall_field_type_for_testing(size_t field_index,
                                          ServerFieldType type) {
    if (field_index < fields_.size() && type > 0 && type < MAX_VALID_FIELD_TYPE)
      fields_[field_index]->set_heuristic_type(GetActivePatternSource(), type);
  }
  // Set the server field type for |fields_[field_index]| to |type| for testing
  // purposes.
  void set_server_field_type_for_testing(size_t field_index,
                                         ServerFieldType type) {
    if (field_index < fields_.size() && type > 0 &&
        type < MAX_VALID_FIELD_TYPE) {
      AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
          prediction;
      prediction.set_type(type);
      fields_[field_index]->set_server_predictions({prediction});
    }
  }
#endif

  void set_password_symbol_vote(int noisified_symbol) {
    DCHECK(password_attributes_vote_.has_value())
        << "password_symbol_vote_| doesn't make sense if "
           "|password_attributes_vote_| has no value.";
    password_symbol_vote_ = noisified_symbol;
  }

  int get_password_symbol_vote() const {
    DCHECK(password_attributes_vote_.has_value())
        << "|password_symbol_vote_| doesn't make sense if "
           "|password_attributes_vote_| has no value";
    return password_symbol_vote_;
  }

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

  const LanguageCode& current_page_language() const {
    return current_page_language_;
  }

  void set_current_page_language(LanguageCode language) {
    current_page_language_ = std::move(language);
  }

  bool value_from_dynamic_change_form() const {
    return value_from_dynamic_change_form_;
  }

  void set_value_from_dynamic_change_form(bool v) {
    value_from_dynamic_change_form_ = v;
  }

  FormGlobalId global_id() const { return {host_frame_, unique_renderer_id_}; }

  FormVersion version() const { return version_; }

  void set_single_username_data(
      AutofillUploadContents::SingleUsernameData single_username_data) {
    single_username_data_ = single_username_data;
  }
  absl::optional<AutofillUploadContents::SingleUsernameData>
  single_username_data() const {
    return single_username_data_;
  }

 private:
  friend class FormStructureTestApi;

  // This class wraps a vector of vectors of field indices. The indices of a
  // vector belong to the same group.
  class SectionedFieldsIndexes;

  // Parses the field types from the server query response. |forms| must be the
  // same as the one passed to EncodeQueryRequest when constructing the query.
  // |form_interactions_ukm_logger| is used to provide logs to UKM and can be
  // null in tests.
  static void ProcessQueryResponse(
      const AutofillQueryResponse& response,
      const std::vector<FormStructure*>& forms,
      const std::vector<FormSignature>& queried_form_signatures,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      LogManager* log_manager);

  FormStructure(FormSignature form_signature,
                const std::vector<FieldSignature>& field_signatures);

  // A function to fine tune the credit cards related predictions. For example:
  // lone credit card fields in an otherwise non-credit-card related form is
  // unlikely to be correct, the function will override that prediction.
  void RationalizeCreditCardFieldPredictions(LogManager* log_manager);

  // A function to rewrite sequences of (street address, address_line2) into
  // (address_line1, address_line2) as server predictions sometimes introduce
  // wrong street address predictions.
  void RationalizeStreetAddressAndAddressLine(LogManager* log_manager);

  // The rationalization is based on the visible fields, but should be applied
  // to the hidden select fields. This is because hidden 'select' fields are
  // also autofilled to take care of the synthetic fields.
  void ApplyRationalizationsToHiddenSelects(
      size_t field_index,
      ServerFieldType new_type,
      AutofillMetrics::FormInteractionsUkmLogger*);

  // Returns true if we can replace server predictions with the heuristics one.
  bool HeuristicsPredictionsAreApplicable(size_t upper_index,
                                          size_t lower_index,
                                          ServerFieldType first_type,
                                          ServerFieldType second_type);

  // Applies upper type to upper field, and lower type to lower field, and
  // applies the rationalization also to hidden select fields if necessary.
  void ApplyRationalizationsToFields(
      size_t upper_index,
      size_t lower_index,
      ServerFieldType upper_type,
      ServerFieldType lower_type,
      AutofillMetrics::FormInteractionsUkmLogger*);

  // Returns true if the fields_[index] server type should be rationalized to
  // ADDRESS_HOME_COUNTRY.
  bool FieldShouldBeRationalizedToCountry(size_t index);

  // Set fields_[|field_index|] to |new_type| and log this change.
  void ApplyRationalizationsToFieldAndLog(
      size_t field_index,
      ServerFieldType new_type,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger);

  // Two or three fields predicted as the whole address should be address lines
  // 1, 2 and 3 instead.
  void RationalizeAddressLineFields(
      SectionedFieldsIndexes* sections_of_address_indexes,
      AutofillMetrics::FormInteractionsUkmLogger*,
      LogManager* log_manager);

  // Rationalize state and country interdependently.
  void RationalizeAddressStateCountry(
      SectionedFieldsIndexes* sections_of_state_indexes,
      SectionedFieldsIndexes* sections_of_country_indexes,
      AutofillMetrics::FormInteractionsUkmLogger*,
      LogManager* log_manager);

  // Tunes the fields with identical predictions.
  void RationalizeRepeatedFields(AutofillMetrics::FormInteractionsUkmLogger*,
                                 LogManager* log_manager);

  // Filters out fields that don't meet the relationship ruleset for their type
  // defined in |type_relationships_rules_|.
  void RationalizeTypeRelationships(LogManager* log_manager);

  // A helper function to review the predictions and do appropriate adjustments
  // when it considers necessary.
  void RationalizeFieldTypePredictions(LogManager* log_manager);

  void EncodeFormForQuery(AutofillPageQueryRequest* query,
                          std::vector<FormSignature>* queried_form_signatures,
                          std::set<FormSignature>* processed_forms) const;

  // Encodes the fields of `this` in the in-out parameter `upload`.
  // Helper function for EncodeUploadRequest().
  //
  // If `filter_renderer_form_id` is non-nullopt, only fields that originate
  // from the given renderer form are encoded. See EncodeUploadRequest() for
  // details.
  void EncodeFormFieldsForUpload(
      bool is_raw_metadata_uploading_enabled,
      absl::optional<FormGlobalId> filter_renderer_form_id,
      AutofillUploadContents* upload) const;

  // Returns true if the form has no fields, or too many.
  bool IsMalformed() const;

  // Classifies each field in |fields_| into a logical section.
  // Sections are identified by the heuristic (or by the heuristic and the
  // autocomplete section attribute, if defined when feature flag
  // kAutofillUseNewSectioningMethod is enabled) that a logical section should
  // not include multiple fields of the same autofill type (with some
  // exceptions, as described in the implementation). Credit card fields also,
  // have a single separate section from address fields.
  // If |has_author_specified_sections| is true, only the second pass --
  // distinguishing credit card sections from non-credit card ones -- is made.
  void IdentifySections(bool has_author_specified_sections);
  void IdentifySectionsWithNewMethod();

  // Returns true if field should be skipped when talking to Autofill server.
  bool ShouldSkipField(const FormFieldData& field) const;

  // Further processes the extracted |fields_|.
  void ProcessExtractedFields();

  // Extracts the parseable field name by removing a common affix.
  void ExtractParseableFieldNames();

  // Extract parseable field labels by potentially splitting labels between
  // adjacent fields.
  void ExtractParseableFieldLabels();

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
  std::vector<std::unique_ptr<AutofillField>> fields_;

  // The number of fields that are part of the form signature and that are
  // included in queries to the Autofill server.
  size_t active_field_count_ = 0;

  // Whether the server expects us to always upload, never upload, or default
  // to the stored upload rates.
  UploadRequired upload_required_ = USE_UPLOAD_RATES;

  // Whether the form includes any field types explicitly specified by the site
  // author, via the |autocompletetype| attribute.
  bool has_author_specified_types_ = false;

  // Whether the form includes any sections explicitly specified by the site
  // author, via the autocomplete attribute.
  bool has_author_specified_sections_ = false;

  // Whether the form includes a field that explicitly sets it autocomplete
  // type to "upi-vpa".
  bool has_author_specified_upi_vpa_hint_ = false;

  // Whether the form was parsed for autocomplete attribute, thus assigning
  // the real values of |has_author_specified_types_| and
  // |has_author_specified_sections_|.
  bool was_parsed_for_autocomplete_attributes_ = false;

  // True if the form contains at least one password field.
  bool has_password_field_ = false;

  // True if the form is a <form>.
  bool is_form_tag_ = true;

  // True if all form fields are password fields.
  bool all_fields_are_passwords_ = false;

  // The unique signature for this form, composed of the target url domain,
  // the form name, and the form field names in a 64-bit hash.
  FormSignature form_signature_;

  // The timestamp (not wallclock time) when this form was initially parsed.
  base::TimeTicks form_parsed_timestamp_;

  // If phone number rationalization has been performed for a given section.
  std::map<std::string, bool> phone_rationalized_;

  // True iff the form is a password form and the user has seen the password
  // value before accepting the prompt to save. Used for crowdsourcing.
  bool passwords_were_revealed_ = false;

  // The vote about password attributes (e.g. whether the password has a numeric
  // character).
  absl::optional<std::pair<PasswordAttribute, bool>> password_attributes_vote_;

  // If |password_attribute_vote_| contains (kHasSpecialSymbol, true), this
  // field contains nosified information about a special symbol in a
  // user-created password stored as ASCII code. The default value of 0
  // indicates that no symbol was set.
  int password_symbol_vote_ = 0;

  // Noisified password length for crowdsourcing. If |password_attributes_vote_|
  // has no value, |password_length_vote_| should be ignored.
  size_t password_length_vote_;

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

  bool value_from_dynamic_change_form_ = false;

  // A unique identifier of the containing frame.
  // This value must not be leaked to other renderer processes.
  LocalFrameToken host_frame_;

  // A monotonically increasing counter that indicates the generation of the
  // form.
  FormVersion version_;

  // An identifier of the form that is unique among the forms from the same
  // frame.
  FormRendererId unique_renderer_id_;

  // Single username details, if applicable.
  absl::optional<AutofillUploadContents::SingleUsernameData>
      single_username_data_;
};

LogBuffer& operator<<(LogBuffer& buffer, const FormStructure& form);
std::ostream& operator<<(std::ostream& buffer, const FormStructure& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_
