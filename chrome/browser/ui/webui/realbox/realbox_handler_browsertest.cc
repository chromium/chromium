// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/realbox/realbox_handler.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_browser_test_base.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
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
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace {

class BrowserTestWithParam
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::pair<bool, bool>> {
 public:
  BrowserTestWithParam() {
    const bool is_cr23_enabled = GetParam().second;
    if (is_cr23_enabled) {
      scoped_feature_list_.InitAndEnableFeature(features::kChromeRefresh2023);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kChromeRefresh2023);
    }
  }
  BrowserTestWithParam(const BrowserTestWithParam&) = delete;
  BrowserTestWithParam& operator=(const BrowserTestWithParam&) = delete;
  ~BrowserTestWithParam() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Each value listed below represents the following:
// {is_bookmark, is_cr23_enabled}
INSTANTIATE_TEST_SUITE_P(RealboxHandlerIconTest,
                         BrowserTestWithParam,
                         testing::Values(std::pair<bool, bool>(true, true),
                                         std::pair<bool, bool>(true, false),
                                         std::pair<bool, bool>(false, true),
                                         std::pair<bool, bool>(false, false)));

// Tests that all Omnibox match vector icons map to an equivalent SVG for use in
// the NTP Realbox.
IN_PROC_BROWSER_TEST_P(BrowserTestWithParam, MatchVectorIcons) {
  for (int type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
       type != AutocompleteMatchType::NUM_TYPES; type++) {
    AutocompleteMatch match;
    match.type = static_cast<AutocompleteMatchType::Type>(type);
    const bool is_bookmark = BrowserTestWithParam::GetParam().first;
    const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmark);
    const std::string& svg_name =
        RealboxHandler::AutocompleteMatchVectorIconToResourceName(vector_icon);
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

// Tests that all Omnibox Answer vector icons map to an equivalent SVG for use
// in the NTP Realbox.
IN_PROC_BROWSER_TEST_P(BrowserTestWithParam, AnswerVectorIcons) {
  for (int answer_type = SuggestionAnswer::ANSWER_TYPE_DICTIONARY;
       answer_type != SuggestionAnswer::ANSWER_TYPE_TOTAL_COUNT;
       answer_type++) {
    AutocompleteMatch match;
    SuggestionAnswer answer;
    answer.set_type(answer_type);
    match.answer = answer;
    const bool is_bookmark = BrowserTestWithParam::GetParam().first;
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
        RealboxHandler::PedalVectorIconToResourceName(vector_icon);
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
        RealboxHandler::PedalVectorIconToResourceName(vector_icon);
    EXPECT_FALSE(svg_name.empty());
  }
}

class RealboxSearchPreloadBrowserTest : public SearchPrefetchBaseBrowserTest {
 public:
  RealboxSearchPreloadBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &RealboxSearchPreloadBrowserTest::GetWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kSupportSearchSuggestionForPrerender2, {}},
         {kSearchPrefetchServicePrefetching,
          {{"max_attempts_per_caching_duration", "3"},
           {"cache_size", "1"},
           {"device_memory_threshold_MB", "0"}}}},
        /*disabled_features=*/{kSearchPrefetchBlockBeforeHeaders});
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A sink instance that allows Realbox to make IPC without failing DCHECK.
class RealboxSearchBrowserTestPage : public omnibox::mojom::Page {
 public:
  // omnibox::mojom::Page
  void AutocompleteResultChanged(
      omnibox::mojom::AutocompleteResultPtr result) override {}
  void UpdateSelection(
      omnibox::mojom::OmniboxPopupSelectionPtr selection) override {}
  mojo::PendingRemote<omnibox::mojom::Page> GetRemotePage() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<omnibox::mojom::Page> receiver_{this};
};

// Tests the realbox input can trigger prerender and prefetch.
IN_PROC_BROWSER_TEST_F(RealboxSearchPreloadBrowserTest, SearchPreloadSuccess) {
  mojo::Remote<omnibox::mojom::PageHandler> remote_page_handler;
  RealboxSearchBrowserTestPage page;
  RealboxHandler realbox_handler = RealboxHandler(
      remote_page_handler.BindNewPipeAndPassReceiver(), browser()->profile(),
      GetWebContents(), /*metrics_reporter=*/nullptr,
      /*omnibox_controller=*/nullptr);
  realbox_handler.SetPage(page.GetRemotePage());
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetWebContents());

  std::string input_query = "prerender";
  std::string search_terms = "prerender";
  AddNewSuggestionRule(input_query, {search_terms}, /*prefetch_index=*/0,
                       /*prerender_index=*/0);
  auto [_, prerender_url] = GetSearchPrefetchAndNonPrefetch(search_terms);
  // Fake a WebUI input.
  remote_page_handler->QueryAutocomplete(base::ASCIIToUTF16(input_query),
                                         /*prevent_inline_autocomplete=*/false);
  remote_page_handler.FlushForTesting();

  // Prerender and Prefetch should be triggered.
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(prerender_url),
                           SearchPrefetchStatus::kComplete);
  registry_observer.WaitForTrigger(prerender_url);
}
