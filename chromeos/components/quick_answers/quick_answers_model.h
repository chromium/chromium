// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace quick_answers {

// Interaction with the consent-view (used for logging).
enum class NoticeInteractionType {
  // When user clicks on the "grant-consent" button.
  kAccept = 0,
  // When user clicks on the "manage-settings" button.
  kManageSettings = 1,
  // When user otherwise dismisses or ignores the consent-view.
  kDismiss = 2
};

// The status of loading quick answers.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |QuickAnswersLoadStatus|.
enum class LoadStatus {
  kSuccess = 0,
  kNetworkError = 1,
  kNoResult = 2,
  kMaxValue = kNoResult,
};

// The type of the result. Valid values are map to the search result types.
// Please see go/1ns-doc for more detail.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |QuickAnswersResultType|.
enum class ResultType {
  kNoResult = 0,
  kDefinitionResult = 5493,
  kTranslationResult = 6613,
  kUnitConversionResult = 13668,
};

// The predicted intent of the request.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |QuickAnswersIntentType|.
enum class IntentType {
  kUnknown = 0,
  kUnit = 1,
  kDictionary = 2,
  kTranslation = 3,
  kMaxValue = kTranslation
};

std::optional<quick_answers::Intent> ToIntent(IntentType intent_type);

enum class QuickAnswerUiElementType {
  kUnknown = 0,
  kText = 1,
  kImage = 2,
};

// Enumeration of Quick Answers exit points. These values are persisted to logs.
// Entries should never be renumbered and numeric values should never be reused.
// Append to this enum is allowed only if the possible exit point grows.
enum class QuickAnswersExitPoint {
  // The exit point is unspecified. Might be used by tests, obsolete code or as
  // placeholders.
  kUnspecified = 0,
  kContextMenuDismiss = 1,
  kContextMenuClick = 2,
  kQuickAnswersClick = 3,
  kSettingsButtonClick = 4,
  kReportQueryButtonClick = 5,
  kMaxValue = kReportQueryButtonClick,
};

struct QuickAnswerUiElement {
  explicit QuickAnswerUiElement(QuickAnswerUiElementType type) : type(type) {}
  QuickAnswerUiElement(const QuickAnswerUiElement&) = default;
  QuickAnswerUiElement& operator=(const QuickAnswerUiElement&) = default;
  QuickAnswerUiElement(QuickAnswerUiElement&&) = default;
  virtual ~QuickAnswerUiElement() = default;

  QuickAnswerUiElementType type = QuickAnswerUiElementType::kUnknown;
};

// Class to describe an answer text.
struct QuickAnswerText : public QuickAnswerUiElement {
  explicit QuickAnswerText(const std::string& text,
                           ui::ColorId color_id = ui::kColorLabelForeground)
      : QuickAnswerUiElement(QuickAnswerUiElementType::kText),
        text(base::UTF8ToUTF16(text)),
        color_id(color_id) {}

  std::u16string text;

  // Attributes for text style.
  ui::ColorId color_id;
};

struct QuickAnswerResultText : public QuickAnswerText {
 public:
  QuickAnswerResultText(
      const std::string& text,
      ui::ColorId color_id = ui::kColorLabelForegroundSecondary)
      : QuickAnswerText(text, color_id) {}
};

struct QuickAnswerImage : public QuickAnswerUiElement {
  explicit QuickAnswerImage(const gfx::Image& image)
      : QuickAnswerUiElement(QuickAnswerUiElementType::kImage), image(image) {}

  gfx::Image image;
};

// Class to describe quick answers phonetics info.
struct PhoneticsInfo {
 public:
  PhoneticsInfo();
  PhoneticsInfo(const PhoneticsInfo&);
  ~PhoneticsInfo();

  // Pronunciation of a word, i.e. in phonetic symbols.
  std::string text;

  // Phonetics audio URL for playing pronunciation of dictionary results.
  // For other type of results the URL will be empty.
  GURL phonetics_audio;

  // Set to true if tts audio (`query_text` and `locale`) can be used.
  // TODO(b/346794579): remove this field.
  bool tts_audio_enabled = false;

  // Query text and locale which will be used for tts if enabled and
  // there is no phonetics audio available.
  std::string query_text;
  std::string locale;

  bool PhoneticsInfoAvailable() const;
  bool AudioUrlAvailable() const;
  bool TtsAudioAvailable() const;
};

// Structure to describe a quick answer.
struct QuickAnswer {
  QuickAnswer();
  ~QuickAnswer();

