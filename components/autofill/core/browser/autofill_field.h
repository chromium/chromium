// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_

#include <stddef.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

class AutofillQueryResponse_FormSuggestion_FieldSuggestion_FieldPrediction;
class FormAutofillHistory;
class FormFiller;

enum FormatString_Type : int;

using FieldPrediction =
    AutofillQueryResponse_FormSuggestion_FieldSuggestion_FieldPrediction;

// LINT.IfChange(AutofillPredictionSource)

// Values are persisted in UMA logs, values should not be reused/renumbered.
// Enum representing prediction sources that are recognized.
enum class AutofillPredictionSource {
  kServerCrowdsourcing = 0,
  kServerOverride = 1,
  kHeuristics = 2,
  kAutocomplete = 3,
  kRationalization = 4,
  kMaxValue = kRationalization
};

// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillPredictionSource2)

std::string_view AutofillPredictionSourceToStringView(
    AutofillPredictionSource source);

// Stores information about the section of the field.
class Section {
 public:
  struct Autocomplete {
    friend auto operator<=>(const Autocomplete& lhs,
                            const Autocomplete& rhs) = default;
    friend bool operator==(const Autocomplete& lhs,
                           const Autocomplete& rhs) = default;

    std::string section;
    HtmlFieldMode mode = HtmlFieldMode::kNone;
  };

  using Default = std::monostate;

  struct FieldIdentifier {
    FieldIdentifier() = default;
    FieldIdentifier(std::string field_name,
                    size_t local_frame_id,
                    FieldRendererId field_renderer_id)
        : field_name(std::move(field_name)),
          local_frame_id(local_frame_id),
          field_renderer_id(field_renderer_id) {}

    friend auto operator<=>(const FieldIdentifier& lhs,
                            const FieldIdentifier& rhs) = default;
    friend bool operator==(const FieldIdentifier& lhs,
                           const FieldIdentifier& rhs) = default;

    std::string field_name;
    size_t local_frame_id;
    FieldRendererId field_renderer_id;
  };

  static Section FromAutocomplete(Autocomplete autocomplete);
  static Section FromFieldIdentifier(
      const FormFieldData& field,
      base::flat_map<LocalFrameToken, size_t>& frame_token_ids);

  Section();
  Section(const Section& section);
  Section& operator=(const Section& section);
  Section(Section&& section);
  Section& operator=(Section&& section);
  ~Section();

  friend auto operator<=>(const Section& lhs, const Section& rhs) = default;
  friend bool operator==(const Section& lhs, const Section& rhs) = default;
  explicit operator bool() const;

  bool is_from_autocomplete() const;
  bool is_from_fieldidentifier() const;
  bool is_default() const;

  // Reconstructs `this` to a string. The string representation of the section
  // is used in the renderer.
  // TODO(crbug.com/40200532): Remove when fixed.
  std::string ToString() const;

 private:
  // Represents the section's origin:
  //  - `Default` is the empty, initial value before running any sectioning
  //     algorithm,
  //  - `Autocomplete` represents a section derived from the autocomplete
  //     attribute,
  //  - `FieldIdentifier` represents a section generated based on the first
  //     field in the section.
  using SectionValue = std::variant<Default, Autocomplete, FieldIdentifier>;

  SectionValue value_;
};

LogBuffer& operator<<(LogBuffer& buffer, const Section& section);
std::ostream& operator<<(std::ostream& os, const Section& section);

// Describes formatting information for a field. Currently used only for
// filling Autofill AI data.
//
// Currently, the following kinds of format stings are supported:
// - Affix format strings: data_util::IsValidAffixFormat().
// - Date format strings: data_util::IsValidDateFormat().
// - Date format strings: ICU format.
// - Flight number format strings (data_util::IsValidFlightNumberFormat().
struct AutofillFormatString final {
  AutofillFormatString();
  AutofillFormatString(std::u16string value, FormatString_Type type);

  AutofillFormatString(const AutofillFormatString&);
  AutofillFormatString& operator=(const AutofillFormatString&);
  AutofillFormatString(AutofillFormatString&&);
  AutofillFormatString& operator=(AutofillFormatString&&);
  ~AutofillFormatString();

