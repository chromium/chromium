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
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "url/url_constants.h"

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
static constexpr char kAnswerJsonImage[] = "i";
static constexpr char kAnswerJsonImageData[] = "d";

constexpr char kAnswerUsedUmaHistogramName[] =
    "Omnibox.SuggestionUsed.AnswerInSuggest";

}  // namespace

namespace omnibox::answer_data_parser {

// If necessary, concatenate scheme and host/path using only ':' as
// separator. This is due to the results delivering strings of the form
// "//host/path", which is web-speak for "use the enclosing page's scheme",
// but not a valid path of a URL. The GWS frontend commonly (always?)
// redirects to HTTPS, so we just default to that here.
GURL GetFormattedURL(const std::string* url_string) {
  return GURL(base::StartsWith(*url_string, "//", base::CompareCase::SENSITIVE)
                  ? (std::string(url::kHttpsScheme) + ":" + *url_string)
                  : *url_string);
}

void SetColorType(int text_type,
                  omnibox::FormattedString::FormattedStringFragment* fragment) {
  switch (text_type) {
    case DESCRIPTION_NEGATIVE: {
      fragment->set_color(omnibox::FormattedString::COLOR_ON_SURFACE_NEGATIVE);
      break;
    }
    case DESCRIPTION_POSITIVE: {
      fragment->set_color(omnibox::FormattedString::COLOR_ON_SURFACE_POSITIVE);
      break;
    }
    default:
      break;
  }
}

bool ParseJsonToFormattedStringFragment(
    const base::Value::Dict& field_json,
    omnibox::FormattedString* formatted_string) {
  const std::string* text = field_json.FindString(kAnswerJsonText);
  std::optional<int> type = field_json.FindInt(kAnswerJsonTextType);
  const bool parsed = text && !text->empty() && type;
  if (!parsed) {
    return false;
  }
  omnibox::FormattedString::FormattedStringFragment* fragment =
      formatted_string->add_fragments();
  std::string unescaped_text =
      base::UTF16ToUTF8(base::UnescapeForHTML(base::UTF8ToUTF16(*text)));
  if (formatted_string->has_text()) {
    // Append fragment text to FormattedString text.
    fragment->set_start_index(formatted_string->text().size() + 1);
    std::string full_text = formatted_string->text() + " " + unescaped_text;
    formatted_string->set_text(full_text);
  } else {
    fragment->set_start_index(0);
    formatted_string->set_text(unescaped_text);
  }
  fragment->set_text(unescaped_text);
  SetColorType(type.value(), fragment);
  return true;
}

bool ParseJsonToFormattedString(const base::Value::Dict& line_json,
                                omnibox::FormattedString* formatted_string,
                                omnibox::Image* image) {
  const base::Value::Dict* inner_json =
      line_json.FindDict(kAnswerJsonImageLine);
  if (!inner_json) {
    return false;
  }

  const base::Value::List* fields_json = inner_json->FindList(kAnswerJsonText);
  if (!fields_json || fields_json->empty()) {
    return false;
  }

  for (const base::Value& field_json : *fields_json) {
    if (!field_json.is_dict() || !ParseJsonToFormattedStringFragment(
                                     field_json.GetDict(), formatted_string)) {
      return false;
    }
  }

  const base::Value::Dict* additional_text_json =
      inner_json->FindDict(kAnswerJsonAdditionalText);
  if (additional_text_json && !ParseJsonToFormattedStringFragment(
                                  *additional_text_json, formatted_string)) {
    return false;
  }

  const std::string* accessibility_label =
      inner_json->FindString(kAnswerJsonAccessibilityLabel);
  if (accessibility_label) {
    formatted_string->set_a11y_text(*accessibility_label);
  }

  const base::Value::Dict* status_text_json =
      inner_json->FindDict(kAnswerJsonStatusText);
  if (status_text_json && !ParseJsonToFormattedStringFragment(
                              *status_text_json, formatted_string)) {
    return false;
  }

  const base::Value::Dict* image_json = inner_json->FindDict(kAnswerJsonImage);
  if (image_json) {
    const std::string* url_string =
        image_json->FindString(kAnswerJsonImageData);
    if (!url_string || url_string->empty()) {
      return false;
    }

    GURL image_url = GetFormattedURL(url_string);
    if (!image_url.is_valid()) {
      return false;
    }
    image->set_url(image_url.spec());
  }
  return true;
}

bool ParseJsonToAnswerData(const base::Value::Dict& answer_json,
                           omnibox::RichAnswerTemplate* answer_template) {
  // Ensure there are exactly two lines in the response.
  const base::Value::List* lines_json = answer_json.FindList(kAnswerJsonLines);
  if (!lines_json || lines_json->size() != 2) {
    return false;
  }

  const base::Value::Dict* first_line_dict = (*lines_json)[0].GetIfDict();
  omnibox::AnswerData* answer_data = answer_template->add_answers();
  if (!first_line_dict || !ParseJsonToFormattedString(
                              *first_line_dict, answer_data->mutable_headline(),
                              answer_data->mutable_image())) {
    return false;
  }

  const base::Value::Dict* second_line_dict = (*lines_json)[1].GetIfDict();
  if (!second_line_dict ||
      !ParseJsonToFormattedString(*second_line_dict,
                                  answer_data->mutable_subhead(),
                                  answer_data->mutable_image())) {
    return false;
  }

  const std::string* image_url;
  const base::Value::Dict* optional_image =
      answer_json.FindDict(kAnswerJsonImage);
  if (optional_image &&
      (image_url = optional_image->FindString(kAnswerJsonImageData)) &&
      !answer_data->image().has_url()) {
    answer_data->mutable_image()->set_url(*image_url);
  }
  return true;
}

void LogAnswerUsed(omnibox::AnswerType answer_type) {
  UMA_HISTOGRAM_ENUMERATION(kAnswerUsedUmaHistogramName, answer_type,
                            omnibox::AnswerType_MAX);
}

}  // namespace omnibox::answer_data_parser