  ResultType result_type = ResultType::kNoResult;
  std::vector<std::unique_ptr<QuickAnswerUiElement>> title;
  std::vector<std::unique_ptr<QuickAnswerUiElement>> first_answer_row;
  std::unique_ptr<QuickAnswerImage> image;

  PhoneticsInfo phonetics_info;
};

// Information of the device that used by the user to send the request.
struct DeviceProperties {
  // Whether the request is send by an internal user.
  bool is_internal = false;
};

struct IntentInfo {
  IntentInfo();
  IntentInfo(const IntentInfo& other);
  IntentInfo(const std::string& intent_text,
             IntentType intent_type,
             const std::string& device_language = std::string(),
             const std::string& source_language = std::string());
  ~IntentInfo();

  // The text extracted from the selected_text associated with the intent.
  std::string intent_text;

  // Predicted intent.
  IntentType intent_type = IntentType::kUnknown;

  // Device language code.
  std::string device_language;

  // Source language for definition or translation query, should only be used
  // for definition or translation intents.
  std::string source_language;
};

// Extract information generated from |QuickAnswersRequest|.
struct PreprocessedOutput {
  PreprocessedOutput();
  PreprocessedOutput(const PreprocessedOutput& other);
  ~PreprocessedOutput();

  IntentInfo intent_info;

  // Rewritten query based on |intent_type| and |intent_text|.
  std::string query;
};

// Structure of quick answers request context, including device properties and
// surrounding text.
struct Context {
  // Device specific properties.
  DeviceProperties device_properties;

  std::string surrounding_text;
};

// Structure to describe an quick answer request including selected content and
// context.
struct QuickAnswersRequest {
  QuickAnswersRequest();
  QuickAnswersRequest(const QuickAnswersRequest& other);
  ~QuickAnswersRequest();

  // The selected text.
  std::string selected_text;

  // Output of processed result.
  PreprocessedOutput preprocessed_output;

  // Context information.
  Context context;

  // TODO(b/169346016): Add context and other targeted objects (e.g: images,
  // links, etc).
};

// `Sense` must be copyable as a member of `DefinitionResult`.
struct Sense {
 public:
  Sense();
  Sense(const Sense& other);
  Sense& operator=(const Sense& other);
  ~Sense();

  std::string definition;
  // Not every word sense will have a sample sentence or synonyms.
  std::optional<std::string> sample_sentence;
  std::optional<std::vector<std::string>> synonyms_list;
};

// `DefinitionResult` holds result for definition intent.
// `DefinitionResult` must be copyable.
struct DefinitionResult {
 public:
  DefinitionResult();
  DefinitionResult(const DefinitionResult& other);
  DefinitionResult& operator=(const DefinitionResult& other);
  ~DefinitionResult();

  std::string word;
  std::string word_class;
  PhoneticsInfo phonetics_info;
  Sense sense;
  // Not every word will have subsenses.
  std::optional<std::vector<Sense>> subsenses_list;
};

// `TranslationResult` holds result for translation intent.
// `TranslationResult` must be copyable.
struct TranslationResult {
 public:
  TranslationResult();
  TranslationResult(const TranslationResult& other);
  TranslationResult& operator=(const TranslationResult& other);
  ~TranslationResult();

  std::string text_to_translate;
  std::string translated_text;
  std::string source_locale;
  std::string target_locale;
};

// A unit conversion rule between a unit and the standard SI unit
// of the same category.
// `ConversionRule` must be copyable as a member of `UnitConversion`.
class ConversionRule {
 public:
  ConversionRule(const ConversionRule& other);
  ConversionRule& operator=(const ConversionRule& other);
  ~ConversionRule();

  // Build a `ConversionRule` – returns nullopt if invalid conversion variables.
  static std::optional<ConversionRule> Create(
      const std::string& category,
      const std::string& unit_name,
      const std::optional<double>& term_a,
      const std::optional<double>& term_b,
      const std::optional<double>& term_c);

  double ConvertAmountToSi(double unit_amount) const;
  double ConvertAmountFromSi(double si_amount) const;
  bool IsSingleVariableLinearConversion() const;

  double term_a() const { return term_a_; }
  const std::string& category() const { return category_; }
  const std::string& unit_name() const { return unit_name_; }

 private:
  ConversionRule(const std::string& category,
                 const std::string& unit_name,
                 double term_a,
                 double term_b,
                 double term_c);

