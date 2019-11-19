// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TEST_HELPERS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TEST_HELPERS_H_

#include <string>

#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Gmock matcher that allows checking a member of the Suggestion class in a
// vector. This wraps a GMock container matcher, converts the suggestion
// members to a vector, and then runs the container matcher against the result
// to test an argument. See SuggestionVectorIdsAre() below.
template <typename EltType>
class SuggestionVectorMembersAreMatcher
    : public testing::MatcherInterface<const std::vector<Suggestion>&> {
 public:
  typedef std::vector<EltType> Container;
  typedef testing::Matcher<Container> ContainerMatcher;

  SuggestionVectorMembersAreMatcher(const ContainerMatcher& seq_matcher,
                                    EltType Suggestion::*elt)
      : container_matcher_(seq_matcher), element_(elt) {}

  virtual bool MatchAndExplain(const std::vector<Suggestion>& suggestions,
                               testing::MatchResultListener* listener) const {
    Container container;
    for (const auto& suggestion : suggestions)
      container.push_back(suggestion.*element_);
    return container_matcher_.MatchAndExplain(container, listener);
  }

  virtual void DescribeTo(::std::ostream* os) const {
    container_matcher_.DescribeTo(os);
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    container_matcher_.DescribeNegationTo(os);
  }

 private:
  ContainerMatcher container_matcher_;
  EltType Suggestion::*element_;
};

// Use this matcher to compare a sequence vector's IDs to a list. In an
// EXPECT_CALL statement, use the following for an vector<Suggestion> argument
// to compare the IDs against a constant list:
//   SuggestionVectorIdsAre(testing::ElementsAre(1, 2, 3, 4))
template <class EltsAreMatcher>
inline testing::Matcher<const std::vector<Suggestion>&> SuggestionVectorIdsAre(
    const EltsAreMatcher& elts_are_matcher) {
  return testing::MakeMatcher(new SuggestionVectorMembersAreMatcher<int>(
      elts_are_matcher, &Suggestion::frontend_id));
}

// Like SuggestionVectorIdsAre above, but tests the values.
template <class EltsAreMatcher>
inline testing::Matcher<const std::vector<Suggestion>&>
SuggestionVectorValuesAre(const EltsAreMatcher& elts_are_matcher) {
  return testing::MakeMatcher(
      new SuggestionVectorMembersAreMatcher<base::string16>(
          elts_are_matcher, &Suggestion::value));
}

// Like SuggestionVectorIdsAre above, but tests the labels.
template <class EltsAreMatcher>
inline testing::Matcher<const std::vector<Suggestion>&>
SuggestionVectorLabelsAre(const EltsAreMatcher& elts_are_matcher) {
  return testing::MakeMatcher(
      new SuggestionVectorMembersAreMatcher<base::string16>(
          elts_are_matcher, &Suggestion::label));
}

// Like SuggestionVectorIdsAre above, but tests the icons.
template <class EltsAreMatcher>
inline testing::Matcher<const std::vector<Suggestion>&>
SuggestionVectorIconsAre(const EltsAreMatcher& elts_are_matcher) {
  return testing::MakeMatcher(
      new SuggestionVectorMembersAreMatcher<std::string>(elts_are_matcher,
                                                         &Suggestion::icon));
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_TEST_HELPERS_H_
