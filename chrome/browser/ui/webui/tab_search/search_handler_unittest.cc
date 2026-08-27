// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/search_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TabSearchSearchHandlerTest : public testing::Test {
 public:
  TabSearchSearchHandlerTest() {
    handler_ =
        std::make_unique<SearchHandler>(remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<tab_search::mojom::SearchHandler> remote_;
  std::unique_ptr<SearchHandler> handler_;
};

TEST_F(TabSearchSearchHandlerTest, GetRangesIgnoringCaseAndAccentsEmpty) {
  std::vector<std::vector<tab_search::mojom::TokenRangePtr>> ranges;
  remote_->GetRangesIgnoringCaseAndAccents(
      "", {"test", "hello"},
      base::BindLambdaForTesting(
          [&](std::vector<std::vector<tab_search::mojom::TokenRangePtr>>
                  result) { ranges = std::move(result); }));
  remote_.FlushForTesting();

  ASSERT_EQ(2u, ranges.size());
  EXPECT_TRUE(ranges[0].empty());
  EXPECT_TRUE(ranges[1].empty());
}

TEST_F(TabSearchSearchHandlerTest, GetRangesIgnoringCaseAndAccentsMatch) {
  std::vector<std::vector<tab_search::mojom::TokenRangePtr>> ranges;
  remote_->GetRangesIgnoringCaseAndAccents(
      "cafe", {"Café de Paris", "No match", "Café and café"},
      base::BindLambdaForTesting(
          [&](std::vector<std::vector<tab_search::mojom::TokenRangePtr>>
                  result) { ranges = std::move(result); }));
  remote_.FlushForTesting();

  ASSERT_EQ(3u, ranges.size());
  ASSERT_EQ(1u, ranges[0].size());
  EXPECT_EQ(0u, ranges[0][0]->start);
  EXPECT_EQ(4u, ranges[0][0]->length);

  EXPECT_TRUE(ranges[1].empty());

  ASSERT_EQ(2u, ranges[2].size());
  EXPECT_EQ(0u, ranges[2][0]->start);
  EXPECT_EQ(4u, ranges[2][0]->length);
  EXPECT_EQ(9u, ranges[2][1]->start);
  EXPECT_EQ(4u, ranges[2][1]->length);
}

}  // namespace
