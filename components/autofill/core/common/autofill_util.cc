// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_util.h"

#include <stddef.h>

#include <algorithm>
#include <numeric>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

using mojom::FocusedFieldType;
using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

namespace {

constexpr base::StringPiece16 kSplitCharacters = u" .,-_@";

template <typename Char>
struct Compare : base::CaseInsensitiveCompareASCII<Char> {
  explicit Compare(bool case_sensitive) : case_sensitive_(case_sensitive) {}
  bool operator()(Char x, Char y) const {
    return case_sensitive_
               ? (x == y)
               : base::CaseInsensitiveCompareASCII<Char>::operator()(x, y);
  }

 private:
  bool case_sensitive_;
};

}  // namespace

bool IsShowAutofillSignaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShowAutofillSignatures);
}

bool IsKeyboardAccessoryEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else  // !BUILDFLAG(IS_ANDROID)
  return false;
#endif
}

bool IsPrefixOfEmailEndingWithAtSign(const std::u16string& full_string,
                                     const std::u16string& prefix) {
  return base::StartsWith(full_string, prefix + u"@",
                          base::CompareCase::SENSITIVE);
}

size_t GetTextSelectionStart(const std::u16string& suggestion,
                             const std::u16string& field_contents,
                             bool case_sensitive) {
  // Loop until we find either the |field_contents| is a prefix of |suggestion|
  // or character right before the match is one of the splitting characters.
  for (std::u16string::const_iterator it = suggestion.begin();
       (it = std::search(
            it, suggestion.end(), field_contents.begin(), field_contents.end(),
            Compare<std::u16string::value_type>(case_sensitive))) !=
       suggestion.end();
       ++it) {
    if (it == suggestion.begin() ||
        kSplitCharacters.find(it[-1]) != std::string::npos) {
      // Returns the character position right after the |field_contents| within
      // |suggestion| text as a caret position for text selection.
      return it - suggestion.begin() + field_contents.size();
    }
  }

  // Unable to find the |field_contents| in |suggestion| text.
  return std::u16string::npos;
}

bool IsCheckable(const FormFieldData::CheckStatus& check_status) {
  return check_status != FormFieldData::CheckStatus::kNotCheckable;
}

bool IsChecked(const FormFieldData::CheckStatus& check_status) {
  return check_status == FormFieldData::CheckStatus::kChecked;
}

void SetCheckStatus(FormFieldData* form_field_data,
                    bool isCheckable,
                    bool isChecked) {
  if (isChecked) {
    form_field_data->check_status = FormFieldData::CheckStatus::kChecked;
  } else {
    if (isCheckable) {
      form_field_data->check_status =
          FormFieldData::CheckStatus::kCheckableButUnchecked;
    } else {
      form_field_data->check_status = FormFieldData::CheckStatus::kNotCheckable;
    }
  }
}