  static bool IsValid(std::u16string_view value, FormatString_Type type);

  friend bool operator==(const AutofillFormatString&,
                         const AutofillFormatString&) = default;

  // The actual format string.
  std::u16string value;

  // Format strings can have different types: They can specify a date
  // format, an affix format, etc. See `FormatString_Type` for allowed values.
  FormatString_Type type{};
};

// The ordering matters: higher values overrule lower values (e.g., kServer
// overrules kHeuristics).
enum class AutofillFormatStringSource {
  kUnset = 0,        // No format string set.
  kHeuristics = 1,   // Set by local heuristics.
  kModelResult = 2,  // Set by a direct model response
  kServer = 3,       // Set by an (Autofill) server response.
};

// Defines the way a field's value was modified.
enum class FieldModifier {
  kUser = 0,
  kAutofill = 1,
  kMaxValue = kAutofill,
};

class AutofillField : public FormFieldData {
 public:
  using FieldLogEventType = std::variant<std::monostate,
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

  // This is deprecated, consider using `AutofillField::field_modifiers_`
  // instead.
  // TODO(crbug.com/393114125): Remove this getter after launching
  // `AutofillFixIsAutofilled`.
  bool is_autofilled_deprecated(base::PassKey<FormStructure> pass_key) const {
    return is_autofilled_according_to_renderer();
  }

  // This is deprecated, consider using `AutofillField::AddFieldModifier()`
  // instead.
  // TODO(crbug.com/393114125): Remove this setter after launching
  // `AutofillFixIsAutofilled`.
  void set_is_autofilled_deprecated(
      bool is_autofilled_deprecated,
      base::PassKey<FormStructure, FormFiller> pass_key) {
    set_is_autofilled_according_to_renderer(is_autofilled_deprecated);
  }

  // The unique identifier of the section (e.g. billing vs. shipping address)
  // of this field.
  const Section& section() const { return section_; }
  void set_section(Section section) { section_ = std::move(section); }

  FieldType heuristic_type() const;
  FieldType heuristic_type(HeuristicSource s) const;
  FieldType server_type() const;
  bool server_type_prediction_is_override() const;
  const std::vector<FieldPrediction>& server_predictions() const {
    return server_predictions_;
  }

  HtmlFieldType html_type() const { return html_type_; }
  HtmlFieldMode html_mode() const { return html_mode_; }
  const FieldTypeSet& possible_types() const { return possible_types_; }
  bool only_fill_when_focused() const { return only_fill_when_focused_; }

  void set_heuristic_type(HeuristicSource s, FieldType t);

  // Sets the server predictions to `predictions` after performing some
  // filtering. If `predictions` is empty, it creates a `NO_SERVER_DATA`
  // prediction.
  void set_server_predictions(std::vector<FieldPrediction> predictions);
  // Adds `prediction` to the back of the existing `server_predictions_` if
  // the prediction's source passes various validity checks. If the only
  // existing server prediction is an empty one, it replaces that one.
  void MaybeAddServerPrediction(FieldPrediction prediction);

  void set_possible_types(const FieldTypeSet& possible_types) {
    possible_types_ = possible_types;
  }

  void SetHtmlType(HtmlFieldType type, HtmlFieldMode mode);

  // This is deprecated. Please use `AutofillField::AddFieldModifier()` instead.
  void set_previously_autofilled_deprecated(
      bool previously_autofilled_deprecated) {
    previously_autofilled_deprecated_ = previously_autofilled_deprecated;
  }
  // This is deprecated. Please use `AutofillField::all_modifiers()` instead.
  bool previously_autofilled_deprecated() const {
    return previously_autofilled_deprecated_;
  }

  // This is deprecated. Please use `AutofillField::AddFieldModifier()` instead.
  void set_is_user_edited_deprecated(bool is_user_edited_deprecated) {
    is_user_edited_deprecated_ = is_user_edited_deprecated;
  }
  // This is deprecated. Please use `AutofillField::all_modifiers()` instead.
  bool is_user_edited_deprecated() const { return is_user_edited_deprecated_; }

