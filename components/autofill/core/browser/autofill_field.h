// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_

#include <stddef.h>

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/profile_value_source.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

// Specifies if the Username First Flow vote has intermediate values.
enum class IsMostRecentSingleUsernameCandidate {
  // Field is not part of Username First Flow.
  kNotPartOfUsernameFirstFlow = 0,
  // Field is candidate for username in Username First Flow and has no
  // intermediate fields
  kMostRecentCandidate = 1,
  // Field is candidate for username in Username First Flow and has intermediate
  // fields between candidate and password form.
  kHasIntermediateValuesInBetween = 2,
};

// Specifies which type of field value is desired from AutofillField::value().
// TODO: crbug.com/40227496 - Remove together with `value(ValueSemantics)`.
enum class ValueSemantics {
  // The field's last known value or the field's value to be filled:
  // FormFieldData::value().
  kCurrent,
  // The field's first known value.
  kInitial,
};

class AutofillField : public FormFieldData {
 public:
  using FieldLogEventType = absl::variant<absl::monostate,
                                          AskForValuesToFillFieldLogEvent,
                                          TriggerFillFieldLogEvent,
                                          FillFieldLogEvent,
                                          TypingFieldLogEvent,
                                          HeuristicPredictionFieldLogEvent,
                                          AutocompleteAttributeFieldLogEvent,
                                          ServerPredictionFieldLogEvent,
                                          RationalizationFieldLogEvent,
                                          AblationFieldLogEvent>;

  AutofillField();
  explicit AutofillField(const FormFieldData& field);

  AutofillField(const AutofillField&) = delete;
  AutofillField& operator=(const AutofillField&) = delete;
  AutofillField(AutofillField&&);
  AutofillField& operator=(AutofillField&&);

  virtual ~AutofillField();

  // Creates AutofillField that has bare minimum information for uploading
  // votes, namely a field signature. Warning: do not use for Autofill code,
  // since it is likely missing some fields.
  static std::unique_ptr<AutofillField> CreateForPasswordManagerUpload(
      FieldSignature field_signature);

  FieldType heuristic_type() const;
  FieldType heuristic_type(HeuristicSource s) const;
  FieldType server_type() const;
  bool server_type_prediction_is_override() const;
  const std::vector<
      AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction>&
  server_predictions() const {
    return server_predictions_;
  }
  const std::vector<
      AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction>&
  experimental_server_predictions() const {
    return experimental_server_predictions_;
  }
  std::optional<bool> may_use_prefilled_placeholder() const {
    return may_use_prefilled_placeholder_;
  }
  HtmlFieldType html_type() const { return html_type_; }
  HtmlFieldMode html_mode() const { return html_mode_; }
  const FieldTypeSet& possible_types() const { return possible_types_; }
  bool previously_autofilled() const { return previously_autofilled_; }
  const std::u16string& parseable_name() const { return parseable_name_; }
  const std::u16string& parseable_label() const { return parseable_label_; }
  bool only_fill_when_focused() const { return only_fill_when_focused_; }

  // Setters for the detected types.
  void set_heuristic_type(HeuristicSource s, FieldType t);
  void set_server_predictions(
      std::vector<AutofillQueryResponse::FormSuggestion::FieldSuggestion::
                      FieldPrediction> predictions);

  void set_may_use_prefilled_placeholder(
      std::optional<bool> may_use_prefilled_placeholder) {
    may_use_prefilled_placeholder_ = may_use_prefilled_placeholder;
  }
  void set_possible_types(const FieldTypeSet& possible_types) {
    possible_types_ = possible_types;
  }

  // Adds a profile `identifier` for `type` as a possible profile value source.
  // If `type` is not an address type the call will be a noop.
  PossibleProfileValueSources* possible_profile_value_sources() {
    return &possible_profile_value_sources_;
  }

  void set_possible_profile_value_sources(
      PossibleProfileValueSources possible_profile_value_sources) {
    possible_profile_value_sources_ = std::move(possible_profile_value_sources);
  }

