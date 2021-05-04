// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/search/search_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/components/help_app_ui/search/search.mojom-test-utils.h"
#include "chromeos/components/help_app_ui/search/search_tag_registry.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace help_app {
namespace {

class FakeObserver : public mojom::SearchResultsObserver {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  mojo::PendingRemote<mojom::SearchResultsObserver> GenerateRemote() {
    mojo::PendingRemote<mojom::SearchResultsObserver> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  size_t num_calls() const { return num_calls_; }

 private:
  // mojom::SearchResultsObserver:
  void OnSearchResultAvailabilityChanged() override { ++num_calls_; }

  size_t num_calls_ = 0;
  mojo::Receiver<mojom::SearchResultsObserver> receiver_{this};
};

}  // namespace

class HelpAppSearchHandlerTest : public testing::Test {
 protected:
  HelpAppSearchHandlerTest()
      : search_tag_registry_(local_search_service_proxy_.get()),
        handler_(&search_tag_registry_, local_search_service_proxy_.get()) {}
  ~HelpAppSearchHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    handler_.BindInterface(handler_remote_.BindNewPipeAndPassReceiver());

    handler_remote_->Observe(observer_.GenerateRemote());
    handler_remote_.FlushForTesting();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  SearchTagRegistry search_tag_registry_;
  SearchHandler handler_;
  mojo::Remote<mojom::SearchHandler> handler_remote_;
  FakeObserver observer_;
};

TEST_F(HelpAppSearchHandlerTest, UpdateAndSearch) {
  // Add some search tags.
  std::vector<mojom::SearchConceptPtr> search_concepts;
  mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
      /*id=*/"test-id-1",
      /*title=*/u"Title 1",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Test tag", u"Tag 2"},
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  mojom::SearchConceptPtr new_concept_2 = mojom::SearchConcept::New(
      /*id=*/"test-id-2",
      /*title=*/u"Title 2",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Another test tag"},
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  search_concepts.push_back(std::move(new_concept_1));
  search_concepts.push_back(std::move(new_concept_2));

  bool callback_done = false;
  search_tag_registry_.Update(
      search_concepts,
      base::BindOnce([](bool* callback_done) { *callback_done = true; },
                     &callback_done));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(callback_done);
  EXPECT_EQ(1u, observer_.num_calls());

  std::vector<mojom::SearchResultPtr> search_results;

  // 2 results should be available for a "test tag" query.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"test tag",
              /*max_num_results=*/3u, &search_results);
  EXPECT_EQ(search_results.size(), 2u);

  // Limit results to 1 max and ensure that only 1 result is returned.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"test tag",
              /*max_num_results=*/1u, &search_results);
  EXPECT_EQ(search_results.size(), 1u);

  // Search for a query which should return no results.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("QueryWithNoResults"),
              /*max_num_results=*/3u, &search_results);
  EXPECT_TRUE(search_results.empty());
}

TEST_F(HelpAppSearchHandlerTest, SearchResultMetadata) {
  // Add some search tags.
  std::vector<mojom::SearchConceptPtr> search_concepts;
  mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
      /*id=*/"test-id-1",
      /*title=*/u"Title 1",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Test tag", u"Printing"},
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  search_concepts.push_back(std::move(new_concept_1));

  search_tag_registry_.Update(search_concepts, base::BindOnce([]() {}));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  std::vector<mojom::SearchResultPtr> search_results;
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"Printing",
              /*max_num_results=*/3u, &search_results);

  EXPECT_EQ(search_results.size(), 1u);
  EXPECT_EQ(search_results[0]->id, "test-id-1");
  EXPECT_EQ(search_results[0]->title, u"Title 1");
  EXPECT_EQ(search_results[0]->main_category, u"Help");
  EXPECT_EQ(search_results[0]->locale, "");
  EXPECT_GT(search_results[0]->relevance_score, 0.01);
}

TEST_F(HelpAppSearchHandlerTest, SearchResultOrdering) {
  // Add some search tags.
  std::vector<mojom::SearchConceptPtr> search_concepts;
  mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
      /*id=*/"test-id-less",
      /*title=*/u"Title 1",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"less relevance"},
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  mojom::SearchConceptPtr new_concept_2 = mojom::SearchConcept::New(
      /*id=*/"test-id-more",
      /*title=*/u"Title 2",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"more relevant tag", u"Tag 2"},
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  search_concepts.push_back(std::move(new_concept_1));
  search_concepts.push_back(std::move(new_concept_2));

  search_tag_registry_.Update(search_concepts, base::BindOnce([]() {}));
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  std::vector<mojom::SearchResultPtr> search_results;
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"relevant tag",
              /*max_num_results=*/3u, &search_results);

  // The more relevant concept should be first, but the other concept still has
  // some relevance.
  ASSERT_EQ(search_results.size(), 2u);
  EXPECT_EQ(search_results[0]->id, "test-id-more");
  EXPECT_EQ(search_results[1]->id, "test-id-less");
  EXPECT_GT(search_results[0]->relevance_score,
            search_results[1]->relevance_score);
  EXPECT_GT(search_results[1]->relevance_score, 0.01);
}

}  // namespace help_app
}  // namespace chromeos
