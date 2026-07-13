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
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ContextualSearchActionTest : public testing::Test {
 public:
  ContextualSearchActionTest() = default;
};

TEST_F(ContextualSearchActionTest, RecordActionShown) {
  std::vector<std::pair<scoped_refptr<OmniboxAction>, std::string>> test_cases =
      {{base::MakeRefCounted<ContextualSearchOpenLensAction>(),
        "ContextualSearchOpenLensAction"}};

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

TEST_F(ContextualSearchActionTest, Execute_RoutesToCoBrowse) {
  using ::testing::_;
  using ::testing::Return;

  MockAutocompleteProviderClient client;
  OmniboxAction::ExecutionContext context(
      client, OmniboxAction::ExecutionContext::OpenUrlCallback(),
      base::TimeTicks(), WindowOpenDisposition::IGNORE_ACTION);

  auto action = base::MakeRefCounted<ContextualSearchOpenLensAction>();

  // Case 1: ShouldOpenCoBrowsePanel is true -> Opens CoBrowse, bypasses Lens
  EXPECT_CALL(client, ShouldOpenCoBrowsePanel()).WillOnce(Return(true));
  EXPECT_CALL(client, OpenCoBrowsePanel()).Times(1);
  EXPECT_CALL(client, OpenLensOverlay(_)).Times(0);
  action->Execute(context);

  testing::Mock::VerifyAndClearExpectations(&client);

  // Case 2: ShouldOpenCoBrowsePanel is false -> Opens Lens Overlay, bypasses
  // CoBrowse
  EXPECT_CALL(client, ShouldOpenCoBrowsePanel()).WillOnce(Return(false));
  EXPECT_CALL(client, OpenCoBrowsePanel()).Times(0);
  EXPECT_CALL(client, OpenLensOverlay(true)).Times(1);
  action->Execute(context);
}