  std::optional<ProfileValueSource> assumed_profile_value_source() const {
    return assumed_profile_value_source_;
  }

  void set_assumed_profile_value_source(
      std::optional<ProfileValueSource> value_source) {
    assumed_profile_value_source_ = value_source;
  }

  void SetHtmlType(HtmlFieldType type, HtmlFieldMode mode);

  void set_previously_autofilled(bool previously_autofilled) {
    previously_autofilled_ = previously_autofilled;
  }
  void set_parseable_name(std::u16string parseable_name) {
    parseable_name_ = std::move(parseable_name);
  }
  void set_parseable_label(std::u16string parseable_label) {
    parseable_label_ = std::move(parseable_label);
  }

  void set_only_fill_when_focused(bool fill_when_focused) {
    only_fill_when_focused_ = fill_when_focused;
  }

  // Set the type of the field. This sets the value returned by |Type|.
  // This function can be used to override the value that would be returned by
  // |ComputedType|.
  // As the |type| is expected to depend on |ComputedType|, the value will be
  // reset to |ComputedType| if some internal value change (e.g. on call to
  // (|set_heuristic_type|).
  // |SetTypeTo| cannot be called with
  // type.GetStoreableType() == NO_SERVER_DATA.
  void SetTypeTo(const AutofillType& type);

  // This function returns |ComputedType| unless the value has been overriden
  // by |SetTypeTo|.
  // (i.e. overall_type_ != NO_SERVER_DATA ? overall_type_ : ComputedType())
  AutofillType Type() const;

  // This function automatically chooses among the Autofill server, heuristic
  // and html type, depending on the data available for this field alone. This
  // type does not take into account the rationalization involving the
  // surrounding fields.
  AutofillType ComputedType() const;

  // The rank of a field is N iff this field is preceded by N other fields
  // in the frame-transcending form.
  size_t rank() const { return rank_; }
  void set_rank(size_t rank) { rank_ = rank; }

  // The rank in the signature group of a field is N in a form iff this field is
  // preceded by N other fields whose signature is identical to this field's
  // signature in the frame-transcending form.
  size_t rank_in_signature_group() const { return rank_in_signature_group_; }
  void set_rank_in_signature_group(size_t rank_in_signature_group) {
    rank_in_signature_group_ = rank_in_signature_group;
  }

  // The rank of a field is N iff this field is preceded by N other fields
  // in the form in the host frame.
  size_t rank_in_host_form() const { return rank_in_host_form_; }
  void set_rank_in_host_form(size_t rank_in_host_form) {
    rank_in_host_form_ = rank_in_host_form;
  }

  // The rank in the signature group of a field is N in a form iff this field is
  // preceded by N other fields whose signature is identical to this field's
  // signature in the form in the host frame.
  size_t rank_in_host_form_signature_group() const {
    return rank_in_host_form_signature_group_;
  }
  void set_rank_in_host_form_signature_group(
      size_t rank_in_host_form_signature_group) {
    rank_in_host_form_signature_group_ = rank_in_host_form_signature_group;
  }

  // The unique signature of this field, composed of the field name and the html
  // input type in a 32-bit hash.
  FieldSignature GetFieldSignature() const;

  // Returns the field signature as string.
  std::string FieldSignatureAsStr() const;

  // Returns true if the field type has been determined (without the text in the
  // field).
  bool IsFieldFillable() const;

  // Returns true if the field's type is a credit card expiration type.
  bool HasExpirationDateType() const;

  // Address Autofill is disabled for fields with unrecognized autocomplete
  // attribute - except if the field has a server overwrite.
  // Without `kAutofillPredictionsForAutocompleteUnrecognized`, this happens
  // implicitly, since ac=unrecognized suppresses the predicted type. As of
  // `kAutofillPredictionsForAutocompleteUnrecognized`, ac=unrecognized fields
  // receive a predictions, but suggestions and filling are still suppressed.
  // This function can be used to determine whether suggestions and filling
  // should be suppressed for this field (independently of the predicted type).
  bool ShouldSuppressSuggestionsAndFillingByDefault() const;

