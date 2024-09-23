// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>
#include <utility>

#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "components/continuous_search/renderer/config.h"
#include "components/continuous_search/renderer/search_result_extractor_impl.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace continuous_search {

class SearchResultExtractorImplRenderViewTest : public content::RenderViewTest {
 public:
  SearchResultExtractorImplRenderViewTest() = default;
  ~SearchResultExtractorImplRenderViewTest() override = default;

  // Loads the contents of `html` and attempts to extract data. Caller should
  // provide the `expected_status` and `expected_results` which are used to
  // verify the extraction behaved as intended. Note that
  // `expected_results->document_url` will be overwritten with the document url
  // once the provided `html` is loaded.
  void LoadHtmlAndExpectExtractedOutput(
      std::string_view html,
      const std::vector<mojom::ResultType>& result_types,
      mojom::SearchResultExtractor::Status expected_status,
      mojom::CategoryResultsPtr expected_results) {
    LoadHTML(html.data());
    expected_results->document_url =
        GURL(GetMainRenderFrame()->GetWebFrame()->GetDocument().Url());
    base::RunLoop loop;
    mojom::SearchResultExtractor::Status out_status;
    mojom::CategoryResultsPtr out_results;
    {
      auto* extractor = SearchResultExtractorImpl::Create(GetMainRenderFrame());
      EXPECT_NE(extractor, nullptr);
      extractor->ExtractCurrentSearchResults(
          result_types, base::BindOnce(
                            [](base::OnceClosure quit,
                               mojom::SearchResultExtractor::Status* out_status,
                               mojom::CategoryResultsPtr* out_results,
                               mojom::SearchResultExtractor::Status status,
                               mojom::CategoryResultsPtr results) {
                              *out_status = status;
                              *out_results = std::move(results);
                              std::move(quit).Run();
                            },
                            loop.QuitClosure(), base::Unretained(&out_status),
                            base::Unretained(&out_results)));
      loop.Run();
    }
    EXPECT_EQ(expected_status, out_status);
    EXPECT_TRUE(expected_results.Equals(out_results));
  }
};

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractResultsOnly) {
  auto result1 = mojom::SearchResult::New();
  result1->link = GURL("https://www.foo.com/");
  result1->title = u"Foo";

  auto result2 = mojom::SearchResult::New();
  result2->link = GURL("https://www.bar.com/");
  result2->title = u"Bar";

  auto result_group = mojom::ResultGroup::New();
  result_group->type = mojom::ResultType::kSearchResults;
  result_group->results.push_back(std::move(result1));
  result_group->results.push_back(std::move(result2));

  auto expected_results = mojom::CategoryResults::New();
  expected_results->category_type = mojom::Category::kOrganic;
  expected_results->groups.push_back(std::move(result_group));
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div>
             <div></div>
             <div id="tads">
               <div>
                 <div class="mnr-c foo">
                   <a href="https://www.example.com/">
                     <div></div>
                     <div role="heading">
                       <div>Hello</div>
                     </div>
                   </a>
                 </div>
               </div>
             </div>
             <div id="rso">
               <div class="mnr-c">
                 <div></div>
                 <div>
                   <a href="https://www.foo.com/">
                     <div role="heading">Foo </div>
                   </a>
                 </div>
               </div>
               <div class="mnr-c">
                 <div></div>
                 <div>
                   <a href="https://www.bar.com/">
                     <div role="heading">Bar
                     </div>
                   </a>
                 </div>
               </div>
               <div class="alpha">
                 <div></div>
                 <div>
                   <a href="https://www.beta.com/">
                     <div role="heading">Beta</div>
                   </a>
                 </div>
               </div>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kSuccess,
      std::move(expected_results));
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractRelatedSearches) {
  Config config;
  config.related_searches_id = "someid";
  config.related_searches_anchor_classname = "someanchor";
  config.related_searches_title_classname = "sometitle";
  SetConfigForTesting(config);

  auto result1 = mojom::SearchResult::New();
  result1->link = GURL("https://www.example1.com/");
  result1->title = u"Related 1";

  auto result2 = mojom::SearchResult::New();
  result2->link = GURL("https://www.example2.com/");
  result2->title = u"Related 2";

  auto related_searches_group = mojom::ResultGroup::New();
  related_searches_group->type = mojom::ResultType::kRelatedSearches;
  related_searches_group->results.push_back(std::move(result1));
  related_searches_group->results.push_back(std::move(result2));

  auto expected_results = mojom::CategoryResults::New();
  expected_results->category_type = mojom::Category::kOrganic;
  expected_results->groups.push_back(std::move(related_searches_group));

  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="someid">
             <div class="foo">
               <a href="https://www.example1.com/" class="someanchor bar">
                 <div class="sometitle bar">
                   <span>Related 1</span>
                 </div>
               </a>
               <a href="https://www.example2.com/" class="someanchor baz">
                 <div class="sometitle baz">
                   <span>Related 2</span>
                 </div>
               </a>
             </div>
             <div class="mnr-c bar">
               <a href="https://www.example1.com/" class="someanchor buz">
                 <div>
                   <span>Skipped</span>
                 </div>
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kRelatedSearches},
      mojom::SearchResultExtractor::Status::kSuccess,
      std::move(expected_results));
}

