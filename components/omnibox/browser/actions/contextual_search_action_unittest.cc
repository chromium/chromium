// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/contextual_search_action.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "testing/gtest/include/gtest/gtest.h"

class ContextualSearchActionTest : public testing::Test {
 public:
  ContextualSearchActionTest() = default;
};

TEST_F(ContextualSearchActionTest, RecordActionShown) {
  std::vector<std::pair<scoped_refptr<OmniboxAction>, std::string>> test_cases =
      {{base::MakeRefCounted<ContextualSearchOpenLensAction>(),
        "ContextualSearchOpenLensAction"},
       {base::MakeRefCounted<StarterPackBookmarksAction>(),
        "StarterPackBookmarksAction"},
       {base::MakeRefCounted<StarterPackHistoryAction>(),
        "StarterPackHistoryAction"},
       {base::MakeRefCounted<StarterPackTabsAction>(), "StarterPackTabsAction"},
       {base::MakeRefCounted<StarterPackAiModeAction>(),
        "StarterPackAiModeAction"}};

  for (const auto& entry : test_cases) {
    {
      SCOPED_TRACE(entry.second + ", shown but not used");
      base::HistogramTester histograms;
      entry.first->RecordActionShown(1, false);
      histograms.ExpectUniqueSample("Omnibox." + entry.second + ".Ctr", false,
                                    1);
    }
    {
      SCOPED_TRACE(entry.second + ", shown and used");
      base::HistogramTester histograms;
      entry.first->RecordActionShown(1, true);
      histograms.ExpectUniqueSample("Omnibox." + entry.second + ".Ctr", true,
                                    1);
    }
  }
}
