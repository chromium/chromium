// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_browser_test_base.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/actions/history_clusters_action.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/tab_switch_action.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace {

class BrowserTestWithParam : public InProcessBrowserTest,
                             public testing::WithParamInterface<bool> {
 public:
  BrowserTestWithParam() = default;
  BrowserTestWithParam(const BrowserTestWithParam&) = delete;
  BrowserTestWithParam& operator=(const BrowserTestWithParam&) = delete;
  ~BrowserTestWithParam() override = default;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(All, BrowserTestWithParam, testing::Bool());

// Tests that all Omnibox match vector icons map to an equivalent SVG for use in
// the NTP Realbox.
IN_PROC_BROWSER_TEST_P(BrowserTestWithParam, MatchVectorIcons) {
  for (int type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
       type != AutocompleteMatchType::NUM_TYPES; type++) {
    AutocompleteMatch match;
    match.type = static_cast<AutocompleteMatchType::Type>(type);
    if (match.type == AutocompleteMatchType::STARTER_PACK) {
      // All STARTER_PACK suggestions should have non-empty vector icons.
      for (int starter_pack_id = TemplateURLStarterPackData::kBookmarks;
           starter_pack_id != TemplateURLStarterPackData::kMaxStarterPackID;
           starter_pack_id++) {
        TemplateURLData turl_data;
        turl_data.starter_pack_id = starter_pack_id;
        TemplateURL turl(turl_data);
        const gfx::VectorIcon& vector_icon =
            match.GetVectorIcon(/*is_bookmark=*/false, &turl);
        const std::string& svg_name =
            RealboxHandler::AutocompleteMatchVectorIconToResourceName(
                vector_icon);
        EXPECT_FALSE(svg_name.empty());
      }
    } else {
      const bool is_bookmark = BrowserTestWithParam::GetParam();
      const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmark);
      const std::string& svg_name =
          RealboxHandler::AutocompleteMatchVectorIconToResourceName(
              vector_icon);
      if (vector_icon.is_empty()) {
        // An empty resource name is effectively a blank icon.
        EXPECT_TRUE(svg_name.empty());
      } else if (is_bookmark) {
        EXPECT_EQ("//resources/images/icon_bookmark.svg", svg_name);
      } else {
        EXPECT_FALSE(svg_name.empty());
      }
    }
  }
}

// Tests that all Omnibox Answer vector icons map to an equivalent SVG for use
// in the NTP Realbox.
IN_PROC_BROWSER_TEST_P(BrowserTestWithParam, AnswerVectorIcons) {
  for (int answer_type = omnibox::ANSWER_TYPE_DICTIONARY;
       answer_type != omnibox::AnswerType_ARRAYSIZE; answer_type++) {
    AutocompleteMatch match;
    SuggestionAnswer answer;
    match.answer_type = static_cast<omnibox::AnswerType>(answer_type);
    match.answer = answer;
    const bool is_bookmark = BrowserTestWithParam::GetParam();
    const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmark);
    const std::string& svg_name =
        RealboxHandler::AutocompleteMatchVectorIconToResourceName(vector_icon);
    if (is_bookmark) {
      EXPECT_EQ("//resources/images/icon_bookmark.svg", svg_name);
    } else {
      EXPECT_FALSE(svg_name.empty());
      EXPECT_NE("search.svg", svg_name);
    }
  }
}

// Tests that all Omnibox Pedal vector icons map to an equivalent SVG for use in
// the NTP Realbox.
IN_PROC_BROWSER_TEST_P(BrowserTestWithParam, PedalVectorIcons) {
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals =
      GetPedalImplementations(/*incognito=*/true, /*guest=*/false,
                              /*testing=*/true);
  for (auto const& it : pedals) {
    const scoped_refptr<OmniboxPedal> pedal = it.second;
    const gfx::VectorIcon& vector_icon = pedal->GetVectorIcon();
    const std::string& svg_name =
        RealboxHandler::ActionVectorIconToResourceName(vector_icon);
    EXPECT_FALSE(svg_name.empty());
  }
}

// Tests that all Omnibox Action vector icons map to an equivalent SVG for use
// in the NTP Realbox.
IN_PROC_BROWSER_TEST_P(BrowserTestWithParam, ActionVectorIcons) {
  std::vector<scoped_refptr<OmniboxAction>> actions = {
      base::MakeRefCounted<history_clusters::HistoryClustersAction>(
          "test", history::ClusterKeywordData()),
      base::MakeRefCounted<TabSwitchAction>(GURL("test")),
  };
  for (auto const& action : actions) {
    const gfx::VectorIcon& vector_icon = action->GetVectorIcon();
    const std::string& svg_name =
        RealboxHandler::ActionVectorIconToResourceName(vector_icon);
    EXPECT_FALSE(svg_name.empty());
  }
}

// A sink instance that allows Realbox to make IPC without failing DCHECK.
class RealboxSearchBrowserTestPage : public searchbox::mojom::Page {
 public:
  // searchbox::mojom::Page
  void AutocompleteResultChanged(
      searchbox::mojom::AutocompleteResultPtr result) override {}
  void UpdateSelection(
      searchbox::mojom::OmniboxPopupSelectionPtr old_selection,
      searchbox::mojom::OmniboxPopupSelectionPtr selection) override {}
  void SetInputText(const std::string& input_text) override {}
  void SetThumbnail(const std::string& thumbnail_url) override {}
  mojo::PendingRemote<searchbox::mojom::Page> GetRemotePage() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<searchbox::mojom::Page> receiver_{this};
};

