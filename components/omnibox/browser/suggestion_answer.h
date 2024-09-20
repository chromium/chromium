// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_ANSWER_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_ANSWER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "third_party/omnibox_proto/answer_data.pb.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace omnibox::answer_data_parser {
// These values are named and numbered to match a specification at go/ais_api.
// The values are only used for answer results.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.omnibox
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AnswerTextType
enum TextType {
  // Deprecated: ANSWER = 1,
  // Deprecated: HEADLINE = 2,
  TOP_ALIGNED = 3,
  // Deprecated: DESCRIPTION = 4,
  DESCRIPTION_NEGATIVE = 5,
  DESCRIPTION_POSITIVE = 6,
  // Deprecated: MORE_INFO = 7,
  SUGGESTION = 8,
  // Deprecated: SUGGESTION_POSITIVE = 9,
  // Deprecated: SUGGESTION_NEGATIVE = 10,
  // Deprecated: SUGGESTION_LINK = 11,
  // Deprecated: STATUS = 12,
  PERSONALIZED_SUGGESTION = 13,
  // Deprecated: IMMERSIVE_DESCRIPTION_TEXT = 14,
  // Deprecated: DATE_TEXT = 15,
  // Deprecated: PREVIEW_TEXT = 16,
  ANSWER_TEXT_MEDIUM = 17,
  ANSWER_TEXT_LARGE = 18,
  SUGGESTION_SECONDARY_TEXT_SMALL = 19,
  SUGGESTION_SECONDARY_TEXT_MEDIUM = 20,
};

GURL GetFormattedURL(const std::string* url_string);

bool ParseJsonToAnswerData(const base::Value::Dict& answer_json,
                           omnibox::RichAnswerTemplate* answer_template);

// Logs which answer type was used (if any) at the time a user used the
// omnibox to go somewhere.
void LogAnswerUsed(omnibox::AnswerType answer_type);
}  // namespace omnibox::answer_data_parser

// Structured representation of the JSON payload of a suggestion with an answer.
// An answer has exactly two image lines, so called because they are a
// combination of text and an optional image URL.  Each image line has 1 or more
// text fields, each of which is required to contain a string and an integer
// type.  The text fields are contained in a non-empty vector and three optional
// named properties, referred to as `additional text`, `status text`, and
// `accessibility label`.
//
// When represented in the UI, these elements should be styled and laid out
// according to the specification at https://goto.google.com/ais_api.
class SuggestionAnswer {
 public:
  class TextField;
  typedef std::vector<TextField> TextFields;
  typedef std::vector<GURL> URLs;

  // The above TextType values match what is sent by server, but are not used
  // normally by new answers.  These enum values are used instead, styling
  // answer text through a client-side process of interpretation that depends
  // on answer type, text type, and even line rank (first|second).  This
  // interpretation process happens during answer parsing and allows
  // downstream logic to remain simple, deciding how to present answers
  // based on a finite set of text types instead of answer properties and rules.
  // Performance is also improved by doing this once at parse time instead of
  // every time render text is invalidated.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.omnibox
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: AnswerTextStyle
  enum class TextStyle {
    NONE = 0,
    NORMAL,
    NORMAL_DIM,
    SECONDARY,
    BOLD,
    POSITIVE,
    NEGATIVE,
    SUPERIOR,  // This is not superscript (see gfx::BaselineStyle).
  };

  class TextField {
   public:
    TextField();
    ~TextField();
    TextField(const TextField&);
    TextField(TextField&&) noexcept;
    TextField& operator=(const TextField&);
    TextField& operator=(TextField&&) noexcept;

    // Parses |field_json| dictionary and populates |text_field| with the
    // contents.  If any of the required elements is missing, returns false and
    // leaves text_field in a partially populated state.
    static bool ParseTextField(const base::Value::Dict& field_json,
                               TextField* text_field);

    const std::u16string& text() const { return text_; }
    int type() const { return type_; }
    TextStyle style() const { return style_; }
    void set_style(TextStyle style) { style_ = style; }
    bool has_num_lines() const { return has_num_lines_; }
    int num_lines() const { return num_lines_; }

