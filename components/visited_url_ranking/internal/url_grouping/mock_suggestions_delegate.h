// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_MOCK_SUGGESTIONS_DELEGATE_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_MOCK_SUGGESTIONS_DELEGATE_H_

#include "base/functional/callback.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace visited_url_ranking {

class MockGroupSuggestionsDelegate : public GroupSuggestionsDelegate {
 public:
  MockGroupSuggestionsDelegate();
  ~MockGroupSuggestionsDelegate() override;

  MOCK_METHOD(void,
              ShowSuggestion,
              (const GroupSuggestions& group_suggestions,
               SuggestionResponseCallback response_callback),
              (override));

  MOCK_METHOD(void,
              OnDumpStateForFeedback,
              (const std::string& dump_state),
              (override));
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_MOCK_SUGGESTIONS_DELEGATE_H_