class RealboxSearchPreloadBrowserTest : public SearchPrefetchBaseBrowserTest {
 public:
  RealboxSearchPreloadBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &RealboxSearchPreloadBrowserTest::GetWebContents,
            base::Unretained(this))) {
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {features::kSupportSearchSuggestionForPrerender2, {}},
        {kSearchPrefetchServicePrefetching,
         {{"max_attempts_per_caching_duration", "3"},
          {"cache_size", "1"},
          {"device_memory_threshold_MB", "0"}}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  // Starts prefetch and then prerender. Returns a pair of prefetch URL and
  // prerender URL.
  std::pair<GURL, GURL> StartPrefetchAndPrerender() {
    mojo::Remote<searchbox::mojom::PageHandler> remote_page_handler;
    RealboxSearchBrowserTestPage page;
    RealboxHandler realbox_handler = RealboxHandler(
        remote_page_handler.BindNewPipeAndPassReceiver(), browser()->profile(),
        GetWebContents(), /*metrics_reporter=*/nullptr,
        /*lens_searchbox_client=*/nullptr,
        /*omnibox_controller=*/nullptr);
    realbox_handler.SetPage(page.GetRemotePage());
    content::test::PrerenderHostRegistryObserver registry_observer(
        *GetWebContents());

    std::string input_query = "prerender";
    std::string search_terms = "prerender";
    AddNewSuggestionRule(input_query, {search_terms}, /*prefetch_index=*/0,
                         /*prerender_index=*/0);
    auto [search_url, _] = GetSearchPrefetchAndNonPrefetch(search_terms);
    // Fake a WebUI input.
    remote_page_handler->QueryAutocomplete(
        base::ASCIIToUTF16(input_query),
        /*prevent_inline_autocomplete=*/false);
    remote_page_handler.FlushForTesting();

    // Prefetch should be triggered.
    WaitUntilStatusChangesTo(GetCanonicalSearchURL(search_url),
                             SearchPrefetchStatus::kComplete);
    const GURL prefetch_url =
        GetRealPrefetchUrlForTesting(GetCanonicalSearchURL(search_url));

    // Prerender should also be triggered.
    const GURL prerender_url = [&registry_observer]() {
      base::flat_set<GURL> triggered_urls =
          registry_observer.GetTriggeredUrls();
      if (triggered_urls.size() == 1) {
        // Prerender has already been triggered.
        return *triggered_urls.begin();
      }
      // Prerender hasn't been triggered yet. Wait until it's triggered.
      EXPECT_TRUE(triggered_urls.empty());
      return registry_observer.WaitForNextTrigger();
    }();

    return std::make_pair(prefetch_url, prerender_url);
  }

  // TODO(crbug.com/40285326): This fails with the field trial testing config.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SearchPrefetchBaseBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class RealboxSearchPreloadWithSearchStatsBrowserTest
    : public RealboxSearchPreloadBrowserTest {
 public:
  RealboxSearchPreloadWithSearchStatsBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        switches::kRemoveSearchboxStatsParamFromPrefetchRequests);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class RealboxSearchPreloadWithoutSearchStatsBrowserTest
    : public RealboxSearchPreloadBrowserTest {
 public:
  RealboxSearchPreloadWithoutSearchStatsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        switches::kRemoveSearchboxStatsParamFromPrefetchRequests);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the realbox input can trigger prerender and prefetch, and those request
// URLs have appropriate parameters.
IN_PROC_BROWSER_TEST_F(RealboxSearchPreloadWithSearchStatsBrowserTest,
                       SearchPreloadSuccess) {
  auto [prefetch_url, prerender_url] = StartPrefetchAndPrerender();

  // Verify the prefetch and prerender URLs.
  // Only the prefetch URL should have the "pf=cs".
  EXPECT_TRUE(base::Contains(prefetch_url.query(), "pf=cs&"));
  EXPECT_FALSE(base::Contains(prerender_url.query(), "pf=cs&"));
  EXPECT_TRUE(base::Contains(prefetch_url.query(), "gs_lcrp="));
  EXPECT_TRUE(base::Contains(prerender_url.query(), "gs_lcrp="));

  // The prefetch should match the prerender.
  EXPECT_TRUE(IsSearchDestinationMatch(GetCanonicalSearchURL(prefetch_url),
                                       browser()->profile(), prerender_url));
}

IN_PROC_BROWSER_TEST_F(RealboxSearchPreloadWithoutSearchStatsBrowserTest,
                       SearchPreloadSuccess) {
  auto [prefetch_url, prerender_url] = StartPrefetchAndPrerender();

  // Verify the prefetch and prerender URLs.
  // Only the prefetch URL should have the "pf=cs".
  EXPECT_TRUE(base::Contains(prefetch_url.query(), "pf=cs&"));
  EXPECT_FALSE(base::Contains(prerender_url.query(), "pf=cs&"));
  // The prefetch URL should not have the "gs_lcrp" if
  // switches::kRemoveSearchboxStatsParamFromPrefetchRequests is true, while the
  // prerender URL should always have that.
  EXPECT_FALSE(base::Contains(prefetch_url.query(), "gs_lcrp="));
  EXPECT_TRUE(base::Contains(prerender_url.query(), "gs_lcrp="));

  // The prefetch should match the prerender.
  EXPECT_TRUE(IsSearchDestinationMatch(GetCanonicalSearchURL(prefetch_url),
                                       browser()->profile(), prerender_url));
}
