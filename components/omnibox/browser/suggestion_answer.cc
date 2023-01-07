// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_answer.h"

#include <stddef.h>

#include <memory>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "build/build_config.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "components/omnibox/browser/jni_headers/SuggestionAnswer_jni.h"

using base::android::ScopedJavaLocalRef;
#endif

namespace {

// All of these are defined here (even though most are only used once each) so
// the format details are easy to locate and update or compare to the spec doc.
static constexpr char kAnswerJsonLines[] = "l";
static constexpr char kAnswerJsonImageLine[] = "il";
static constexpr char kAnswerJsonText[] = "t";
static constexpr char kAnswerJsonAdditionalText[] = "at";
static constexpr char kAnswerJsonStatusText[] = "st";
static constexpr char kAnswerJsonAccessibilityLabel[] = "al";
static constexpr char kAnswerJsonTextType[] = "tt";
static constexpr char kAnswerJsonNumLines[] = "ln";
static constexpr char kAnswerJsonImage[] = "i";
static constexpr char kAnswerJsonImageData[] = "d";

void AppendWithSpace(const SuggestionAnswer::TextField* text,
                     std::u16string* output) {
  if (!text) {
    return;
  }
  if (!output->empty() && !text->text().empty()) {
    *output += ' ';
  }
  *output += text->text();
}

}  // namespace

// SuggestionAnswer::TextField -------------------------------------------------

SuggestionAnswer::TextField::TextField() = default;
SuggestionAnswer::TextField::~TextField() = default;
SuggestionAnswer::TextField::TextField(const TextField&) = default;
SuggestionAnswer::TextField::TextField(TextField&&) noexcept = default;
SuggestionAnswer::TextField& SuggestionAnswer::TextField::operator=(
    const TextField&) = default;
SuggestionAnswer::TextField& SuggestionAnswer::TextField::operator=(
    TextField&&) noexcept = default;

// static
bool SuggestionAnswer::TextField::ParseTextField(
    const base::Value::Dict& field_json,
    TextField* text_field) {
  const std::string* text = field_json.FindString(kAnswerJsonText);
  absl::optional<int> type = field_json.FindInt(kAnswerJsonTextType);
  const bool parsed = text && !text->empty() && type;
  if (parsed) {
    text_field->type_ = *type;
    text_field->text_ = base::UnescapeForHTML(base::UTF8ToUTF16(*text));

    absl::optional<int> num_lines = field_json.FindInt(kAnswerJsonNumLines);
    text_field->has_num_lines_ = num_lines.has_value();
    if (num_lines) {
      text_field->has_num_lines_ = true;
      text_field->num_lines_ = *num_lines;
    } else {
      text_field->has_num_lines_ = false;
    }
  }
  return parsed;
}

bool SuggestionAnswer::TextField::Equals(const TextField& field) const {
  return type_ == field.type_ && text_ == field.text_ &&
         has_num_lines_ == field.has_num_lines_ &&
         (!has_num_lines_ || num_lines_ == field.num_lines_);
}

size_t SuggestionAnswer::TextField::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(text_);
}

// SuggestionAnswer::ImageLine -------------------------------------------------

SuggestionAnswer::ImageLine::ImageLine() : num_text_lines_(1) {}
SuggestionAnswer::ImageLine::ImageLine(const ImageLine& line) = default;
SuggestionAnswer::ImageLine::ImageLine(ImageLine&&) noexcept = default;

SuggestionAnswer::ImageLine& SuggestionAnswer::ImageLine::operator=(
    const ImageLine& line) = default;
SuggestionAnswer::ImageLine& SuggestionAnswer::ImageLine::operator=(
    ImageLine&&) noexcept = default;

SuggestionAnswer::ImageLine::~ImageLine() = default;

