// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/html_based_username_detector.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/html_based_username_detector_vocabulary.h"
#include "components/autofill/core/common/form_data.h"
#include "third_party/blink/public/web/web_form_element.h"

using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;

namespace autofill {

namespace {

// List of separators that can appear in HTML attribute values.
constexpr char16_t kDelimiters[] =
    u"$\"\'?%*@!\\/&^#:+~`;,>|<.[](){}-_ 0123456789";

// Minimum length of a word, in order not to be considered short word. Short
// words will not be searched in attribute values (especially after delimiters
// removing), because a short word may be a part of another word. A short word
// should be enclosed between delimiters, otherwise an occurrence doesn't count.
constexpr int kMinimumWordLength = 4;

// For each input element that can be a username, developer and user group
// values are computed. The user group value includes what a user sees: label,
// placeholder, aria-label (all are stored in FormFieldData.label). The
// developer group value consists of name and id attribute values.
// For each group the set of short tokens (tokens shorter than
// |kMinimumWordLength|) is computed as well.
struct UsernameFieldData {
  FieldRendererId renderer_id;
  std::u16string developer_value;
  base::flat_set<std::u16string> developer_short_tokens;
  std::u16string user_value;
  base::flat_set<std::u16string> user_short_tokens;
};

// Words that the algorithm looks for are split into multiple categories based
// on feature reliability.
// A category may contain a latin dictionary and a non-latin dictionary. It is
// mandatory that it has a latin one, but a non-latin might be missing.
// "Latin" translations are the translations of the words for which the
// original translation is similar to the romanized translation (translation of
// the word only using ISO basic Latin alphabet).
// "Non-latin" translations are the translations of the words that have custom,
// country specific characters.
struct CategoryOfWords {
  const base::span<const std::u16string_view> latin_dictionary;
  const base::span<const std::u16string_view> non_latin_dictionary;
};

// 1. Removes delimiters from |raw_value| and appends the remainder to
// |*field_data_value|. A sentinel symbol is added first if |*field_data_value|
// is not empty.
// 2. Tokenizes and appends short tokens (shorter than |kMinimumWordLength|)
// from |raw_value| to |*field_data_short_tokens|, if any.
void AppendValueAndShortTokens(
    const std::u16string& raw_value,
    std::u16string* field_data_value,
    base::flat_set<std::u16string>* field_data_short_tokens) {
  const std::u16string lowercase_value = base::i18n::ToLower(raw_value);
  std::vector<std::u16string_view> tokens =
      base::SplitStringPiece(lowercase_value, kDelimiters,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // When computing the developer value, '$' safety guard is being added
  // between field name and id, so that forming of accidental words is
  // prevented.
  if (!field_data_value->empty())
    field_data_value->push_back('$');

  field_data_value->reserve(field_data_value->size() + lowercase_value.size());
  std::vector<std::u16string> short_tokens;
  for (const std::u16string_view& token : tokens) {
    if (token.size() < kMinimumWordLength)
      short_tokens.emplace_back(token);
    field_data_value->append(token);
  }
  // It is better to insert elements to a |base::flat_set| in one operation.
  field_data_short_tokens->insert(short_tokens.begin(), short_tokens.end());
}

// For the given |input_element|, compute developer and user value, along with
// sets of short tokens, and returns it.
UsernameFieldData ComputeUsernameFieldData(const FormFieldData& field) {
  UsernameFieldData field_data;
  field_data.renderer_id = field.renderer_id();

  AppendValueAndShortTokens(field.name(), &field_data.developer_value,
                            &field_data.developer_short_tokens);
  AppendValueAndShortTokens(field.id_attribute(), &field_data.developer_value,
                            &field_data.developer_short_tokens);
  AppendValueAndShortTokens(field.label(), &field_data.user_value,
                            &field_data.user_short_tokens);
  return field_data;
}

void InferUsernameFieldData(
    const FormData& form_data,
    std::vector<UsernameFieldData>* possible_usernames_data) {
  for (const FormFieldData& field : form_data.fields()) {
    if (field.name().empty() &&
        field.form_control_type() == FormControlType::kInputPassword) {
      continue;
    }
    possible_usernames_data->push_back(ComputeUsernameFieldData(field));
  }
}

// Check if any word from |dictionary| is encountered in computed field
// information (i.e. |value|, |tokens|).
bool CheckFieldWithDictionary(
    const std::u16string& value,
    const base::flat_set<std::u16string>& short_tokens,
    base::span<const std::u16string_view> dictionary) {
  for (std::u16string_view word : dictionary) {
    if (word.length() < kMinimumWordLength) {
      // Treat short words by looking them up in the tokens set.
      if (short_tokens.find(word) != short_tokens.end()) {
        return true;
      }
    } else {
      // Treat long words by looking them up as a substring in |value|.
      if (value.find(word) != std::string::npos) {
        return true;
      }
    }
  }
  return false;
}

// Check if any word from |category| is encountered in computed field
// information (|possible_username|).
bool ContainsWordFromCategory(const UsernameFieldData& possible_username,
                              const CategoryOfWords& category) {
  // For user value, search in latin and non-latin dictionaries, because this
  // value is user visible. For developer value, only look up in latin
  /// dictionaries.
  return CheckFieldWithDictionary(possible_username.user_value,
                                  possible_username.user_short_tokens,
                                  category.latin_dictionary) ||
         CheckFieldWithDictionary(possible_username.user_value,
                                  possible_username.user_short_tokens,
                                  category.non_latin_dictionary) ||
         CheckFieldWithDictionary(possible_username.developer_value,
                                  possible_username.developer_short_tokens,
                                  category.latin_dictionary);
}

// Remove from |possible_usernames_data| the elements that definitely cannot be
// usernames, because their computed values contain at least one negative word.
void RemoveFieldsWithNegativeWords(
    std::vector<UsernameFieldData>* possible_usernames_data) {
  static constexpr CategoryOfWords kNegativeCategory = {kNegativeLatin,
                                                        kNegativeNonLatin};

  std::erase_if(
      *possible_usernames_data, [](const UsernameFieldData& possible_username) {
        return ContainsWordFromCategory(possible_username, kNegativeCategory);
      });
}

// Check if any word from the given category (|category|) appears in fields from
// the form (|possible_usernames_data|). If the category words appear in more
// than 2 fields, do nothing, because it may just be a prefix. If the words
// appears in 1 or 2 fields, the first field is added to |username_predictions|.
void FindWordsFromCategoryInForm(
    const std::vector<UsernameFieldData>& possible_usernames_data,
    const CategoryOfWords& category,
    std::vector<FieldRendererId>* username_predictions) {
  // Auxiliary element that contains the first field (in order of appearance in
  // the form) in which a substring is encountered.
  FieldRendererId chosen_field_renderer_id;

  size_t fields_found = 0;
  for (const UsernameFieldData& field_data : possible_usernames_data) {
    if (ContainsWordFromCategory(field_data, category)) {
      if (fields_found == 0) {
        chosen_field_renderer_id = field_data.renderer_id;
      }
      fields_found++;
    }
  }

  if (fields_found > 0 && fields_found <= 2)
    if (!base::Contains(*username_predictions, chosen_field_renderer_id))
      username_predictions->push_back(chosen_field_renderer_id);
}

// Find username elements if there is no cached result for the given form and
// add them to |username_predictions| in the order of decreasing reliability.
void FindUsernameFieldInternal(
    const FormData& form_data,
    std::vector<FieldRendererId>* username_predictions) {
  DCHECK(username_predictions);
  DCHECK(username_predictions->empty());

  static constexpr CategoryOfWords kUsernameCategory = {kUsernameLatin,
                                                        kUsernameLatin};
  static constexpr CategoryOfWords kUserCategory = {kUserLatin, kUserNonLatin};
  static constexpr CategoryOfWords kTechnicalCategory = {kTechnicalWords, {}};
  static constexpr CategoryOfWords kWeakCategory = {kWeakWords, {}};
  // These categories contain words that point to username field.
  // Order of categories is vital: the detector searches for words in descending
  // order of probability to point to a username field.
  static constexpr auto kPositiveCategories = std::to_array<CategoryOfWords>(
      {kUsernameCategory, kUserCategory, kTechnicalCategory, kWeakCategory});
  std::vector<UsernameFieldData> possible_usernames_data;

  InferUsernameFieldData(form_data, &possible_usernames_data);
  RemoveFieldsWithNegativeWords(&possible_usernames_data);

  // These are the searches performed by the username detector.
  for (const CategoryOfWords& category : kPositiveCategories) {
    FindWordsFromCategoryInForm(possible_usernames_data, category,
                                username_predictions);
  }
}

}  // namespace

const std::vector<FieldRendererId>& GetPredictionsFieldBasedOnHtmlAttributes(
    const FormData& form_data,
    UsernameDetectorCache* username_detector_cache) {
  // The cache will store the object referenced in the return value, so it must
  // exist. It can be empty.
  DCHECK(username_detector_cache);

  auto [form_position, cache_miss] = username_detector_cache->emplace(
      form_data.renderer_id(), std::vector<FieldRendererId>());

  if (cache_miss) {
    std::vector<FieldRendererId> username_predictions;
    FindUsernameFieldInternal(form_data, &username_predictions);
    if (!username_predictions.empty())
      form_position->second = std::move(username_predictions);
  }
  return form_position->second;
}

}  // namespace autofill