// The tests below this line are intended to test the branching of the
// extractor. The goal is to ensure there are no scenarios where the extraction
// might crash/fail if an almost correct result is presented.

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractRelatedSearchesNoId) {
  // No id="w3bYAd".
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div>
             <a href="https://www.example1.com/" class="k8XOCe bar">
               <div class="s75CSd bar">
                 <span>Related 1</span>
               </div>
             </a>
           </div>
         </body>)",
      {mojom::ResultType::kRelatedSearches},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractRelatedSearchesNoAnchors) {
  // No anchors.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="w3bYAd">
           </div>
         </body>)",
      {mojom::ResultType::kRelatedSearches},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractRelatedSearchesNoAnchorClass) {
  // No "k8XOCe" class on anchors.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="w3bYAd">
             <a href="https://www.example1.com/" class="bar">
               <div class="s75CSd bar">
                 <span>Related 1</span>
               </div>
             </a>
           </div>
         </body>)",
      {mojom::ResultType::kRelatedSearches},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractRelatedSearchesNoTitleClass) {
  // No "s75CSd" class on title div.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="w3bYAd">
             <a href="https://www.example1.com/" class="k8XOCe bar">
               <div class="bar">
                 <span>Related 1</span>
               </div>
             </a>
           </div>
         </body>)",
      {mojom::ResultType::kRelatedSearches},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractResultsNoRso) {
  // No class="rso".
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div>
             <div class="mnr-c">
               <a href="https://www.foo.com/">
                 <div role="heading">Foo</div>
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractResultsNoDivs) {
  // No divs inside "rso".
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <a href="https://www.foo.com/">
               <span role="heading">Foo</span>
             </a>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractResultsNoMnrCardNoClass) {
  // No class attribute on inner divs.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <div>
               <a href="https://www.foo.com/">
                 <div role="heading">Foo</div>
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractNoMnrCardNotFirstClass) {
  // mnr-c is the second class.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <div class="foo mnr-c">
               <a href="https://www.foo.com/">
                 <div role="heading">Foo</div>
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractResultsNoLinkNoAnchor) {
  // No anchor inside mnr-c.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <div class="mnr-c">
               <div href="https://www.foo.com/">
                 <div role="heading">Foo</div>
               </div>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractResultsNoLinkNoHref) {
  // No href for the anchor.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <div class="mnr-c">
               <a>
                 <div role="heading">Foo</div>
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractResultsNoLinkEmptyHref) {
  // Empty href.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <div class="mnr-c">
               <a href="">
                 <div role="heading">Foo</div>
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractResultsNoLinkWrongScheme) {
  // href is not http/https.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <div class="mnr-c">
               <a href="mailto:foo@example.com">
                 <div role="heading">Foo</div>
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractResultsNoTitleNoDiv) {
  // No inner div.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <div class="mnr-c">
               <a href="https://www.foo.com/">
                 Foo
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractResultsNoTitleNoRole) {
  // Inner div has no role.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <div class="mnr-c">
               <a href="https://www.foo.com/">
                 <div>Foo</div>
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractResultsNoTitleNotDivHeading) {
  // Not a div, but role="heading".
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <div class="mnr-c">
               <a href="https://www.bar.com/" role="heading">
                 <span role="heading">Bar</span>
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractResultsNoTitleNoText) {
  // No text.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="rso">
             <div class="mnr-c">
               <a href="https://www.baz.com/">
                 <div role="heading"></div>
               </a>
             </div>
           </div>
         </body>)",
      {mojom::ResultType::kSearchResults},
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

}  // namespace continuous_search
