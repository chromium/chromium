// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TEST_HELPERS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TEST_HELPERS_H_

#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

template <class... Matchers>
inline auto SuggestionVectorIdsAre(const Matchers&... matchers) {
  return ::testing::ElementsAre(::testing::Field(
      "popup_item_id", &Suggestion::popup_item_id, matchers)...);
}

template <class... Matchers>
inline auto SuggestionVectorMainTextsAre(const Matchers&... matchers) {
  return ::testing::ElementsAre(
      ::testing::Field("main_text", &Suggestion::main_text, matchers)...);
}

template <class... Matchers>
inline auto SuggestionVectorLabelsContains(const Matchers&... matchers) {
  return ::testing::Contains(
      ::testing::Field("labels", &Suggestion::labels, matchers)...);
}

template <class... Matchers>
inline auto SuggestionVectorIconsAre(const Matchers&... matchers) {
  return ::testing::ElementsAre(
      ::testing::Field("icon", &Suggestion::icon, matchers)...);
}

template <class... Matchers>
inline auto SuggestionVectorStoreIndicatorIconsAre(
    const Matchers&... matchers) {
  return ::testing::ElementsAre(
      ::testing::Field("icon", &Suggestion::trailing_icon, matchers)...);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TEST_HELPERS_H_