std::vector<std::string> LowercaseAndTokenizeAttributeString(
    base::StringPiece attribute) {
  return base::SplitString(base::ToLowerASCII(attribute),
                           base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

bool SanitizedFieldIsEmpty(const std::u16string& value) {
  // Some sites enter values such as ____-____-____-____ or (___)-___-____ in
  // their fields. Check if the field value is empty after the removal of the
  // formatting characters.
  static const base::NoDestructor<std::u16string> formatting(
      base::StrCat({u"-_()/",
                    {&base::i18n::kRightToLeftMark, 1},
                    {&base::i18n::kLeftToRightMark, 1},
                    base::kWhitespaceUTF16}));

  return base::ContainsOnlyChars(value, *formatting);
}

size_t LevenshteinDistance(std::u16string_view a,
                           std::u16string_view b,
                           std::optional<size_t> max_distance) {
  if (a.size() > b.size()) {
    a.swap(b);
  }

  // max(a.size(), b.size()) steps always suffice.
  const size_t k = max_distance.value_or(b.size());
  // If the string's lengths differ by more than `k`, so does their
  // Levenshtein distance.
  if (a.size() + k < b.size() || a.size() > b.size() + k) {
    return k + 1;
  }
  // The classical Levenshtein distance DP defines dp[i][j] as the minimum
  // number of insert, remove and replace operation to convert a[:i] to b[:j].
  // To make this more efficient, one can define dp[i][d] as the distance of
  // a[:i] and b[:i + d]. Intuitively, d represents the delta between j and i in
  // the former dp. Since the Levenshtein distance is restricted by `k`, abs(d)
  // can be bounded by `k`. Since dp[i][d] only depends on values from dp[i-1],
  // it is not necessary to store the entire 2D table. Instead, this code just
  // stores the d-dimension, which represents "the distance with the current
  // prefix of the string, for a given delta d". Since d is between `-k` and
  // `k`, the implementation shifts the d-index by `k`, bringing it in range
  // [0, `2*k`].

  // The algorithm only cares if the Levenshtein distance is at most `k`. Thus,
  // any unreachable states and states in which the distance is certainly larger
  // than `k` can be set to any value larger than `k`, without affecting the
  // result.
  const size_t kInfinity = k + 1;
  std::vector<size_t> dp(2 * k + 1, kInfinity);
  // Initially, `dp[d]` represents the Levenshtein distance of the empty prefix
  // of `a` and the j = d - k characters of `b`. Their distance is j, since j
  // removals are required. States with negative d are not reachable, since that
  // corresponds to a negative index into `b`.
  std::iota(dp.begin() + static_cast<long>(k), dp.end(), 0);
  for (size_t i = 0; i < a.size(); i++) {
    // Right now, `dp` represents the Levenshtein distance when considering the
    // first `i` characters (up to index `i-1`) of `a`. After the next loop,
    // `dp` will represent the Levenshtein distance when considering the first
    // `i+1` characters.
    for (size_t d = 0; d <= 2 * k; d++) {
      if (i + d < k || i + d >= b.size() + k) {
        // `j = i + d - k` is out of range of `b`.
        dp[d] = kInfinity;
        continue;
      }
      const size_t j = i + d - k;
      // If `a[i] == `b[j]` the Levenshtein distance for `d` remained the same.
      if (a[i] != b[j]) {
        // (i, j) -> (i-1, j-1), `d` stays the same.
        const size_t replace = dp[d];
        // (i, j) -> (i-1, j), `d` increases by 1.
        // If the distance between `i` and `j` becomes larger than `k`, their
        // distance is at least `k + 1`. Same in the `insert` case.
        const size_t remove = d != 2 * k ? dp[d + 1] : kInfinity;
        // (i, j) -> (i, j-1), `d` decreases by 1. Since `i` stays the same,
        // this is intentionally using the dp value updated in the previous
        // iteration.
        const size_t insert = d != 0 ? dp[d - 1] : kInfinity;
        dp[d] = 1 + std::min({replace, remove, insert});
      }
    }
  }
  return std::min(dp[b.size() + k - a.size()], k + 1);
}

bool ShouldAutoselectFirstSuggestionOnArrowDown() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool IsFillable(FocusedFieldType focused_field_type) {
  switch (focused_field_type) {
    case FocusedFieldType::kFillableTextArea:
    case FocusedFieldType::kFillableSearchField:
    case FocusedFieldType::kFillableNonSearchField:
    case FocusedFieldType::kFillableUsernameField:
    case FocusedFieldType::kFillablePasswordField:
    case FocusedFieldType::kFillableWebauthnTaggedField:
      return true;
    case FocusedFieldType::kUnfillableElement:
    case FocusedFieldType::kUnknown:
      return false;
  }
  NOTREACHED_NORETURN();
}

SubmissionIndicatorEvent ToSubmissionIndicatorEvent(SubmissionSource source) {
  switch (source) {
    case SubmissionSource::NONE:
      return SubmissionIndicatorEvent::NONE;
    case SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      return SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;
    case SubmissionSource::XHR_SUCCEEDED:
      return SubmissionIndicatorEvent::XHR_SUCCEEDED;
    case SubmissionSource::FRAME_DETACHED:
      return SubmissionIndicatorEvent::FRAME_DETACHED;
    case SubmissionSource::DOM_MUTATION_AFTER_XHR:
      return SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR;
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return SubmissionIndicatorEvent::PROBABLE_FORM_SUBMISSION;
    case SubmissionSource::FORM_SUBMISSION:
      return SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  }

  NOTREACHED();
  return SubmissionIndicatorEvent::NONE;
}

GURL StripAuthAndParams(const GURL& gurl) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return gurl.ReplaceComponents(rep);
}

}  // namespace autofill
