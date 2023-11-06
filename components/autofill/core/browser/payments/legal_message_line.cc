// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/legal_message_line.h"

#include "base/check.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace autofill {
namespace {

// Replace "{0}", "{1}", ... in |template_icu| with corresponding strings
// from |display_texts|. Sets |out_message| to the resulting string, with
// start position of each replacement in |out_offsets|.
// Return false on failure. If false is returned then contents of |out_message|
// and |out_offsets| are undefined.
bool ReplaceTemplatePlaceholders(
    const std::u16string& template_icu,
    const std::vector<std::u16string>& display_texts,
    std::u16string* out_message,
    std::vector<size_t>* out_offsets) {
  // Escape "$" -> "$$" for ReplaceStringPlaceholders().
  //
  // Edge cases:
  // 1. Two or more consecutive $ characters will be incorrectly expanded
  //    ("$$" -> "$$$$", which ReplaceStringPlaceholders() then turns into
  //    "$$$").
  //
  // 2. "${" will cause false to be returned. "${0}" will expand to "$${0}".
  //    FormatWithNumberedArgs() turns it into "$$$1", which
  //    ReplaceStringPlaceholders() then turns into "$$1" without doing the
  //    parameter replacement. This causes false to be returned because each
  //    parameter is not used exactly once.
  //
  // Both of these cases are noted in the header file, and are unlikely to
  // occur in any actual legal message.
  std::u16string template_icu_escaped;
  base::ReplaceChars(template_icu, u"$", u"$$", &template_icu_escaped);

  // Replace "{0}" -> "$1", "{1}" -> "$2", ... to prepare |template_dollars|
  // for ReplaceStringPlaceholders().
  std::u16string template_dollars =
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          template_icu_escaped, "$1", "$2", "$3", "$4", "$5", "$6", "$7");

  // FormatWithNumberedArgs() returns an empty string on failure.
  if (template_dollars.empty() && !template_icu.empty())
    return false;

  // Replace "$1", "$2", ... with the display text of each parameter.
  *out_message = base::ReplaceStringPlaceholders(template_dollars,
                                                 display_texts, out_offsets);

  // Each parameter must be used exactly once. If a parameter is unused or
  // used more than once then it can't be determined which |offsets| entry
  // corresponds to which parameter.
  return out_offsets->size() == display_texts.size();
}

}  // namespace

LegalMessageLine::Link::Link(size_t start,
                             size_t end,
                             const std::string& url_spec)
    : range(start, end), url(url_spec) {}

LegalMessageLine::Link::~Link() = default;

bool LegalMessageLine::Link::operator==(
    const LegalMessageLine::Link& other) const = default;

LegalMessageLine::LegalMessageLine() = default;

LegalMessageLine::LegalMessageLine(const LegalMessageLine& other) = default;

LegalMessageLine::~LegalMessageLine() = default;

bool LegalMessageLine::operator==(const LegalMessageLine& other) const =
    default;

// static
bool LegalMessageLine::Parse(const base::Value::Dict& legal_message,
                             LegalMessageLines* out,
                             bool escape_apostrophes) {
  const base::Value::List* lines_list = legal_message.FindList("line");
  if (lines_list) {
    LegalMessageLines lines;
    lines.reserve(lines_list->size());
    for (const base::Value& single_line : *lines_list) {
      lines.emplace_back();
      if (!single_line.is_dict() ||
          !lines.back().ParseLine(single_line.GetDict(), escape_apostrophes)) {
        return false;
      }
    }

    out->swap(lines);
  }
  return true;
}

bool LegalMessageLine::ParseLine(const base::Value::Dict& line,
                                 bool escape_apostrophes) {
  DCHECK(text_.empty());
  DCHECK(links_.empty());

  // |display_texts| elements are the strings that will be substituted for
  // "{0}", "{1}", etc. in the template string.
  std::vector<std::u16string> display_texts;

  // Process all the template parameters.
  const base::Value::List* template_parameters =
      line.FindList("template_parameter");
  if (template_parameters) {
    display_texts.reserve(template_parameters->size());
    links_.reserve(template_parameters->size());

    for (const base::Value& parameter : *template_parameters) {
      if (!parameter.is_dict())
        return false;

      const std::string* display_text =
          parameter.GetDict().FindString("display_text");
      if (!display_text)
        return false;

      const std::string* url = parameter.GetDict().FindString("url");
      if (!url)
        return false;

      display_texts.push_back(base::UTF8ToUTF16(*display_text));
      links_.emplace_back(0, 0, *url);
    }
  }

  // Read the template string. It's a small subset of the ICU message format
  // syntax.
  const std::string* template_icu_utf8 = line.FindString("template");
  if (!template_icu_utf8)
    return false;

  std::u16string template_icu = base::UTF8ToUTF16(*template_icu_utf8);
  if (escape_apostrophes) {
    // The ICU standard counts "'{" as beginning an escaped string literal, even
    // if there's no closing apostrophe.  This fails legal message templates
    // where an apostrophe precedes the template parameter, like "l'{1}" in
    // Italian.  Therefore, when |escape_apostrophes| is true, escape all
    // apostrophes in the string by doubling them up.
    // http://www.icu-project.org/apiref/icu4c/messagepattern_8h.html#af6e0757e0eb81c980b01ee5d68a9978b
    base::ReplaceChars(template_icu, u"'", u"''", &template_icu);
  }

  // Replace the placeholders in |template_icu| with strings from
  // |display_texts|, and store the start position of each replacement in
  // |offsets|.
  std::vector<size_t> offsets;
  if (!ReplaceTemplatePlaceholders(template_icu, display_texts, &text_,
                                   &offsets))
    return false;

  // Fill in range values for all links.
  for (size_t offset_index = 0; offset_index < offsets.size(); ++offset_index) {
    size_t range_start = offsets[offset_index];
    links_[offset_index].range = gfx::Range(
        range_start, range_start + display_texts[offset_index].size());
  }

  return true;
}

}  // namespace autofill