  // Returns the requested current or initial value depending on the
  // `ValueSemantics`, if `features::kAutofillFixValueSemantics` is enabled.
  // Otherwise just forwards to `FormFieldData::value().
  //
  // In the context of form submission and import, consider calling
  // `value_for_import()`.
  //
  // Currently, `value(ValueSemantics::kInitial)` is the empty string for fields
  // of FormControlType::kSelect*.
  // TODO: crbug.com/40227496 - Let `value(kInitial)` for select elements behave
  // the same as for non-select elements.
  //
  // TODO: crbug.com/40227496 - When kAutofillFixValueSemantics is cleaned up,
  // replace
  // - `value(ValueSemantics::kCurrent)` with `FormFieldData::value()`
  // - `value(ValueSemantics::kInitial)` with `AutofillField::initial_value()`
  const std::u16string& value(ValueSemantics s) const;

  // Returns the current value, formatted as desired for import:
  // (1) If the user left a field unchanged, returns the empty string.
  // (2) If the field has FormControlType::kSelect* and has a selected text,
  //     it is FormFieldData::selected_text().
  //
  // The motivation behind (1) is that unchanged values usually carry little
  // value for importing. The exception are <select> feilds, which often have
  // a correct default value, so we consider them for import even if their value
  // didn't change.
  // TODO: crbug.com/40137859 - Consider making an exception for also for
  // non-<select> ADDRESS_HOME_{STATE,COUNTRY} fields.
  //
  // The motivation behind (2) is that the human-readable text of an <option> is
  // usually better suited for import than the its value. See the documentation
  // of FormFieldData::value() and FormFieldData::selected_text() for further
  // details.
  //
  // This function only behaves reasonably if kAutofillFixValueSemantics and
  // kAutofillFixCurrentValueInImport are enabled. If the latter is not enabled,
  // FormStructure::RetrieveFromCache() resets the field's current value, with
  // the intention of avoiding form import.
  // TODO: crbug.com/40227496 - Remove the previous paragraph when the feature
  // is launched.
  const std::u16string& value_for_import() const;

  // Sets the field's current value, if `features::kAutofillFixValueSemantics`
  // is enabled. Otherwise just forwards to FormFieldData::set_value().
  void set_initial_value(std::u16string initial_value,
                         base::PassKey<FormStructure> pass_key);

  void set_initial_value_hash(uint32_t value) { initial_value_hash_ = value; }
  std::optional<uint32_t> initial_value_hash() { return initial_value_hash_; }

  // TODO: crbug.com/40227496 - Remove when kAutofillFixValueSemantics is
  // cleaned up.
  void set_initial_value_changed(std::optional<bool> initial_value_changed) {
    initial_value_changed_ = initial_value_changed;
  }
  std::optional<bool> initial_value_changed() const {
    return initial_value_changed_;
  }

  void set_value_identified_as_potentially_sensitive(
      bool potentially_sensitive) {
    value_identified_as_potentially_sensitive_ = potentially_sensitive;
  }

  bool value_identified_as_potentially_sensitive() const {
    return value_identified_as_potentially_sensitive_;
  }

  void set_field_is_eligible_for_prediction_improvements(
      std::optional<bool> eligibily) {
    field_is_eligible_for_prediction_improvements_ = eligibily;
  }
  std::optional<bool> field_is_eligible_for_prediction_improvements() const {
    return field_is_eligible_for_prediction_improvements_;
  }

  void set_credit_card_number_offset(size_t position) {
    credit_card_number_offset_ = position;
  }
  size_t credit_card_number_offset() const {
    return credit_card_number_offset_;
  }

  void set_generation_type(
      AutofillUploadContents::Field::PasswordGenerationType type) {
    generation_type_ = type;
  }
  AutofillUploadContents::Field::PasswordGenerationType generation_type()
      const {
    return generation_type_;
  }

