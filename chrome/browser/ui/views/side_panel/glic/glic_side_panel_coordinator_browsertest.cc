// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class TestGlicSidePanelCoordinator : public GlicSidePanelCoordinator {
 public:
  // Inherit the constructors from the base class.
  using GlicSidePanelCoordinator::GlicSidePanelCoordinator;
  // Expose the protected method as public.
  using GlicSidePanelCoordinator::OnGlicEnabledChanged;
};

class GlicSidePanelCoordinatorTest : public InProcessBrowserTest {
 public:
  GlicSidePanelCoordinatorTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicRollout,
         features::kTabstripComboButton, features::kGlicMultiInstance},
        {});
  }

 protected:
  Profile* profile() { return browser()->profile(); }

  GlicEnabling* enabling() {
    return &GlicKeyedServiceFactory::GetGlicKeyedService(profile())->enabling();
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return browser()->browser_window_features()->side_panel_coordinator();
  }
  SidePanelRegistry* registry() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->side_panel_registry();
  }

  void SetUpOnMainThread() override {
    coordinator_ = std::make_unique<TestGlicSidePanelCoordinator>(
        browser()->GetActiveTabInterface(), registry());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    coordinator_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  actions::ActionItem* glic_action_item() {
    return actions::ActionManager::Get().FindAction(
        kActionSidePanelShowGlic,
        browser()->browser_actions()->root_action_item());
  }

  void CallOnGlicEnabledChanged() { coordinator_->OnGlicEnabledChanged(); }

 private:
  std::unique_ptr<TestGlicSidePanelCoordinator> coordinator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorTest, EntryAdded) {
  ForceSigninAndModelExecutionCapability(profile());
  EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  CallOnGlicEnabledChanged();

  EXPECT_TRUE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));
  EXPECT_TRUE(glic_action_item()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorTest, EntryNotAdded) {
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));

  CallOnGlicEnabledChanged();

  EXPECT_FALSE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));
  EXPECT_FALSE(glic_action_item()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorTest,
                       EligibilityChangesReflected) {
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));

  CallOnGlicEnabledChanged();
  EXPECT_FALSE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));
  EXPECT_FALSE(glic_action_item()->GetVisible());

  ForceSigninAndModelExecutionCapability(profile());

  EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
  EXPECT_TRUE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));
  EXPECT_TRUE(glic_action_item()->GetVisible());

  SetModelExecutionCapability(profile(), false);

  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));
  EXPECT_FALSE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));
  EXPECT_FALSE(glic_action_item()->GetVisible());
}

}  // namespace
}  // namespace glic
