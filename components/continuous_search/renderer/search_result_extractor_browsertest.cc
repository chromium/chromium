// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "components/continuous_search/renderer/search_result_extractor_impl.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/render_view_test.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace continuous_search {

class SearchResultExtractorImplRenderViewTest : public content::RenderViewTest {
 public:
  SearchResultExtractorImplRenderViewTest() = default;
  ~SearchResultExtractorImplRenderViewTest() override = default;

  content::RenderFrame* GetFrame() { return view_->GetMainRenderFrame(); }

  // Loads the contents of `html` and attempts to extract data. Caller should
  // provide the `expected_status` and `expected_results` which are used to
  // verify the extraction behaved as intended. Note that
  // `expected_results->document_url` will be overwritten with the document url
  // once the provided `html` is loaded.
  void LoadHtmlAndExpectExtractedOutput(
      base::StringPiece html,
      mojom::SearchResultExtractor::Status expected_status,
      mojom::CategoryResultsPtr expected_results) {
    LoadHTML(html.data());
    expected_results->document_url =
        GURL(GetFrame()->GetWebFrame()->GetDocument().Url());
    base::RunLoop loop;
    mojom::SearchResultExtractor::Status out_status;
    mojom::CategoryResultsPtr out_results;
    {
      auto* extractor = SearchResultExtractorImpl::Create(GetFrame());
      EXPECT_NE(extractor, nullptr);
      extractor->ExtractCurrentSearchResults(base::BindOnce(
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

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractAdsOnly) {
  auto result1 = mojom::SearchResult::New();
  result1->link = GURL("https://www.example.com/");
  result1->title = u"Hello";

  auto result2 = mojom::SearchResult::New();
  result2->link = GURL("https://www.example1.com/");
  result2->title = u"World";

  auto ad_group = mojom::ResultGroup::New();
  ad_group->label = "Ads";
  ad_group->is_ad_group = true;
  ad_group->results.push_back(std::move(result1));
  ad_group->results.push_back(std::move(result2));

  auto expected_results = mojom::CategoryResults::New();
  expected_results->groups.push_back(std::move(ad_group));

  // If only ads are present the status reports that there are no results as the
  // organic search result extraction working is a requirement. However, the
  // results for the ad group are still stored in the response.
  LoadHtmlAndExpectExtractedOutput(
      R"(<!doctype html>
         <body>
           <div id="tads">
             <div class="mnr-c foo">
               <a href="https://www.example.com/">
                 <div role="heading">
                   <span>Hello</span>
                 </div>
               </a>
               <a href="https://www.skipped_url.com/">
                 <div role="heading">
                   <span>Skipped</span>
                 </div>
               </a>
             </div>
             <div class="mnr-c bar">
               <a href="https://www.example1.com/">
                 <div role="heading">
                   <span>World</span>
                 </div>
               </a>
             </div>
           </div>
         </body>)",
      mojom::SearchResultExtractor::Status::kNoResults,
      std::move(expected_results));
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractAdsAndResults) {
  auto ad_result = mojom::SearchResult::New();
  ad_result->link = GURL("https://www.example.com/");
  ad_result->title = u"Hello";

  auto ad_group = mojom::ResultGroup::New();
  ad_group->label = "Ads";
  ad_group->is_ad_group = true;
  ad_group->results.push_back(std::move(ad_result));

  auto result1 = mojom::SearchResult::New();
  result1->link = GURL("https://www.foo.com/");
  result1->title = u"Foo";

  auto result2 = mojom::SearchResult::New();
  result2->link = GURL("https://www.bar.com/");
  result2->title = u"Bar";

  auto result_group = mojom::ResultGroup::New();
  result_group->label = "Search Results";
  result_group->is_ad_group = false;
  result_group->results.push_back(std::move(result1));
  result_group->results.push_back(std::move(result2));

  auto expected_results = mojom::CategoryResults::New();
  expected_results->category_type = mojom::Category::kOrganic;
  expected_results->groups.push_back(std::move(ad_group));
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
      mojom::SearchResultExtractor::Status::kSuccess,
      std::move(expected_results));
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractResultsOnly) {}

// The tests below this line are intended to test the branching of the
// extractor. The goal is to ensure there are no scenarios where the extraction
// might crash/fail if an almost correct result is presented.

TEST_F(SearchResultExtractorImplRenderViewTest, TestNoRso) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractNoDivs) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractNoMnrCardNoClass) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractNoLinkNoAnchor) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractNoLinkNoHref) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractNoLinkEmptyHref) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractNoLinkWrongScheme) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractNoTitleNoDiv) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractNoTitleNoRole) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest,
       TestExtractNoTitleNotDivHeading) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

TEST_F(SearchResultExtractorImplRenderViewTest, TestExtractNoTitleNoText) {
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
      mojom::SearchResultExtractor::Status::kNoResults,
      mojom::CategoryResults::New());
}

}  // namespace continuous_search
