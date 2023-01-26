// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_

#include <stddef.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

typedef std::map<ServerFieldType, std::vector<AutofillDataModel::ValidityState>>
    ServerFieldTypeValidityStatesMap;

typedef std::map<ServerFieldType, AutofillDataModel::ValidityState>
    ServerFieldTypeValidityStateMap;

class AutofillField : public FormFieldData {
 public:
  using FieldLogEventType = absl::variant<absl::monostate,
                                          AskForValuesToFillFieldLogEvent,
                                          TriggerFillFieldLogEvent,
                                          FillFieldLogEvent,
                                          TypingFieldLogEvent,
                                          HeuristicPredictionFieldLogEvent,
                                          AutocompleteAttributeFieldLogEvent,
                                          ServerPredictionFieldLogEvent>;

  AutofillField();
  explicit AutofillField(const FormFieldData& field);

  AutofillField(const AutofillField&) = delete;
  AutofillField& operator=(const AutofillField&) = delete;

  virtual ~AutofillField();

  // Creates AutofillField that has bare minimum information for uploading
  // votes, namely a field signature. Warning: do not use for Autofill code,
  // since it is likely missing some fields.
  static std::unique_ptr<AutofillField> CreateForPasswordManagerUpload(
      FieldSignature field_signature);

  ServerFieldType heuristic_type() const;
  ServerFieldType heuristic_type(PatternSource s) const;
  ServerFieldType server_type() const;
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
  bool may_use_prefilled_placeholder() const {
    return may_use_prefilled_placeholder_;
  }
  HtmlFieldType html_type() const { return html_type_; }
  HtmlFieldMode html_mode() const { return html_mode_; }
  const ServerFieldTypeSet& possible_types() const { return possible_types_; }
  const ServerFieldTypeValidityStatesMap& possible_types_validities() const {
    return possible_types_validities_;
  }
  bool previously_autofilled() const { return previously_autofilled_; }
  const std::u16string& parseable_name() const { return parseable_name_; }
  const std::u16string& parseable_label() const { return parseable_label_; }
  bool only_fill_when_focused() const { return only_fill_when_focused_; }

  // Setters for the detected types.
  void set_heuristic_type(PatternSource s, ServerFieldType t);
  void add_possible_types_validities(
      const ServerFieldTypeValidityStateMap& possible_types_validities);
  void set_server_predictions(
      std::vector<AutofillQueryResponse::FormSuggestion::FieldSuggestion::
                      FieldPrediction> predictions);

  void set_may_use_prefilled_placeholder(bool may_use_prefilled_placeholder) {
    may_use_prefilled_placeholder_ = may_use_prefilled_placeholder;
  }
  void set_possible_types(const ServerFieldTypeSet& possible_types) {
    possible_types_ = possible_types;
  }
  void set_possible_types_validities(
      const ServerFieldTypeValidityStatesMap& possible_types_validities) {
    possible_types_validities_ = possible_types_validities;
  }
  std::vector<AutofillDataModel::ValidityState>
      get_validities_for_possible_type(ServerFieldType);

  void SetHtmlType(HtmlFieldType type, HtmlFieldMode mode);