  // Returns all the modifiers to have acted on the field, in no particular
  // order.
  DenseSet<FieldModifier> all_modifiers() const;
  // Returns the last `FieldModifier` to have acted on the field.
  std::optional<FieldModifier> last_modifier() const;
  // Adds `modifier` as the most recent field modifier.
  void AddFieldModifier(FieldModifier modifier);
  void RemoveFieldModifier(FieldModifier modifier,
                           base::PassKey<FormFiller> pass_key);

  // TODO(crbug.com/456719060): Remove `FormStructure` from the `pass_key` of
  // both functions below after launching `kAutofillOptimizeCacheUpdates`.
  const std::vector<FieldModifier>& field_modifiers(
      base::PassKey<FormStructure, FormAutofillHistory>) const {
    return field_modifiers_;
  }
  void set_field_modifiers(std::vector<FieldModifier> field_modifiers,
                           base::PassKey<FormStructure, FormFiller>) {
    field_modifiers_ = std::move(field_modifiers);
  }

  void set_only_fill_when_focused(bool fill_when_focused) {
    only_fill_when_focused_ = fill_when_focused;
  }

  // Set the type of the field. This sets the value returned by Type().
  // This function can be used to override the value that would be returned by
  // ComputedType().
  // As the `type` is expected to depend on ComputedType(), the value will be
  // reset to ComputedType() if some internal value change (e.g. on call to
  // (set_heuristic_type()).
  // SetTypeTo() must not be called with `type.GetTypes().empty()`.
  void SetTypeTo(const AutofillType& type,
                 std::optional<AutofillPredictionSource> source);

  // The type of `GetOverallPredictionResult()`.
  AutofillType Type() const;

  // The prediction source of `GetOverallPredictionResult()`.
  // Note that if no prediction was made by any source, PredictionSource will be
  // std::nullopt. Type() would return UNKNOWN_TYPE in such a case.
  std::optional<AutofillPredictionSource> PredictionSource() const;

  // The type of `GetComputedPredictionResult()`.
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
  // Fields with `autocomplete=unrecognized` receive a predictions, but
  // suggestions and filling are suppressed on Desktop. This function can be
  // used to determine whether suggestions and filling should be suppressed for
  // this field (independently of the predicted type).
  // `ac_unrecognized_behavior` describes the general behavior (as per
  // `AutofillClient`) whether fields with unrecognized autocomplete value can
  // have suppressed suggestions in general. The concrete behavior is influenced
  // by the concrete `AutofillField` and the operating system.
  bool ShouldSuppressSuggestionsAndFillingByDefault(
      AutocompleteUnrecognizedBehavior ac_unrecognized_behavior) const;

  // Returns the current value, formatted as desired for import:
  // (1) If the field value hasn't changed since it was seen and the field is a
  //     non-<select> (except for ADDRESS_HOME_{STATE,COUNTRY}), returns the
  //     empty string.
  // (2) If the field has FormControlType::kSelect* and has a selected option,
  //     returns that option's human-readable text.
  // (3) Otherwise returns value().
  //
  // The motivation behind (1) is that unchanged values usually carry little
  // value for importing. <select> fields are exempted because their default
  // value is often correct (e.g., in ADDRESS_HOME_COUNTRY fields).
  // ADDRESS_HOME_{STATE,COUNTRY} fields are also exempted because the prefilled
  // values are often correct (e.g. determinable via GeoIP).
  //
  // The motivation behind (2) is that the human-readable text of an <option> is
  // usually better suited for import than the its value. See the documentation
  // of FormFieldData::value() and FormFieldData::selected_text() for further
  // details.
  const std::u16string& value_for_import() const;

  // Returns the value the field had when it was first seen by the
  // AutofillManager. For fields that exist on page load, this is typically the
  // value on page load.
  //
  // There are some special cases where the above does not apply, such as:
  // - When the field has moved to another form.
  // - When the form has been extracted without the field. For example, this
  //   could happen because the field was temporarily removed from the DOM.
  //
  // For the field's current value, see FormFieldData::value().
  const std::u16string& initial_value() const { return initial_value_; }

  // Sets the field's initial value.
  void set_initial_value(std::u16string initial_value,
                         base::PassKey<FormStructure> pass_key) {
    initial_value_ = std::move(initial_value);
  }

  void set_credit_card_number_offset(size_t position) {
    credit_card_number_offset_ = position;
  }
  size_t credit_card_number_offset() const {
    return credit_card_number_offset_;
  }

