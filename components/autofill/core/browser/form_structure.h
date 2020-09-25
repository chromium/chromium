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
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/renderer_id.h"
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
  virtual ~FormStructure();

  // Runs several heuristics against the form fields to determine their possible
  // types.
  void DetermineHeuristicTypes(LogManager* log_manager = nullptr);

  // Encodes the proto |upload| request from this FormStructure, and stores
  // the (single) FormSignature and the signatures of the fields to be uploaded
  // in |encoded_signatures|.
  // In some cases, a |login_form_signature| is included as part of the upload.
  // This field is empty when sending upload requests for non-login forms.
  bool EncodeUploadRequest(
      const ServerFieldTypeSet& available_field_types,
      bool form_was_autofilled,
      const std::string& login_form_signature,
      bool observed_submission,
      autofill::AutofillUploadContents* upload,
      std::vector<FormSignature>* encoded_signatures) const;

  // Encodes the proto |query| request for the list of |forms| and their fields
  // that are valid. The queried FormSignatures and FieldSignatures are stored
  // in |queried_form_signatures| in the same order as in |query|. In case
  // multiple FormStructures have the same FormSignature, only the first one is
  // included in |query| and |queried_form_signatures|.
  static bool EncodeQueryRequest(
      const std::vector<FormStructure*>& forms,
      autofill::AutofillPageQueryRequest* query,
      std::vector<FormSignature>* queried_form_signatures);

  // Parses `payload` as AutofillQueryResponse proto and calls
  // ProcessQueryResponse().
  static void ParseApiQueryResponse(
      base::StringPiece payload,
      const std::vector<FormStructure*>& forms,
      const std::vector<FormSignature>& queried_form_signatures,
      AutofillMetrics::FormInteractionsUkmLogger*);

  // Returns predictions using the details from the given |form_structures| and
  // their fields' predicted types.
  static std::vector<FormDataPredictions> GetFieldTypePredictions(
      const std::vector<FormStructure*>& form_structures);

  // Returns whether sending autofill field metadata to the server is enabled.
  static bool IsAutofillFieldMetadataEnabled();

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
      bool observed_submission) const;

  // Log the quality of the heuristics and server predictions for this form
  // structure, if autocomplete attributes are present on the fields (they are
  // used as golden truths).
  void LogQualityMetricsBasedOnAutocomplete(
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger)
      const;

  // Classifies each field in |fields_| based upon its |autocomplete| attribute,
  // if the attribute is available.  The association is stored into the field's
  // |heuristic_type|.
  // Fills |has_author_specified_types_| with |true| if the attribute is
  // available and neither empty nor set to the special values "on" or "off" for
  // at least one field.
  // Fills |has_author_specified_sections_| with |true| if the attribute
  // specifies a section for at least one field.
  void ParseFieldTypesFromAutocompleteAttributes();

  // Returns the values that can be filled into the form structure for the
  // given type. For example, there's no way to fill in a value of "The Moon"
  // into ADDRESS_HOME_STATE if the form only has a
  // <select autocomplete="region"> with no "The Moon" option. Returns an
  // empty set if the form doesn't reference the given type or if all inputs
  // are accepted (e.g., <input type="text" autocomplete="region">).
  // All returned values are standardized to upper case.
  std::set<base::string16> PossibleValues(ServerFieldType type);

  // Rationalize phone number fields in a given section, that is only fill
  // the fields that are considered composing a first complete phone number.
  void RationalizePhoneNumbersInSection(std::string section);

  // Overrides server predictions with specific heuristic predictions:
  // * NAME_LAST_SECOND heuristic predictions are unconditionally used.
  void OverrideServerPredictionsWithHeuristics();

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

  const base::string16& form_name() const { return form_name_; }

  const base::string16& id_attribute() const { return id_attribute_; }

  const base::string16& name_attribute() const { return name_attribute_; }

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
  std::set<FormType> GetFormTypes() const;

  bool passwords_were_revealed() const { return passwords_were_revealed_; }
  void set_passwords_were_revealed(bool passwords_were_revealed) {
    passwords_were_revealed_ = passwords_were_revealed;
  }

  void set_password_attributes_vote(
      const std::pair<PasswordAttribute, bool>& vote) {
    password_attributes_vote_ = vote;
  }

  base::Optional<std::pair<PasswordAttribute, bool>>
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
      fields_[field_index]->set_heuristic_type(type);
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

  // Returns an identifier that is used by the refill logic. Takes the first non
  // empty of these or returns an empty string:
  // - Form name
  // - Name for Autofill of first field
  base::string16 GetIdentifierForRefill() const;

  int developer_engagement_metrics() { return developer_engagement_metrics_; }

  void set_randomized_encoder(std::unique_ptr<RandomizedEncoder> encoder);

  void set_is_rich_query_enabled(bool v) { is_rich_query_enabled_ = v; }

  const std::string& page_language() const { return page_language_; }

  void set_page_language(std::string language) {
    page_language_ = std::move(language);
  }

  bool value_from_dynamic_change_form() const {
    return value_from_dynamic_change_form_;
  }

  void set_value_from_dynamic_change_form(bool v) {
    value_from_dynamic_change_form_ = v;
  }

  FormRendererId unique_renderer_id() const { return unique_renderer_id_; }

  bool ShouldSkipFieldVisibleForTesting(const FormFieldData& field) const {
    return ShouldSkipField(field);
  }

  static void ProcessQueryResponseForTesting(
      const AutofillQueryResponse& response,
      const std::vector<FormStructure*>& forms,
      const std::vector<FormSignature>& queried_form_signatures,
      AutofillMetrics::FormInteractionsUkmLogger*
          form_interactions_ukm_logger) {
    ProcessQueryResponse(response, forms, queried_form_signatures,
                         form_interactions_ukm_logger);
  }

 private:
  friend class AutofillMergeTest;
  friend class FormStructureTestImpl;
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadTest, QueryAndUploadTest);
  FRIEND_TEST_ALL_PREFIXES(FormStructureTestImpl, FindLongestCommonPrefix);
  FRIEND_TEST_ALL_PREFIXES(FormStructureTestImpl, FindLongestCommonAffixLength);
  FRIEND_TEST_ALL_PREFIXES(FormStructureTestImpl, IsValidParseableName);
  FRIEND_TEST_ALL_PREFIXES(FormStructureTestImpl,
                           RationalizePhoneNumber_RunsOncePerSection);

  class SectionedFieldsIndexes {
   public:
    SectionedFieldsIndexes();
    ~SectionedFieldsIndexes();

    size_t LastFieldIndex() const {
      if (sectioned_indexes.empty())
        return (size_t)-1;  // Shouldn't happen.
      return sectioned_indexes.back().back();
    }

    void AddFieldIndex(const size_t index, bool is_new_section) {
      if (is_new_section || Empty()) {
        sectioned_indexes.push_back(std::vector<size_t>(1, index));
        return;
      }
      sectioned_indexes.back().push_back(index);
    }

    void WalkForwardToTheNextSection() { current_section_ptr++; }

    bool IsFinished() const {
      return current_section_ptr >= sectioned_indexes.size();
    }

    size_t CurrentIndex() const { return CurrentSection()[0]; }

    std::vector<size_t> CurrentSection() const {
      if (current_section_ptr < sectioned_indexes.size())
        return sectioned_indexes[current_section_ptr];
      return std::vector<size_t>(1, (size_t)-1);  // To handle edge cases.
    }

    void Reset() { current_section_ptr = 0; }

    bool Empty() const { return sectioned_indexes.empty(); }

   private:
    // A vector of sections. Each section is a vector of some of the indexes
    // that belong to the same section. The sections and indexes are sorted by
    // their order of appearance on the form.
    std::vector<std::vector<size_t>> sectioned_indexes;
    // Points to a vector of indexes that belong to the same section.
    size_t current_section_ptr = 0;
  };

  // Parses the field types from the server query response. |forms| must be the
  // same as the one passed to EncodeQueryRequest when constructing the query.
  // |form_interactions_ukm_logger| is used to provide logs to UKM and can be
  // null in tests.
  static void ProcessQueryResponse(
      const AutofillQueryResponse& response,
      const std::vector<FormStructure*>& forms,
      const std::vector<FormSignature>& queried_form_signatures,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger);

  FormStructure(FormSignature form_signature,
                const std::vector<FieldSignature>& field_signatures);

  // A function to fine tune the credit cards related predictions. For example:
  // lone credit card fields in an otherwise non-credit-card related form is
  // unlikely to be correct, the function will override that prediction.
  void RationalizeCreditCardFieldPredictions();

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
      AutofillMetrics::FormInteractionsUkmLogger*);

  // Rationalize state and country interdependently.
  void RationalizeAddressStateCountry(
      SectionedFieldsIndexes* sections_of_state_indexes,
      SectionedFieldsIndexes* sections_of_country_indexes,
      AutofillMetrics::FormInteractionsUkmLogger*);

  // Tunes the fields with identical predictions.
  void RationalizeRepeatedFields(AutofillMetrics::FormInteractionsUkmLogger*);

  // Filters out fields that don't meet the relationship ruleset for their type
  // defined in |type_relationships_rules_|.
  void RationalizeTypeRelationships();

  // A helper function to review the predictions and do appropriate adjustments
  // when it considers necessary.
  void RationalizeFieldTypePredictions();

  void EncodeFormForQuery(
      autofill::AutofillPageQueryRequest::Form* query_form,
      std::vector<FormSignature>* queried_form_signatures) const;

  void EncodeFormForUpload(
      autofill::AutofillUploadContents* upload,
      std::vector<FormSignature>* encoded_signatures) const;

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

  // Returns true if field should be skipped when talking to Autofill server.
  bool ShouldSkipField(const FormFieldData& field) const;

  // Further processes the extracted |fields_|.
  void ProcessExtractedFields();

  // Tries to set |parseable_name| fields by stripping the given offsets from
  // both sides of the |name| fields.
  // Sets |parseable_name| to |name| if the sum of offsets is bigger than
  // |name|.
  // Sets all |parseable_name| to |name| without modification and returns
  // false if a name fails the |IsValidParseableName()| check after stripping.
  bool SetStrippedParseableNames(size_t offset_left, size_t offset_right);

  // Returns true if |string| is a valid parseable_name. Current criterion
  // is the |autofill::kParseableNameValidationRe| regex.
  static bool IsValidParseableName(base::string16 string);

  // Returns the length of the longest common prefix found within |strings|
  // if |findCommonSuffix| is false. Otherwise returns longest common suffix.
  static size_t FindLongestCommonAffixLength(
      const std::vector<base::StringPiece16>& strings,
      bool findCommonSuffix = false);

  // Returns the longest common prefix found within |strings|. Strings below a
  // threshold length defined by |kMinCommonNamePrefixLength| are excluded
  // when performing this check; this is needed because an exceptional
  // field may be missing a prefix which is otherwise consistently applied.
  // For instance, a framework may only apply a prefix to those fields
  // which are bound when POSTing.
  //
  // Soon to be replaced by FindLongestCommonPrefixLength
  static base::string16 FindLongestCommonPrefix(
      const std::vector<base::string16>& strings);

  // The language detected for this form's page, prior to any translations
  // performed by Chrome.
  std::string page_language_;

  // The id attribute of the form.
  base::string16 id_attribute_;

  // The name attribute of the form.
  base::string16 name_attribute_;

  // The name of the form.
  base::string16 form_name_;

  // The titles of form's buttons.
  ButtonTitleList button_titles_;

  // The type of the event that was taken as an indication that the form has
  // been successfully submitted.
  mojom::SubmissionIndicatorEvent submission_event_ =
      mojom::SubmissionIndicatorEvent::NONE;

  // The source URL (excluding the query parameters and fragment identifiers).
  GURL source_url_;

  // The full source URL including query parameters and fragment identifiers.
  GURL full_source_url_;

  // The target URL.
  GURL target_url_;

  // The origin of the main frame of this form.
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

  // True if the form is made of unowned fields (i.e., not within a <form> tag)
  // in what appears to be a checkout flow. This attribute is only calculated
  // and used if features::kAutofillRestrictUnownedFieldsToFormlessCheckout is
  // enabled, to prevent heuristics from running on formless non-checkout.
  bool is_formless_checkout_ = false;

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
  base::Optional<std::pair<PasswordAttribute, bool>> password_attributes_vote_;

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

  FormRendererId unique_renderer_id_;

  DISALLOW_COPY_AND_ASSIGN(FormStructure);
};

LogBuffer& operator<<(LogBuffer& buffer, const FormStructure& form);
std::ostream& operator<<(std::ostream& buffer, const FormStructure& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_