// static
bool SuggestionAnswer::ImageLine::ParseImageLine(
    const base::Value::Dict& line_json,
    ImageLine* image_line) {
  const base::Value::Dict* inner_json =
      line_json.FindDict(kAnswerJsonImageLine);
  if (!inner_json) {
    return false;
  }

  const base::Value::List* fields_json = inner_json->FindList(kAnswerJsonText);
  if (!fields_json || fields_json->empty()) {
    return false;
  }

  bool found_num_lines = false;
  for (const base::Value& field_json : *fields_json) {
    TextField text_field;
    if (!field_json.is_dict() ||
        !TextField::ParseTextField(field_json.GetDict(), &text_field)) {
      return false;
    }

    image_line->text_fields_.push_back(text_field);
    if (!found_num_lines && text_field.has_num_lines()) {
      found_num_lines = true;
      image_line->num_text_lines_ = text_field.num_lines();
    }
  }

  const base::Value::Dict* additional_text_json =
      inner_json->FindDict(kAnswerJsonAdditionalText);
  if (additional_text_json) {
    image_line->additional_text_ = TextField();
    if (!TextField::ParseTextField(*additional_text_json,
                                   &image_line->additional_text_.value())) {
      return false;
    }
  }

  const base::Value::Dict* status_text_json =
      inner_json->FindDict(kAnswerJsonStatusText);
  if (status_text_json) {
    image_line->status_text_ = TextField();
    if (!TextField::ParseTextField(*status_text_json,
                                   &image_line->status_text_.value())) {
      return false;
    }
  }

  const std::string* accessibility_label =
      inner_json->FindString(kAnswerJsonAccessibilityLabel);
  if (accessibility_label) {
    image_line->accessibility_label_ = base::UTF8ToUTF16(*accessibility_label);
  }

  const base::Value::Dict* image_json = inner_json->FindDict(kAnswerJsonImage);
  if (image_json) {
    const std::string* url_string =
        image_json->FindString(kAnswerJsonImageData);
    if (!url_string || url_string->empty()) {
      return false;
    }
    // If necessary, concatenate scheme and host/path using only ':' as
    // separator. This is due to the results delivering strings of the form
    // "//host/path", which is web-speak for "use the enclosing page's scheme",
    // but not a valid path of a URL. The GWS frontend commonly (always?)
    // redirects to HTTPS, so we just default to that here.
    image_line->image_url_ =
        GURL(base::StartsWith(*url_string, "//", base::CompareCase::SENSITIVE)
                 ? (std::string(url::kHttpsScheme) + ":" + *url_string)
                 : *url_string);

    if (!image_line->image_url_.is_valid()) {
      return false;
    }
  }

  return true;
}

bool SuggestionAnswer::ImageLine::Equals(const ImageLine& line) const {
  if (text_fields_.size() != line.text_fields_.size()) {
    return false;
  }
  for (size_t i = 0; i < text_fields_.size(); ++i) {
    if (!text_fields_[i].Equals(line.text_fields_[i])) {
      return false;
    }
  }

  if (num_text_lines_ != line.num_text_lines_) {
    return false;
  }

  if (additional_text_ || line.additional_text_) {
    if (!additional_text_ || !line.additional_text_) {
      return false;
    }
    if (!additional_text_->Equals(*line.additional_text_)) {
      return false;
    }
  }

  if (status_text_ || line.status_text_) {
    if (!status_text_ || !line.status_text_) {
      return false;
    }
    if (!status_text_->Equals(*line.status_text_)) {
      return false;
    }
  }

  if (accessibility_label_ != line.accessibility_label_) {
    return false;
  }

  return image_url_ == line.image_url_;
}

// TODO(jdonnelly): When updating the display of answers in RTL languages,
// modify this to be consistent.
std::u16string SuggestionAnswer::ImageLine::AccessibleText() const {
  std::u16string result;
  for (const TextField& text_field : text_fields_) {
    AppendWithSpace(&text_field, &result);
  }
  AppendWithSpace(additional_text(), &result);
  AppendWithSpace(status_text(), &result);
  return result;
}