  void set_generated_password_changed(bool generated_password_changed) {
    generated_password_changed_ = generated_password_changed;
  }
  bool generated_password_changed() const {
    return generated_password_changed_;
  }

  void set_vote_type(AutofillUploadContents::Field::VoteType type) {
    vote_type_ = type;
  }
  AutofillUploadContents::Field::VoteType vote_type() const {
    return vote_type_;
  }

  void SetPasswordRequirements(PasswordRequirementsSpec spec);
  const std::optional<PasswordRequirementsSpec>& password_requirements() const {
    return password_requirements_;
  }

  // Getter and Setter methods for |state_is_a_matching_type_|.
  void set_state_is_a_matching_type(bool value = true) {
    state_is_a_matching_type_ = value;
  }
  bool state_is_a_matching_type() const { return state_is_a_matching_type_; }

  void set_single_username_vote_type(
      AutofillUploadContents::Field::SingleUsernameVoteType vote_type) {
    single_username_vote_type_ = vote_type;
  }
  std::optional<AutofillUploadContents::Field::SingleUsernameVoteType>
  single_username_vote_type() const {
    return single_username_vote_type_;
  }

  void set_is_most_recent_single_username_candidate(
      IsMostRecentSingleUsernameCandidate
          is_most_recent_single_username_candidate) {
    is_most_recent_single_username_candidate_ =
        is_most_recent_single_username_candidate;
  }

  IsMostRecentSingleUsernameCandidate is_most_recent_single_username_candidate()
      const {
    return is_most_recent_single_username_candidate_;
  }

  void set_field_log_events(const std::vector<FieldLogEventType>& events) {
    field_log_events_ = events;
  }

  const std::vector<FieldLogEventType>& field_log_events() const {
    return field_log_events_;
  }

  // Avoid holding references to the return value. It is invalidated by
  // AppendLogEventIfNotRepeated().
  base::optional_ref<FieldLogEventType> last_field_log_event() {
    if (!field_log_events_.empty()) {
      return field_log_events_.back();
    }
    return std::nullopt;
  }

  // Add the field log events into the vector |field_log_events_| when it is
  // not the same as the last log event in the vector.
  void AppendLogEventIfNotRepeated(const FieldLogEventType& log_event);

  // Clear all the log events for this field.
  void ClearLogEvents() { field_log_events_.clear(); }

  void set_autofill_source_profile_guid(
      std::optional<std::string> autofill_profile_guid) {
    autofill_source_profile_guid_ = std::move(autofill_profile_guid);
  }
  const std::optional<std::string>& autofill_source_profile_guid() const {
    return autofill_source_profile_guid_;
  }

  void set_autofilled_type(std::optional<FieldType> autofilled_type) {
    autofilled_type_ = std::move(autofilled_type);
  }
  std::optional<FieldType> autofilled_type() const { return autofilled_type_; }

  void set_filling_product(FillingProduct filling_product) {
    filling_product_ = filling_product;
  }
  FillingProduct filling_product() const { return filling_product_; }

  bool WasAutofilledWithFallback() const;

  void set_did_trigger_suggestions(bool did_trigger_suggestions) {
    did_trigger_suggestions_ = did_trigger_suggestions;
  }
  bool did_trigger_suggestions() const { return did_trigger_suggestions_; }

  void set_was_focused(bool was_focused) { was_focused_ = was_focused; }
  bool was_focused() const { return was_focused_; }

 private:
  explicit AutofillField(FieldSignature field_signature);

  // Whether the heuristics or server predict a credit card field.
  bool IsCreditCardPrediction() const;

  std::optional<FieldSignature> field_signature_;

  size_t rank_ = 0;
  size_t rank_in_signature_group_ = 0;
  size_t rank_in_host_form_ = 0;
  size_t rank_in_host_form_signature_group_ = 0;