  void set_previously_autofilled(bool previously_autofilled) {
    previously_autofilled_ = previously_autofilled;
  }
  void set_parseable_name(const std::u16string& parseable_name) {
    parseable_name_ = parseable_name;
  }
  void set_parseable_label(const std::u16string& parseable_label) {
    parseable_label_ = parseable_label;
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

  // Returns true if the value of this field is empty.
  bool IsEmpty() const;

  // The unique signature of this field, composed of the field name and the html
  // input type in a 32-bit hash.
  FieldSignature GetFieldSignature() const;

  // Returns the field signature as string.
  std::string FieldSignatureAsStr() const;

  // Returns true if the field type has been determined (without the text in the
  // field).
  bool IsFieldFillable() const;

  // Address Autofill gets disabled by an unrecognized autocomplete attribute.
  // If `kAutofillFillAndImportFromMoreFields` is enabled, this changes and the
  // server/heuristic predictions overwrite the unrecognized autocomplete
  // attribute. Depending on the feature's parameters, Autofill then fills or
  // imports from these fields.
  // This function returns true if the field's type prediction is only available
  // due to the aforementioned feature.
  bool HasPredictionDespiteUnrecognizedAutocompleteAttribute() const;

  void set_initial_value_hash(uint32_t value) { initial_value_hash_ = value; }
  absl::optional<uint32_t> initial_value_hash() { return initial_value_hash_; }

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
  const absl::optional<PasswordRequirementsSpec>& password_requirements()
      const {
    return password_requirements_;
  }

  // Getter and Setter methods for |state_is_a_matching_type_|.
  void set_state_is_a_matching_type(bool value = true) {
    state_is_a_matching_type_ = value;
  }
  const bool& state_is_a_matching_type() const {
    return state_is_a_matching_type_;
  }

  void set_single_username_vote_type(
      AutofillUploadContents::Field::SingleUsernameVoteType vote_type) {
    single_username_vote_type_ = vote_type;
  }
  absl::optional<AutofillUploadContents::Field::SingleUsernameVoteType>
  single_username_vote_type() const {
    return single_username_vote_type_;
  }

  // Getter and Setter methods for
  // |value_not_autofilled_over_existing_value_hash_|.
  void set_value_not_autofilled_over_existing_value_hash(
      absl::optional<size_t> value_not_autofilled_over_existing_value_hash) {
    value_not_autofilled_over_existing_value_hash_ =
        value_not_autofilled_over_existing_value_hash;
  }
  absl::optional<size_t> value_not_autofilled_over_existing_value_hash() const {
    return value_not_autofilled_over_existing_value_hash_;
  }

  // For each type in |possible_types_| that's missing from
  // |possible_types_validities_|, will add it to the
  // |possible_types_validities_| and will set its validity to UNVALIDATED. This
  // is to avoid inconsistencies between |possible_types_| and
  // |possible_types_validities_|. Used especially when the server validity map
  // is not available (is empty), and as a result the
  // |possible_types_validities_| would also be empty.
  void NormalizePossibleTypesValidities();

  bool was_context_menu_shown() const { return was_context_menu_shown_; }
  void set_was_context_menu_shown(bool was_context_menu_shown) {
    was_context_menu_shown_ = was_context_menu_shown;
  }

  void set_field_log_events(const std::vector<FieldLogEventType>& events) {
    field_log_events_ = events;
  }

  const std::vector<FieldLogEventType>& field_log_events() const {
    return field_log_events_;
  }

  // Add the field log events into the vector |field_log_events_| when it is
  // not the same as the last log event in the vector.
  void AppendLogEventIfNotRepeated(const FieldLogEventType& log_event);

  // Clear all the log events for this field.
  void ClearLogEvents() { field_log_events_.clear(); }

 private:
  explicit AutofillField(FieldSignature field_signature);

  // Whether the heuristics or server predict a credit card field.
  bool IsCreditCardPrediction() const;

  absl::optional<FieldSignature> field_signature_;

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
  bool may_use_prefilled_placeholder_ = false;

  // Requirements the site imposes to passwords (for password generation).
  // Corresponds to the requirements determined by the Autofill server.
  absl::optional<PasswordRequirementsSpec> password_requirements_;

  // Predictions which where calculated on the client. This is initialized to
  // `NO_SERVER_DATA`, which means "NO_DATA", i.e. no classification was
  // attempted.
  std::array<ServerFieldType, static_cast<size_t>(PatternSource::kMaxValue) + 1>
      local_type_predictions_;

  // The type of the field. Overrides all other types (html_type_,
  // heuristic_type_).
  // |AutofillType(NO_SERVER_DATA)| is used when this |overall_type_| has not
  // been set.
  AutofillType overall_type_;

  // The type of the field, as specified by the site author in HTML.
  HtmlFieldType html_type_ = HtmlFieldType::kUnspecified;

  // The "mode" of the field, as specified by the site author in HTML.
  // Currently this is used to distinguish between billing and shipping fields.
  HtmlFieldMode html_mode_ = HtmlFieldMode::kNone;

  // The set of possible types for this field.
  ServerFieldTypeSet possible_types_;

  // The set of possible types and their validity for this field.
  ServerFieldTypeValidityStatesMap possible_types_validities_;

  // A low-entropy hash of the field's initial value before user-interactions or
  // automatic fillings. This field is used to detect static placeholders.
  absl::optional<uint32_t> initial_value_hash_;

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
  absl::optional<AutofillUploadContents::Field::SingleUsernameVoteType>
      single_username_vote_type_;

  // Stores the hash of the value which is supposed to be autofilled in the
  // field but was not due to a prefilled value.
  absl::optional<size_t> value_not_autofilled_over_existing_value_hash_;

  // Set to true if the context menu was triggered and shown on the field.
  bool was_context_menu_shown_ = false;

  // A list of field log events, which record when user interacts the field
  // during autofill or editing, such as user clicks on the field, the
  // suggestion list is shown for the field, user accepts one suggestion to
  // fill the form and user edits the field.
  std::vector<FieldLogEventType> field_log_events_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_
