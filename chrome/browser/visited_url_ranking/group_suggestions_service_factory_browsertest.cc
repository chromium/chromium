// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visited_url_ranking/group_suggestions_service_factory.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace visited_url_ranking {

namespace {

class TestGroupSuggestionsDelegate : public GroupSuggestionsDelegate {
 public:
  TestGroupSuggestionsDelegate() = default;
  ~TestGroupSuggestionsDelegate() override = default;

  // GroupSuggestionsDelegate:
  void ShowSuggestion(const GroupSuggestions& group_suggestions,
                      SuggestionResponseCallback response_callback) override {
    wait_run_loop_.QuitClosure().Run();
  }

  void OnDumpStateForFeedback(const std::string& dump_state) override {}

  void WaitForSuggestion() { wait_run_loop_.Run(); }

 private:
  base::RunLoop wait_run_loop_;
};

}  // namespace

class GroupSuggestionsServiceFactoryTest : public AndroidBrowserTest {
 public:
  GroupSuggestionsServiceFactoryTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGroupSuggestionService,
        {{"group_suggestion_enable_recently_opened", "true"},
         {"group_suggestion_enable_visibility_check", "false"}});
  }
  ~GroupSuggestionsServiceFactoryTest() override = default;

  GroupSuggestionsService* GetService() {
    return GroupSuggestionsServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* profile() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/android/google.html")));
    content::RunAllTasksUntilIdle();
  }

  void AddTab(const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
    std::vector<raw_ptr<TabAndroid, VectorExperimental>> tab_vec;
    TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
    tab_vec.push_back(tab_android);

    TabModel* tab_model =
        TabModelList::GetTabModelForWebContents(web_contents());
    TabAndroid* new_tab = TabAndroid::FromWebContents(web_contents());
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile()));
    content::WebContents* new_web_contents = contents.release();
    content::NavigationController::LoadURLParams params(url);
    params.transition_type =
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED);
    params.has_user_gesture = true;
    new_web_contents->GetController().LoadURLWithParams(params);
    tab_model->CreateTab(new_tab, new_web_contents, /*select=*/true);
#endif
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GroupSuggestionsServiceFactoryTest, SuggestionsShown) {
  TestGroupSuggestionsDelegate delegate;
  GetService()->RegisterDelegate(&delegate, GroupSuggestionsService::Scope());
  GetService()->SetConfigForTesting(base::TimeDelta());
  AddTab(GURL("https://1.com"));
  AddTab(GURL("https://2.com"));
  AddTab(GURL("https://3.com"));
  AddTab(GURL("https://4.com"));
  delegate.WaitForSuggestion();
  GetService()->UnregisterDelegate(&delegate);
}

}  // namespace visited_url_ranking