  // The possible types of the field, as determined by the Autofill server.
  std::vector<
      AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction>
      server_predictions_;
  // Predictions from the Autofill server which are not intended for general
  // consumption. They are used for metrics and/or finch experiments.
  std::vector<
      AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction>
      experimental_server_predictions_;

  // Whether the server-side classification believes that the field
  // may be pre-filled with a placeholder in the value attribute.
  // For autofillable types, `nullopt` indicates that there is no server-side
  // classification. For PWM, `nullopt` and `false` are currently identical.
  std::optional<bool> may_use_prefilled_placeholder_ = std::nullopt;

  // Requirements the site imposes to passwords (for password generation).
  // Corresponds to the requirements determined by the Autofill server.
  std::optional<PasswordRequirementsSpec> password_requirements_;

  // Predictions which where calculated on the client. This is initialized to
  // `NO_SERVER_DATA`, which means "NO_DATA", i.e. no classification was
  // attempted.
  std::array<FieldType, static_cast<size_t>(HeuristicSource::kMaxValue) + 1>
      local_type_predictions_;

  // The type of the field. Overrides all other types (html_type_,
  // heuristic_type_).
  // |AutofillType(NO_SERVER_DATA)| is used when this |overall_type_| has not
  // been set.
  // This field serves as a cache to prevent frequent re-evaluation of
  // ComputedType(). It is invalidated when set_heuristic_type(),
  // set_server_predictions() or SetHtmlType() is called and then set to a
  // value during the rationalization.
  AutofillType overall_type_;

  // The type of the field, as specified by the site author in HTML.
  HtmlFieldType html_type_ = HtmlFieldType::kUnspecified;

  // The "mode" of the field, as specified by the site author in HTML.
  // Currently this is used to distinguish between billing and shipping fields.
  HtmlFieldMode html_mode_ = HtmlFieldMode::kNone;

  // The set of possible types for this field. It is normally only populated on
  // submission time together with the `possible_profile_value_sources_`.
  FieldTypeSet possible_types_;

  // An Autofill profile is a source for a filled value when the field's value
  // is contained in the profile stored as a specific type. It does not mean
  // that the value was actually filled from this Autofill profile. It is
  // normally only populated on submission time along with the
  // `possible_types_`. It contains the address related information that is
  // contained in `possible_types_` with the additional information in which
  // profile the matching type was detected.
  PossibleProfileValueSources possible_profile_value_sources_;

  // Holds the assumed profile and type of the `value` found in this field.
  // The assumed source may be derived by using the
  // `possible_profile_value_sources_` or the `autofill_source_profile_guid_`.
  // There are no strong guarantees regarding the consistency between those
  // different fields. The `nullopt` state indicates that no assumed value
  // source was identified yet.
  std::optional<ProfileValueSource> assumed_profile_value_source_;

  // The field's initial value. By default, it's the same as the field's
  // `value()`, but FormStructure::RetrieveFromCache() may override it.
  std::u16string initial_value_ = value(ValueSemantics::kCurrent);

  // A low-entropy hash of the field's initial value before user-interactions or
  // automatic fillings. This field is used to detect static placeholders.
  std::optional<uint32_t> initial_value_hash_;

  // On form submission, set to `true` if the field had a value on page load and
  // it was changed between page load and form submission. Set to `false` if the
  // pre-filled value wasn't changed. Not set if the field didn't have a
  // pre-filled value.
  // Set for <select> fields only if kAutofillFixInitialValueOfSelect is
  // enabled. Always set for <textarea> and <input>.
  std::optional<bool> initial_value_changed_;

  // Indicates if the value contained in the field was identified to potentially
  // contain sensitive data that should be handled with extra caution.
  // Note that the 'false' state does not guarantee that the data is not
  // sensitive, it just means that is wasn't identified as such yet.
  bool value_identified_as_potentially_sensitive_ = false;

  // Indicates if the field was determined to be eligable for prediction
  // improvements. The `nullopt` state implies that the eligibility has not been
  // determined yet.
  std::optional<bool> field_is_eligible_for_prediction_improvements_;

