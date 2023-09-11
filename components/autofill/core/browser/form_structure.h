// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
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
  kHasLetter,
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
      const GeoIpCountryCode& client_country,
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
  // from AutofillDriverRouter.
  //
  // The renderer forms are the forms that constitute a frame-transcending form.
  // AutofillDriverRouter receives these forms from the renderer and flattens
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
    // kFormParsing refers to the process of assigning field types to fields
    // when the renderer notifies the browser about a new, modified or
    // interacted with form.
    //
    // During form parsing, the browser receives a FormData object from the
    // renderer that is converted to a FormStructure object. RetrieveFromCache
    // is responsible for retaining information from the history of the fields
    // in the form (e.g. information about previous fill operations):
    //
    // - The `is_autofilled` and similar members of a field are copied from the
    //   cached form so that a field that was once labeled as autofilled remains
    //   autofilled.
    //
    // - The `value` of a field is copied from the cache as it represents the
    //   initial value of a field during page load time and must not be updated
    //   if a form is parsed a second time.
    //
    // - Also server predictions are preserved (while heuristic predictions
    //   are discarded because they will be generated during the parsing).
    kFormParsing,

    // kFormImport refers to the process of importing address profiles / credit
    // cards from user-filled forms after a form submission.
    //
    // During form import, the browser receives a FormData object from the
    // renderer that is converted to a FormStructure object. RetrieveFromCache
    // is responsible for processing the FormData so that the FormStructure
    // contains the right information that facilitate importing. Therefore,
    // similar work happen as for kFormParsing, except:
    //
    // - During form import, we want to copy field type information from
    //   previous parse operations as these tell which information to save.
    //
    // - The `value` of a FormStructure's field typically represents the
    //   initially observed value of a field during page load. So during
    //   kFormParsing the value is persisted. During import, however, we want to
    //   store the last observed value. Furthermore, if the submitted value of a
    //   field has never been changed, we ignore the previous value from import
    //   (unless it's a state or country as websites can find meanigful default
    //   values via GeoIP).
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

  // Classifies each field in |fields_| using the regular expressions.
  void ParseFieldTypesWithPatterns(PatternSource pattern_source,
                                   const GeoIpCountryCode& client_country,
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
  void RationalizePhoneNumbersInSection(const Section& section);

  // Returns the FieldGlobalIds of the |fields_| that are eligible for manual
  // filling on form interaction.
  static std::vector<FieldGlobalId> FindFieldsEligibleForManualFilling(
      const std::vector<FormStructure*>& forms);

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

  std::vector<AutofillUploadContents::SingleUsernameData> single_username_data()
      const {
    return single_username_data_;
  }

  // The signatures of forms recently submitted on the same origin within a
  // small period of time.
  struct FormAssociations {
    absl::optional<FormSignature> last_address_form_submitted;
    absl::optional<FormSignature> second_last_address_form_submitted;
    absl::optional<FormSignature> last_credit_card_form_submitted;
  };

  void set_form_associations(FormAssociations associations) {
    form_associations_ = associations;
  }

 private:
  friend class FormStructureTestApi;

  // Sets the rank of each field in the form.
  void DetermineFieldRanks();

  // Production code only uses the default parameters.
  // Unit tests also test other parameters.
  struct ShouldBeParsedParams {
    size_t min_required_fields =
        std::min({kMinRequiredFieldsForHeuristics, kMinRequiredFieldsForQuery,
                  kMinRequiredFieldsForUpload});
    size_t required_fields_for_forms_with_only_password_fields =
        kRequiredFieldsForFormsWithOnlyPasswordFields;
  };

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

  [[nodiscard]] bool ShouldBeParsed(ShouldBeParsedParams params,
                                    LogManager* log_manager = nullptr) const;

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

  // Classifies each field in `fields_` into a logical section.
  // The function consists of 2 passes:
  //   - 1st pass: Performed only when `ignore_autocomplete` is true or none of
  //               the fields in `fields_` has a valid autocomplete section.
  //               Sections are identified by the heuristic that a logical
  //               section should not include multiple fields of the same
  //               autofill type with some exceptions, as described in the
  //               implementation.
  //   - 2nd pass: Separate credit card fields from all other fields.
  // Note: `ignore_autocomplete` is set to true only when identifying sections
  // after server response.
  void IdentifySections(bool ignore_autocomplete);
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

  // Whether the form includes a field that explicitly sets it autocomplete
  // type to "upi-vpa".
  bool has_author_specified_upi_vpa_hint_ = false;

  // True if the form contains at least one password field.
  bool has_password_field_ = false;

  // True if the form is a <form>.
  bool is_form_tag_ = true;

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

  // If phone number rationalization has been performed for a given section.
  std::set<Section> phone_rationalized_;

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