  void SetPasswordRequirements(PasswordRequirementsSpec spec);
  const std::optional<PasswordRequirementsSpec>& password_requirements() const {
    return password_requirements_;
  }

  // The format of the value expected by the web document.
  //
  // Only one format string is stored at a time: the one with the
  // highest-ranking `AutofillFormatString::Source`.
  //
  // The server currently does not predict ICU-format dates
  // (`FormatString_Type_ICU_DATE`).
  base::optional_ref<const AutofillFormatString> format_string() const
      LIFETIME_BOUND;

  AutofillFormatStringSource format_string_source() const {
    return format_string_source_;
  }

  void set_format_string_unless_overruled(AutofillFormatString format_string,
                                          AutofillFormatStringSource source) {
    if (format_string_source_ <= source) {
      format_string_ = std::move(format_string);
      format_string_source_ = source;
    }
  }

  void set_field_log_events(const std::vector<FieldLogEventType>& events) {
    field_log_events_ = events;
  }

  const std::vector<FieldLogEventType>& field_log_events() const {
    static const std::vector<FieldLogEventType> empty_vector{};
    return field_log_events_ ? *field_log_events_ : empty_vector;
  }

  // Avoid holding references to the return value. It is invalidated by
  // AppendLogEventIfNotRepeated().
  base::optional_ref<FieldLogEventType> last_field_log_event() {
    if (field_log_events_ && !field_log_events_->empty()) {
      return field_log_events_->back();
    }
    return std::nullopt;
  }

  // Add the field log events into the vector |field_log_events_| when it is
  // not the same as the last log event in the vector.
  void AppendLogEventIfNotRepeated(const FieldLogEventType& log_event);

  // Clear all the log events for this field.
  void ClearLogEvents() {
    if (field_log_events_) {
      field_log_events_->clear();
    }
  }

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

  void set_ml_supported_types(const FieldTypeSet& s) {
    ml_supported_types_ = s;
  }

  const std::optional<FieldTypeSet>& ml_supported_types() const {
    return ml_supported_types_;
  }

  void UpdateFieldData(const FormFieldData& field_data,
                       base::PassKey<FormStructure> pass_key) {
    UpdateFieldData(field_data);
  }

#if defined(UNIT_TEST)
  const std::array<FieldType,
                   static_cast<size_t>(HeuristicSource::kMaxValue) + 1>&
  local_type_predictions() const {
    return local_type_predictions_;
  }
#endif

 private:
  friend class AutofillFieldTestApi;

  using FormFieldData::is_autofilled_according_to_renderer;
  using FormFieldData::set_is_autofilled_according_to_renderer;

  struct PredictionResult {
    // The type may be a union type, i.e., hold multiple FieldTypes.
    AutofillType type;
    // The source of the primary FieldType in `type`: If there are multiple
    // FieldTypes in `type`, the source only refers to the non-Autofill AI
    // types.
    // TODO(crbug.com/432645177): Make the FieldType to which the `source`
    // applies explicit?
    std::optional<AutofillPredictionSource> source;
  };

  explicit AutofillField(FieldSignature field_signature);

  // Copies the information from `field_data` into the members of
  // `AutofillField` that were inherited from `FormFieldData`.
  void UpdateFieldData(const FormFieldData& field_data);

  // Whether the heuristics or server predict a credit card field.
  bool IsCreditCardPrediction() const;

  // Creates a union type that contains
  // - the `primary_field_type` and
  // - the Autofill AI FieldTypes in the `server_predictions_` (modulo conflict
  //   resolution).
  //
  // A union type is an AutofillType that holds multiple FieldType.
  // See AutofillType for details.
  //
  // TODO(crbug.com/436013479): Remove the hack that represents country codes.
  AutofillType MakeAutofillType(FieldType primary_field_type,
                                bool is_country_code = false) const;

  // Combines the server, heuristic and HTML type based predictions. Doesn't
  // take server overwrites or rationalization into consideration.
  PredictionResult GetComputedPredictionResult() const;

  // Returns the GetComputedPredictionResult(), unless there is a server
  // overwrite or the result was overwritten using `SetTypeTo()`.
  PredictionResult GetOverallPredictionResult() const;