  // Used to hold the position of the first digit to be copied as a substring
  // from credit card number.
  size_t credit_card_number_offset_ = 0;

  // Whether the field was autofilled then later edited.
  bool previously_autofilled_ = false;

  // Whether the field should be filled when it is not the highlighted field.
  bool only_fill_when_focused_ = false;

  // The parseable name attribute, with unnecessary information removed (such as
  // a common prefix shared with other fields). Will be used for heuristics
  // parsing.
  std::u16string parseable_name_;

  // The parseable label attribute is potentially only a part of the original
  // label when the label is divided between subsequent fields.
  std::u16string parseable_label_;

  // The type of password generation event, if it happened.
  AutofillUploadContents::Field::PasswordGenerationType generation_type_ =
      AutofillUploadContents::Field::NO_GENERATION;

  // Whether the generated password was changed by user.
  bool generated_password_changed_ = false;

  // The vote type, if the autofill type is USERNAME or any password vote.
  // Otherwise, the field is ignored. |vote_type_| provides context as to what
  // triggered the vote.
  AutofillUploadContents::Field::VoteType vote_type_ =
      AutofillUploadContents::Field::NO_INFORMATION;

  // Denotes if |ADDRESS_HOME_STATE| should be added to |possible_types_|.
  bool state_is_a_matching_type_ = false;

  // Strength of the single username vote signal, if applicable.
  std::optional<AutofillUploadContents::Field::SingleUsernameVoteType>
      single_username_vote_type_;

  // If set to `kMostRecentCandidate`, the field is candidate for username
  // in Username First Flow and the field has no intermediate
  // fields (like OTP/Captcha) between the candidate and the password form.
  // If set to `kHasIntermediateValuesInBetween`, the field is candidate for
  // username in Username First Flow, but has intermediate fields between the
  // candidate and the password form.
  // If set to `kNotPartOfUsernameFirstFlow`, the field is not part of Username
  // First Flow.
  IsMostRecentSingleUsernameCandidate
      is_most_recent_single_username_candidate_ =
          IsMostRecentSingleUsernameCandidate::kNotPartOfUsernameFirstFlow;

  // A list of field log events, which record when user interacts the field
  // during autofill or editing, such as user clicks on the field, the
  // suggestion list is shown for the field, user accepts one suggestion to
  // fill the form and user edits the field.
  std::vector<FieldLogEventType> field_log_events_;

  // The autofill profile's GUID that was used for field filling. It corresponds
  // to the autofill profile's GUID for the last address filling value of the
  // field. nullopt means the field was never autofilled with address data.
  // Note: `is_autofilled` is true for autocompleted fields. So `is_autofilled`
  // is not a sufficient condition for `autofill_source_profile_guid_` to have a
  // value. This is not tracked for fields filled with field by field filling.
  // TODO(crbug.com/364937539): Use AutofillField::ProfileValueSource instead.
  std::optional<std::string> autofill_source_profile_guid_;

  // Denotes the type that was used to fill the field in its last autofill
  // operation. This is different from `overall_type_` because in some cases
  // Autofill might fallback to filling a classified field with a different type
  // than the classified one, based on country-specific rules.
  // This is not tracked for fields filled with field by field filling.
  // TODO(crbug.com/364937539): Use AutofillField::ProfileValueSource instead.
  std::optional<FieldType> autofilled_type_;

  // Denotes the product last responsible for filling the field. If the field is
  // autofilled, then it will correspond to the current filler, otherwise it
  // would correspond to the last filler of the field before the field became
  // not autofilled (due to user or JS edits). Note that this is not necessarily
  // tied to the field type, as some filling mechanisms are independent of the
  // field type (e.g. Autocomplete).
  FillingProduct filling_product_ = FillingProduct::kNone;

  // Denotes whether a user triggered suggestions from this field.
  bool did_trigger_suggestions_ = false;

  // True iff the field was ever focused.
  bool was_focused_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_