  std::string category_;
  std::string unit_name_;
  // Conversion formulas are in one of the two formula formats:
  // 1. 'target = a * source + b'
  // 2. 'target = c / source'
  // where a corresponds with |term_a_|, b corresponds with |term_b_|, and c
  // corresponds with |term_c_|.
  // If |term_a_| is a valid value then |term_c_| must be an invalid value, and
  // vice versa.
  //
  // If |term_a_| is not |kInvalidRateTermValue|, then the conversion formula is
  // 'target = a * source + b'
  double term_a_;
  double term_b_;
  // If |term_c_| is not |kInvalidRateTermValue|, then the conversion formula is
  // 'target = c / source'
  double term_c_;
};

// A unit conversion between two units in the same category.
// `UnitConversion` must be copyable as a member of `UnitConversionResult`.
class UnitConversion {
 public:
  UnitConversion(const UnitConversion& other);
  UnitConversion& operator=(const UnitConversion& other);
  ~UnitConversion();

  // Build a `UnitConversion` – returns nullopt if different unit categories.
  static std::optional<UnitConversion> Create(const ConversionRule& source_rule,
                                              const ConversionRule& dest_rule);

  // Used for sorting alternative unit conversions.
  //
  // We have no direct way of comparing unit conversions with different
  // formulas. The best approximation is to limit comparisons to linear
  // formulas where we only consider the |term_a_| values. A
  // smaller a1/a2 ratio between the |source_rule_| and |dest_rule_| of a
  // `UnitConversion` is understood as a "smaller" unit conversion.
  //
  // Unit conversions involving non-linear formulas will be considered greater
  // by default for our purposes.
  bool operator<(const UnitConversion& other) const;

  // Given a |source_amount| in the source unit, returns the equivalent amount
  // in the destination unit.
  double ConvertSourceAmountToDestAmount(double source_amount) const;

  // Function to build the formula description text shown on the Quick Answers
  // card. Return nullopt if no simple formula can be derived.
  std::optional<std::string> GetConversionFormulaText() const;

  const std::string& category() const { return source_rule_.category(); }
  const ConversionRule& source_rule() const { return source_rule_; }
  const ConversionRule& dest_rule() const { return dest_rule_; }

 private:
  UnitConversion(const ConversionRule& source_rule,
                 const ConversionRule& dest_rule);

  ConversionRule source_rule_;
  ConversionRule dest_rule_;
};

// `UnitConversionResult` holds result for unit conversion intent.
// `UnitConversionResult` must be copyable.
struct UnitConversionResult {
 public:
  UnitConversionResult();
  UnitConversionResult(const UnitConversionResult& other);
  UnitConversionResult& operator=(const UnitConversionResult& other);
  ~UnitConversionResult();

  std::string source_text;
  std::string result_text;
  std::string category;
  double source_amount = 0;
  // Not every unit conversion result will have valid unit conversions.
  std::optional<UnitConversion> source_to_dest_unit_conversion;
  std::vector<UnitConversion> alternative_unit_conversions_list;
};

// `StructuredResult` is NOT copyable as it's not trivial to make a class with
// unique_ptr to copyable.
class StructuredResult {
 public:
  StructuredResult();
  ~StructuredResult();
  StructuredResult(const StructuredResult&) = delete;
  StructuredResult& operator=(const StructuredResult) = delete;

  ResultType GetResultType() const;

  // Result type specific structs must be copyable as they can be copied to
  // views.
  std::unique_ptr<TranslationResult> translation_result;
  std::unique_ptr<DefinitionResult> definition_result;
  std::unique_ptr<UnitConversionResult> unit_conversion_result;
};

// `QuickAnswersSession` holds states related to a single Quick Answer session.
//
// This class currently holds results in `QuickAnswer` and `StructuredResult`.
// `QuickAnswer` field is used by `QuickAnswersView`. Rich Answers will read
// `StructuredResult`. Note that `QuickAnswer` is populated by using information
// in `StructuredResult`, i.e. `StructuredResult` is a super-set of
// `QuickAnswer`.
//
// Longer term plan is to migrate other states to this class, e.g. intent.
//
// `QuickAnswersSession` is NOT copyable as it's not trivial to make a class
// with unique_ptr to copyable.
class QuickAnswersSession {
 public:
  QuickAnswersSession();
  ~QuickAnswersSession();
  QuickAnswersSession(const QuickAnswersSession&) = delete;
  QuickAnswersSession& operator=(const QuickAnswersSession) = delete;

  // TODO(b/278929409): Once we migrate all result types to `StructuredResult`,
  // populate `QuickAnswer` outside of ResultParsers.
  std::unique_ptr<QuickAnswer> quick_answer;
  std::unique_ptr<StructuredResult> structured_result;
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_