  Section section_;

  std::optional<FieldSignature> field_signature_;

  size_t rank_ = 0;
  size_t rank_in_signature_group_ = 0;
  size_t rank_in_host_form_ = 0;
  size_t rank_in_host_form_signature_group_ = 0;

  // The possible types of the field, as determined by the Autofill server.
  std::vector<FieldPrediction> server_predictions_;

  // Requirements the site imposes to passwords (for password generation).
  // Corresponds to the requirements determined by the Autofill server.
  std::optional<PasswordRequirementsSpec> password_requirements_;

  AutofillFormatString format_string_;
  AutofillFormatStringSource format_string_source_ =
      AutofillFormatStringSource::kUnset;

  // Predictions which where calculated on the client. This is initialized to
  // `NO_SERVER_DATA`, which means "NO_DATA", i.e. no classification was
  // attempted.
  std::array<FieldType, static_cast<size_t>(HeuristicSource::kMaxValue) + 1>
      local_type_predictions_;

  // The rationalized `GetComputedPredictionResult()`. This is the type used for
  // all autofilling operations. It defaults to `GetComputedPredictionResult()`
  // and is invalidated when `set_heuristic_type()`, `set_server_predictions()`
  // or `SetHtmlType()` are called. Rationalization potentially overwrites it
  // using `SetTypeTo()`. The result is cached to prevent frequent re-evaluation
  // of `GetComputedPredictionResult()`.
  // Nullopt if no result is cached. If it has a value, the type is guaranteed
  // to be different from NO_SERVER_DATA.
  mutable std::optional<PredictionResult> overall_type_;

  // The type of the field, as specified by the site author in HTML.
  HtmlFieldType html_type_ = HtmlFieldType::kUnspecified;

  // The "mode" of the field, as specified by the site author in HTML.
  // Currently this is used to distinguish between billing and shipping fields.
  HtmlFieldMode html_mode_ = HtmlFieldMode::kNone;

  // The set of possible types for this field. It is normally only populated on
  // submission time.
  FieldTypeSet possible_types_;

  // The field's initial value. Initially, it is the same as
  // `FormFieldData::value()`, but unlike value(), it remains unchanged over
  // time.
  std::u16string initial_value_;

  // Used to hold the position of the first digit to be copied as a substring
  // from credit card number.
  size_t credit_card_number_offset_ = 0;

  // Whether the field was autofilled then later edited.
  // TODO(crbug.com/393114125): Remove after fully launching
  // `AutofillField::field_modifiers_`.
  bool previously_autofilled_deprecated_ = false;

  // This is deprecated, please use `field_modifiers_` instead.
  // Whether or not the user edited the field. It corresponds to whether the
  // field is seen by `AutofillManager` in an `OnTextFieldValueChanged()`
  // signal.
  bool is_user_edited_deprecated_ = false;

  // Tracks the relative order of all the modifiers of the field. Each
  // `FieldModifier` value is present at most once in the list, and the order of
  // the list depends on the order of events that modified the field's value.
  std::vector<FieldModifier> field_modifiers_;

  // Whether the field should be filled when it is not the highlighted field.
  bool only_fill_when_focused_ = false;

  // A list of field log events, which record when user interacts the field
  // during autofill or editing, such as user clicks on the field, the
  // suggestion list is shown for the field, user accepts one suggestion to
  // fill the form and user edits the field.
  std::optional<std::vector<FieldLogEventType>> field_log_events_ =
      std::vector<FieldLogEventType>{};

  // The autofill profile's GUID that was used for field filling. It corresponds
  // to the autofill profile's GUID for the last address filling value of the
  // field. nullopt means the field was never autofilled with address data.
  std::optional<std::string> autofill_source_profile_guid_;

  // Denotes the type that was used to fill the field in its last autofill
  // operation. This is different from `overall_type_` because in some cases
  // Autofill might fallback to filling a classified field with a different type
  // than the classified one, based on country-specific rules.
  // This is not tracked for fields filled with field by field filling.
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

  // Field types that the ML model is able to output.
  // Assigned by the model when it has classified the field.
  std::optional<FieldTypeSet> ml_supported_types_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_H_
