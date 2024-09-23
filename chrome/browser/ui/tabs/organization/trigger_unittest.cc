// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/trigger.h"

#include <memory>

#include "chrome/browser/ui/tabs/organization/trigger_policies.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class TriggerTest : public testing::Test {
 public:
  TriggerTest()
      : profile_(new TestingProfile),
        delegate_(new TestTabStripModelDelegate),
        tab_strip_model_(new TabStripModel(delegate(), profile())) {}

  TestingProfile* profile() { return profile_.get(); }
  TestTabStripModelDelegate* delegate() { return delegate_.get(); }
  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile(),
                                                             nullptr);
  }

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

  content::WebContents* AddTab(GURL url) {
    std::unique_ptr<content::WebContents> contents_unique_ptr =
        CreateWebContents();
    content::WebContents* content_ptr = contents_unique_ptr.get();
    content::WebContentsTester::For(contents_unique_ptr.get())
        ->NavigateAndCommit(url);
    tab_strip_model()->AppendWebContents(std::move(contents_unique_ptr), true);

    return content_ptr;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;

  const std::unique_ptr<TestTabStripModelDelegate> delegate_;
  const std::unique_ptr<TabStripModel> tab_strip_model_;
  tabs::PreventTabFeatureInitialization prevent_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TriggerTest, TriggerHappyPath) {
  auto trigger = std::make_unique<TabOrganizationTrigger>(
      GetTriggerScoringFunction(), GetTriggerScoreThreshold(),
      std::make_unique<DemoTriggerPolicy>());

  // Should not trigger under the score threshold.
  EXPECT_FALSE(trigger->ShouldTrigger(tab_strip_model()));

  // Should trigger the first time over the score threshold.
  for (int i = 0; i < 10; i++) {
    AddTab(GURL("https://www.example.com"));
  }
  EXPECT_TRUE(trigger->ShouldTrigger(tab_strip_model()));

  // Should trigger every time (because DemoTriggerPolicy).
  EXPECT_TRUE(trigger->ShouldTrigger(tab_strip_model()));
}

TEST_F(TriggerTest, DoesntTriggerForEnterprise) {
  scoped_feature_list().InitAndDisableFeature(
      features::kTabOrganizationEnableNudgeForEnterprise);

  auto trigger = std::make_unique<TabOrganizationTrigger>(
      GetTriggerScoringFunction(), GetTriggerScoreThreshold(),
      std::make_unique<DemoTriggerPolicy>());

  // Should trigger when over the score threshold for non-enterprise.
  for (int i = 0; i < 10; i++) {
    AddTab(GURL("https://www.example.com"));
  }
  EXPECT_TRUE(trigger->ShouldTrigger(tab_strip_model()));

  // Shouldn't trigger under the same conditions for enterprise
  TestingProfile::Builder enterprise_profile_builder;
  enterprise_profile_builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> enterprise_profile =
      enterprise_profile_builder.Build();
  std::unique_ptr<TabStripModel> enterprise_tab_strip_model(
      std::make_unique<TabStripModel>(delegate(), enterprise_profile.get()));
  auto enterprise_trigger = std::make_unique<TabOrganizationTrigger>(
      GetTriggerScoringFunction(), GetTriggerScoreThreshold(),
      GetTriggerPolicy(nullptr, enterprise_profile.get()));
  EXPECT_FALSE(
      enterprise_trigger->ShouldTrigger(enterprise_tab_strip_model.get()));
}
