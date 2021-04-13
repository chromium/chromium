// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/search/search_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/components/help_app_ui/search/search.mojom-test-utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace help_app {

class HelpAppSearchHandlerTest : public testing::Test {
 protected:
  HelpAppSearchHandlerTest() : handler_(local_search_service_proxy_.get()) {}
  ~HelpAppSearchHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    handler_.BindInterface(handler_remote_.BindNewPipeAndPassReceiver());

    handler_remote_.FlushForTesting();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  SearchHandler handler_;
  mojo::Remote<mojom::SearchHandler> handler_remote_;
};

TEST_F(HelpAppSearchHandlerTest, CanSearch) {
  // TODO(b/182857903): Add more tests for Update and Search.
  std::vector<mojom::SearchResultPtr> search_results;

  // Search for a query which should return no results.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("QueryWithNoResults"),
              /*max_num_results=*/3u, &search_results);
  EXPECT_TRUE(search_results.empty());
}

}  // namespace help_app
}  // namespace chromeos
