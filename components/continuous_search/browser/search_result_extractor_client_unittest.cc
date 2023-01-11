// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/continuous_search/browser/search_result_extractor_client.h"

#include <utility>

#include "base/functional/callback.h"
#include "components/continuous_search/browser/test/fake_search_result_extractor.h"
#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace continuous_search {

namespace {

const char kUrl[] = "https://www.google.com/search?q=test";

void CheckResponse(SearchResultExtractorClientStatus expected_status,
                   mojom::CategoryResultsPtr expected_results,
                   base::OnceClosure quit,
                   SearchResultExtractorClientStatus status,
                   mojom::CategoryResultsPtr results) {
  EXPECT_EQ(expected_status, status);
  EXPECT_TRUE(expected_results.Equals(results));
  std::move(quit).Run();
}

mojom::CategoryResultsPtr GenerateValidResults(const GURL& document_url) {
  mojom::CategoryResultsPtr expected_results = mojom::CategoryResults::New();
  expected_results->document_url = document_url;
  expected_results->category_type = mojom::Category::kOrganic;
  {
    mojom::ResultGroupPtr result_group = mojom::ResultGroup::New();
    result_group->type = mojom::ResultType::kSearchResults;
    {
      mojom::SearchResultPtr result = mojom::SearchResult::New();
      result->link = GURL("https://www.bar.com/");
      result->title = u"Bar";
      result_group->results.push_back(std::move(result));
    }
    expected_results->groups.push_back(std::move(result_group));
  }
  return expected_results;
}

}  // namespace

class SearchResultExtractorClientRenderViewHostTest
    : public content::RenderViewHostTestHarness {
 public:
  SearchResultExtractorClientRenderViewHostTest() = default;
  ~SearchResultExtractorClientRenderViewHostTest() override = default;

  SearchResultExtractorClientRenderViewHostTest(
      const SearchResultExtractorClientRenderViewHostTest&) = delete;
  SearchResultExtractorClientRenderViewHostTest& operator=(
      const SearchResultExtractorClientRenderViewHostTest&) = delete;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               GURL(kUrl));
  }

  // Overrides the `mojom::SearchResultExtractor` on the main frame with
  // `extractor`. Note `extractor` should outlive any calls made to the
  // interface.
  void OverrideInterface(FakeSearchResultExtractor* extractor) {
    web_contents()
        ->GetPrimaryMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::SearchResultExtractor::Name_,
            base::BindRepeating(&FakeSearchResultExtractor::BindRequest,
                                base::Unretained(extractor)));
  }
};

TEST_F(SearchResultExtractorClientRenderViewHostTest, RequestDataSuccess) {
  mojom::CategoryResultsPtr expected_results = GenerateValidResults(GURL(kUrl));
  FakeSearchResultExtractor fake_extractor;
  fake_extractor.SetResponse(mojom::SearchResultExtractor::Status::kSuccess,
                             expected_results.Clone());
  OverrideInterface(&fake_extractor);

  SearchResultExtractorClient client;
  base::RunLoop loop;
  client.RequestData(
      web_contents(), {mojom::ResultType::kSearchResults},
      base::BindOnce(CheckResponse, SearchResultExtractorClientStatus::kSuccess,
                     std::move(expected_results), loop.QuitClosure()));
  loop.Run();
}

TEST_F(SearchResultExtractorClientRenderViewHostTest,
       RequestDataNoWebContents) {
  SearchResultExtractorClient client;
  base::RunLoop loop;
  client.RequestData(
      nullptr, {mojom::ResultType::kSearchResults},
      base::BindOnce(CheckResponse,
                     SearchResultExtractorClientStatus::kWebContentsGone,
                     mojom::CategoryResults::New(), loop.QuitClosure()));
  loop.Run();
}

TEST_F(SearchResultExtractorClientRenderViewHostTest, RequestDataFailed) {
  mojom::CategoryResultsPtr partial_results = GenerateValidResults(GURL(kUrl));
  FakeSearchResultExtractor fake_extractor;
  fake_extractor.SetResponse(mojom::SearchResultExtractor::Status::kNoResults,
                             std::move(partial_results));
  OverrideInterface(&fake_extractor);

  SearchResultExtractorClient client;
  base::RunLoop loop;
  client.RequestData(
      web_contents(), {mojom::ResultType::kSearchResults},
      base::BindOnce(CheckResponse,
                     SearchResultExtractorClientStatus::kNoResults,
                     mojom::CategoryResults::New(), loop.QuitClosure()));
  loop.Run();
}

TEST_F(SearchResultExtractorClientRenderViewHostTest, RequestDataUrlMismatch) {
  mojom::CategoryResultsPtr bad_results =
      GenerateValidResults(GURL("https://www.baz.com/"));
  FakeSearchResultExtractor fake_extractor;
  fake_extractor.SetResponse(mojom::SearchResultExtractor::Status::kSuccess,
                             std::move(bad_results));
  OverrideInterface(&fake_extractor);

  SearchResultExtractorClient client;
  base::RunLoop loop;
  client.RequestData(
      web_contents(), {mojom::ResultType::kSearchResults},
      base::BindOnce(CheckResponse,
                     SearchResultExtractorClientStatus::kUnexpectedUrl,
                     mojom::CategoryResults::New(), loop.QuitClosure()));
  loop.Run();
}

TEST_F(SearchResultExtractorClientRenderViewHostTest, NonSrpUrl) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/search?q=test"));
  SearchResultExtractorClient client;
  base::RunLoop loop;
  client.RequestData(
      web_contents(), {mojom::ResultType::kSearchResults},
      base::BindOnce(
          CheckResponse,
          SearchResultExtractorClientStatus::kWebContentsHasNonSrpUrl,
          mojom::CategoryResults::New(), loop.QuitClosure()));
  loop.Run();
}

}  // namespace continuous_search