size_t SuggestionAnswer::ImageLine::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(text_fields_);
  res += sizeof(int);
  if (additional_text_) {
    res += base::trace_event::EstimateMemoryUsage(additional_text_.value());
  } else {
    res += sizeof(TextField);
  }
  res += sizeof(int);
  if (status_text_) {
    res += base::trace_event::EstimateMemoryUsage(status_text_.value());
  } else {
    res += sizeof(TextField);
  }
  if (accessibility_label_) {
    res += base::trace_event::EstimateMemoryUsage(accessibility_label_.value());
  } else {
    res += sizeof(std::u16string);
  }
  res += base::trace_event::EstimateMemoryUsage(image_url_);

  return res;
}

void SuggestionAnswer::ImageLine::SetTextStyles(
    int from_type,
    SuggestionAnswer::TextStyle style) {
  const auto replace = [=](auto* field) {
    if (field->style() == TextStyle::NONE &&
        (from_type == 0 || from_type == field->type())) {
      field->set_style(style);
    }
  };
  for (auto& field : text_fields_) {
    replace(&field);
  }
  if (additional_text_) {
    replace(&additional_text_.value());
  }
  if (status_text_) {
    replace(&status_text_.value());
  }
}

// SuggestionAnswer ------------------------------------------------------------

SuggestionAnswer::SuggestionAnswer() = default;

SuggestionAnswer::SuggestionAnswer(const SuggestionAnswer& answer) = default;

SuggestionAnswer::SuggestionAnswer(SuggestionAnswer&&) noexcept = default;

SuggestionAnswer& SuggestionAnswer::operator=(const SuggestionAnswer& answer) =
    default;

SuggestionAnswer& SuggestionAnswer::operator=(SuggestionAnswer&&) noexcept =
    default;

SuggestionAnswer::~SuggestionAnswer() = default;

// static
bool SuggestionAnswer::ParseAnswer(const base::Value::Dict& answer_json,
                                   const std::u16string& answer_type_str,
                                   SuggestionAnswer* result) {
  int answer_type = 0;
  if (!base::StringToInt(answer_type_str, &answer_type)) {
    return false;
  }

  result->set_type(answer_type);

  const base::Value::List* lines_json = answer_json.FindList(kAnswerJsonLines);
  if (!lines_json || lines_json->size() != 2) {
    return false;
  }

  const base::Value::Dict* first_line_dict = (*lines_json)[0].GetIfDict();
  if (!first_line_dict ||
      !ImageLine::ParseImageLine(*first_line_dict, &result->first_line_)) {
    return false;
  }

  const base::Value::Dict* second_line_dict = (*lines_json)[1].GetIfDict();
  if (!second_line_dict ||
      !ImageLine::ParseImageLine(*second_line_dict, &result->second_line_)) {
    return false;
  }

  const std::string* image_url;
  const base::Value::Dict* optional_image = answer_json.FindDict("i");
  if (optional_image && (image_url = optional_image->FindString("d"))) {
    result->image_url_ = GURL(*image_url);
  } else {
    result->image_url_ = result->second_line_.image_url();
  }
  result->InterpretTextTypes();
  return true;
}

bool SuggestionAnswer::Equals(const SuggestionAnswer& answer) const {
  return type_ == answer.type_ && image_url_ == answer.image_url_ &&
         first_line_.Equals(answer.first_line_) &&
         second_line_.Equals(answer.second_line_);
}

void SuggestionAnswer::AddImageURLsTo(URLs* urls) const {
  // Note: first_line_.image_url() is not used in practice (so it's ignored).
  if (image_url_.is_valid()) {
    urls->push_back(image_url_);
  } else if (second_line_.image_url().is_valid()) {
    urls->push_back(second_line_.image_url());
  }
}

size_t SuggestionAnswer::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(image_url_);
  res += base::trace_event::EstimateMemoryUsage(first_line_);
  res += base::trace_event::EstimateMemoryUsage(second_line_);

  return res;
}