    bool Equals(const TextField& field) const;

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;

   private:
    std::u16string text_;
    int type_ = -1;
    bool has_num_lines_ = false;
    int num_lines_ = 1;
    TextStyle style_ = TextStyle::NONE;

    FRIEND_TEST_ALL_PREFIXES(SuggestionAnswerTest, DifferentValuesAreUnequal);

    // No DISALLOW_COPY_AND_ASSIGN since we depend on the copy constructor in
    // SuggestionAnswer::copy and the assignment operator as values in vector.
  };

  class ImageLine {
   public:
    ImageLine();
    explicit ImageLine(const ImageLine& line);
    ImageLine(ImageLine&&) noexcept;
    ImageLine& operator=(const ImageLine& line);
    ImageLine& operator=(ImageLine&&) noexcept;
    ~ImageLine();

    // Parses dictionary |line_json| and populates |image_line| with the
    // contents.  If any of the required elements is missing, returns false and
    // leaves text_field in a partially populated state.
    static bool ParseImageLine(const base::Value::Dict& line_json,
                               ImageLine* image_line);

    const TextFields& text_fields() const { return text_fields_; }
    int num_text_lines() const { return num_text_lines_; }
    const TextField* additional_text() const {
      if (additional_text_)
        return &additional_text_.value();
      else
        return nullptr;
    }
    const TextField* status_text() const {
      if (status_text_)
        return &status_text_.value();
      else
        return nullptr;
    }
    const std::u16string* accessibility_label() const {
      if (accessibility_label_)
        return &accessibility_label_.value();
      else
        return nullptr;
    }
    const GURL& image_url() const { return image_url_; }

    bool Equals(const ImageLine& line) const;

    // Returns a string appropriate for use as a readable representation of the
    // content of this line.
    std::u16string AccessibleText() const;

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;

    // Set text style on all fields where text type matches from_type and style
    // is not already set. Using from_type = 0 matches all values from TextType.
    void SetTextStyles(int from_type, TextStyle style);

   private:
    TextFields text_fields_;
    int num_text_lines_;
    std::optional<TextField> additional_text_;
    std::optional<TextField> status_text_;
    std::optional<std::u16string> accessibility_label_;
    GURL image_url_;

    FRIEND_TEST_ALL_PREFIXES(SuggestionAnswerTest, DifferentValuesAreUnequal);
  };

  SuggestionAnswer();
  SuggestionAnswer(const SuggestionAnswer& answer);
  SuggestionAnswer(SuggestionAnswer&&) noexcept;
  SuggestionAnswer& operator=(const SuggestionAnswer& answer);
  SuggestionAnswer& operator=(SuggestionAnswer&&) noexcept;
  ~SuggestionAnswer();

  // Parses dictionary |answer_json| and fills a SuggestionAnswer containing the
  // contents. Returns true on success. If the supplied data is not well formed
  // or is missing required elements, returns false instead.
  static bool ParseAnswer(const base::Value::Dict& answer_json,
                          omnibox::AnswerType answer_type,
                          SuggestionAnswer* answer);

  const GURL& image_url() const { return image_url_; }
  const ImageLine& first_line() const { return first_line_; }
  const ImageLine& second_line() const { return second_line_; }

  bool Equals(const SuggestionAnswer& answer) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // For new answers, replace old answer text types with appropriate new types.
  void InterpretTextTypes(omnibox::AnswerType answer_type);

  // Some types of matches (answers for dictionary definitions, e.g.) do not
  // follow the common rules for reversing lines.
  bool IsExceptedFromLineReversal(omnibox::AnswerType answer_type) const;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObject(
      omnibox::AnswerType answer_type) const;
#endif

 private:
  static const char kAnswerUsedUmaHistogramName[];

  GURL image_url_;
  ImageLine first_line_;
  ImageLine second_line_;

  FRIEND_TEST_ALL_PREFIXES(SuggestionAnswerTest, DifferentValuesAreUnequal);
  FRIEND_TEST_ALL_PREFIXES(SuggestionAnswerTest, LogAnswerUsed);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_ANSWER_H_
