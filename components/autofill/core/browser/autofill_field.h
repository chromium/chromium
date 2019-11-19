// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_

#include <stddef.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures_util.h"

namespace autofill {

typedef std::map<ServerFieldType, std::vector<AutofillDataModel::ValidityState>>
    ServerFieldTypeValidityStatesMap;

typedef std::map<ServerFieldType, AutofillDataModel::ValidityState>
    ServerFieldTypeValidityStateMap;

class AutofillField : public FormFieldData {
 public:
  enum PhonePart {
    IGNORED = 0,
    PHONE_PREFIX = 1,
    PHONE_SUFFIX = 2,
  };

  AutofillField();
  AutofillField(const FormFieldData& field, const base::string16& unique_name);
  virtual ~AutofillField();

  // Creates AutofillField that has bare minimum information for uploading
  // votes, namely a field signature. Warning: do not use for Autofill code,
  // since it is likely missing some fields.
  static std::unique_ptr<AutofillField> CreateForPasswordManagerUpload(
      FieldSignature field_signature);

  const base::string16& unique_name() const { return unique_name_; }

  ServerFieldType heuristic_type() const { return heuristic_type_; }
  ServerFieldType server_type() const { return server_type_; }
  const std::vector<AutofillQueryResponseContents::Field::FieldPrediction>&
  server_predictions() const {
    return server_predictions_;
  }
  HtmlFieldType html_type() const { return html_type_; }
  HtmlFieldMode html_mode() const { return html_mode_; }
  const ServerFieldTypeSet& possible_types() const { return possible_types_; }
  const ServerFieldTypeValidityStatesMap& possible_types_validities() const {
    return possible_types_validities_;
  }
  PhonePart phone_part() const { return phone_part_; }
  bool previously_autofilled() const { return previously_autofilled_; }
  const base::string16& parseable_name() const { return parseable_name_; }
  bool only_fill_when_focused() const { return only_fill_when_focused_; }

  // Setters for the detected types.
  void set_heuristic_type(ServerFieldType type);
  void set_server_type(ServerFieldType type);
  void add_possible_types_validities(
      const ServerFieldTypeValidityStateMap& possible_types_validities);
  void set_server_predictions(
      const std::vector<AutofillQueryResponseContents::Field::FieldPrediction>
          predictions) {
    server_predictions_ = std::move(predictions);
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
  void set_parseable_name(const base::string16& parseable_name) {
    parseable_name_ = parseable_name;
  }

  void set_only_fill_when_focused(bool fill_when_focused) {
    only_fill_when_focused_ = fill_when_focused;
  }

  // Set the type of the field. This sets the value returned by |Type|.
  // This function can be used to override the value that would be returned by
  // |ComputedType|.
  // As the |type| is expected to depend on |ComputedType|, the value will be
  // reset to |ComputedType| if some internal value change (e.g. on call to
  // (|set_heuristic_type| or |set_server_type|).
  // |SetTypeTo| cannot be called with
  // type.GetStoreableType() == NO_SERVER_DATA.
  void SetTypeTo(const AutofillType& type);

  // This function returns |ComputedType| unless the value has been overriden
  // by |SetTypeTo|.
  // (i.e. overall_type_ != NO_SERVER_DATA ? overall_type_ : ComputedType())
  AutofillType Type() const;

  // This function automatically chooses between server and heuristic autofill
  // type, depending on the data available for this field alone.
  // This type does not take into account the rationalization involving the
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

  void set_initial_value_hash(uint32_t value) { initial_value_hash_ = value; }
  base::Optional<uint32_t> initial_value_hash() { return initial_value_hash_; }

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
  const base::Optional<PasswordRequirementsSpec>& password_requirements()
      const {
    return password_requirements_;
  }

  // For each type in |possible_types_| that's missing from
  // |possible_types_validities_|, will add it to the
  // |possible_types_validities_| and will set its validity to UNVALIDATED. This
  // is to avoid inconsistencies between |possible_types_| and
  // |possible_types_validities_|. Used especially when the server validity map
  // is not available (is empty), and as a result the
  // |possible_types_validities_| would also be empty.
  void NormalizePossibleTypesValidities();

 private:
  explicit AutofillField(FieldSignature field_signature);

  // Whether the heuristics or server predict a credit card field.
  bool IsCreditCardPrediction() const;

  base::Optional<FieldSignature> field_signature_;
  // The unique name of this field, generated by Autofill.
  base::string16 unique_name_;

  // The type of the field, as determined by the Autofill server.
  ServerFieldType server_type_ = NO_SERVER_DATA;

  // The possible types of the field, as determined by the Autofill server,
  // including |server_type_| as the first item.
  std::vector<AutofillQueryResponseContents::Field::FieldPrediction>
      server_predictions_;

  // Requirements the site imposes to passwords (for password generation).
  // Corresponds to the requirements determined by the Autofill server.
  base::Optional<PasswordRequirementsSpec> password_requirements_;

  // The type of the field, as determined by the local heuristics.
  ServerFieldType heuristic_type_ = UNKNOWN_TYPE;

  // The type of the field. Overrides all other types (html_type_,
  // heuristic_type_, server_type_).
  // |AutofillType(NO_SERVER_DATA)| is used when this |overall_type_| has not
  // been set.
  AutofillType overall_type_;

  // The type of the field, as specified by the site author in HTML.
  HtmlFieldType html_type_ = HTML_TYPE_UNSPECIFIED;

  // The "mode" of the field, as specified by the site author in HTML.
  // Currently this is used to distinguish between billing and shipping fields.
  HtmlFieldMode html_mode_ = HTML_MODE_NONE;

  // The set of possible types for this field.
  ServerFieldTypeSet possible_types_;

  // The set of possible types and their validity for this field.
  ServerFieldTypeValidityStatesMap possible_types_validities_;

  // Used to track whether this field is a phone prefix or suffix.
  PhonePart phone_part_ = IGNORED;

  // A low-entropy hash of the field's initial value before user-interactions or
  // automatic fillings. This field is used to detect static placeholders.
  base::Optional<uint32_t> initial_value_hash_;

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
  base::string16 parseable_name_;

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

  DISALLOW_COPY_AND_ASSIGN(AutofillField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_