void SuggestionAnswer::InterpretTextTypes() {
  switch (type()) {
    case SuggestionAnswer::ANSWER_TYPE_WEATHER: {
      second_line_.SetTextStyles(SuggestionAnswer::TOP_ALIGNED,
                                 TextStyle::SUPERIOR);
      break;
    }
    case SuggestionAnswer::ANSWER_TYPE_FINANCE: {
      first_line_.SetTextStyles(
          SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_SMALL,
          TextStyle::SECONDARY);
      second_line_.SetTextStyles(SuggestionAnswer::DESCRIPTION_POSITIVE,
                                 TextStyle::POSITIVE);
      second_line_.SetTextStyles(SuggestionAnswer::DESCRIPTION_NEGATIVE,
                                 TextStyle::NEGATIVE);
      break;
    }
    default:
      break;
  }

  // Most answers uniformly apply different styling for each answer line.
  // Any old styles not replaced above will get these by default.
  if (IsExceptedFromLineReversal()) {
    first_line_.SetTextStyles(0, TextStyle::NORMAL);
    second_line_.SetTextStyles(0, TextStyle::NORMAL_DIM);
  } else {
    first_line_.SetTextStyles(0, TextStyle::NORMAL_DIM);
    second_line_.SetTextStyles(0, TextStyle::NORMAL);
  }
}

bool SuggestionAnswer::IsExceptedFromLineReversal() const {
  return type() == SuggestionAnswer::ANSWER_TYPE_DICTIONARY;
}

// static
void SuggestionAnswer::LogAnswerUsed(
    const absl::optional<SuggestionAnswer>& answer) {
  auto answer_type = SuggestionAnswer::ANSWER_TYPE_INVALID;
  if (answer) {
    answer_type = static_cast<SuggestionAnswer::AnswerType>(answer->type());
  }
  DCHECK_NE(-1, answer_type);  // just in case; |type_| is init'd to -1
  UMA_HISTOGRAM_ENUMERATION(kAnswerUsedUmaHistogramName, answer_type,
                            SuggestionAnswer::ANSWER_TYPE_TOTAL_COUNT);
}

// static
const char SuggestionAnswer::kAnswerUsedUmaHistogramName[] =
    "Omnibox.SuggestionUsed.AnswerInSuggest";

#if BUILDFLAG(IS_ANDROID)
namespace {

ScopedJavaLocalRef<jobject> CreateJavaTextField(
    JNIEnv* env,
    const SuggestionAnswer::TextField& text_field) {
  return Java_SuggestionAnswer_createTextField(
      env, text_field.type(),
      base::android::ConvertUTF16ToJavaString(env, text_field.text()),
      static_cast<int>(text_field.style()), text_field.num_lines());
}

ScopedJavaLocalRef<jobject> CreateJavaImageLine(
    JNIEnv* env,
    const SuggestionAnswer::ImageLine* image_line) {
  ScopedJavaLocalRef<jobject> jtext_fields =
      Java_SuggestionAnswer_createTextFieldList(env);
  for (const SuggestionAnswer::TextField& text_field :
       image_line->text_fields()) {
    Java_SuggestionAnswer_addTextFieldToList(
        env, jtext_fields, CreateJavaTextField(env, text_field));
  }

  ScopedJavaLocalRef<jobject> jadditional_text;
  if (image_line->additional_text()) {
    jadditional_text = CreateJavaTextField(env, *image_line->additional_text());
  }

  ScopedJavaLocalRef<jobject> jstatus_text;
  if (image_line->status_text()) {
    jstatus_text = CreateJavaTextField(env, *image_line->status_text());
  }

  ScopedJavaLocalRef<jstring> jimage_url;
  if (image_line->image_url().is_valid()) {
    jimage_url = base::android::ConvertUTF8ToJavaString(
        env, image_line->image_url().spec());
  }

  return Java_SuggestionAnswer_createImageLine(
      env, jtext_fields, jadditional_text, jstatus_text, jimage_url);
}

}  // namespace

ScopedJavaLocalRef<jobject> SuggestionAnswer::CreateJavaObject() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_SuggestionAnswer_createSuggestionAnswer(
      env, static_cast<int>(type_), CreateJavaImageLine(env, &first_line_),
      CreateJavaImageLine(env, &second_line_));
}
#endif  // BUILDFLAG(IS_ANDROID)
