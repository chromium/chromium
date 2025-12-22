// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator_impl.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

class TestSidePanelEntryObserver : public SidePanelEntryObserver {
 public:
  explicit TestSidePanelEntryObserver(SidePanelEntry* entry) : entry_(entry) {
    entry_->AddObserver(this);
  }
  ~TestSidePanelEntryObserver() override {
    if (entry_) {
      entry_->RemoveObserver(this);
    }
  }

  void OnEntryHideCancelled(SidePanelEntry* entry) override {
    hide_cancelled_future_.SetValue();
  }

  bool WaitForHideCancelled() { return hide_cancelled_future_.Wait(); }

 private:
  raw_ptr<SidePanelEntry> entry_;
  base::test::TestFuture<void> hide_cancelled_future_;
};

class GlicSidePanelCoordinatorTest : public InProcessBrowserTest {
 public:
  GlicSidePanelCoordinatorTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
            features::kGlicRollout,
            features::kTabstripComboButton,
            features::kGlicMultiInstance,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)

        },
        {
            features::kGlicLocaleFiltering,
            features::kGlicCountryFiltering,
        });
  }

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Create a dummy view for the Lens entry.
    auto lens_entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::PanelType::kContent,
        SidePanelEntry::Key(SidePanelEntry::Id::kLens),
        base::BindRepeating([](SidePanelEntryScope& scope) {
          return std::make_unique<views::View>();
        }),
        base::BindRepeating([]() { return 250; }));
    registry()->Register(std::move(lens_entry));
  }

  Profile* profile() { return browser()->profile(); }

  GlicEnabling* enabling() {
    return &GlicKeyedServiceFactory::GetGlicKeyedService(profile())->enabling();
  }

  SidePanelRegistry* registry() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->side_panel_registry();
  }

  GlicSidePanelCoordinatorImpl& coordinator() {
    auto* coordinator =
        GlicSidePanelCoordinator::GetForTab(browser()->GetActiveTabInterface());
    CHECK(coordinator);
    return *static_cast<GlicSidePanelCoordinatorImpl*>(coordinator);
  }

  void CallOnGlicEnabledChanged() { coordinator().OnGlicEnabledChanged(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorTest, EntryAdded) {
  ForceSigninAndGlicCapability(profile());
  EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  CallOnGlicEnabledChanged();

  EXPECT_TRUE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));
}

// TODO(crbug.com/460830593): Enable for ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_EntryNotAdded DISABLED_EntryNotAdded
#else
#define MAYBE_EntryNotAdded EntryNotAdded
#endif
IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorTest, MAYBE_EntryNotAdded) {
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));

  CallOnGlicEnabledChanged();

  EXPECT_FALSE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));
}

// TODO(crbug.com/460830593): Enable for ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_EligibilityChangesReflected DISABLED_EligibilityChangesReflected
#else
#define MAYBE_EligibilityChangesReflected EligibilityChangesReflected
#endif
IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorTest,
                       MAYBE_EligibilityChangesReflected) {
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));
  // Start in a state when glic is not enabled. There should ne no side panel
  // entry.
  CallOnGlicEnabledChanged();
  EXPECT_FALSE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));

  // Change state - glic is not enabled. Verify entry is added.
  ForceSigninAndGlicCapability(profile());

  EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
  EXPECT_TRUE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));

  // Change state - glic is not enabled. Verify entry is still there.
  SetGlicCapability(profile(), false);

  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));
  EXPECT_TRUE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorTest,
                       IsGlicSidePanelActiveTest) {
  SidePanelCoordinator::From(browser())->DisableAnimationsForTesting();
  ForceSigninAndGlicCapability(profile());
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
  CallOnGlicEnabledChanged();
  ASSERT_TRUE(registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));

  // Initially, the Glic side panel should not be active.
  EXPECT_FALSE(GlicSidePanelCoordinator::IsGlicSidePanelActive(
      browser()->GetActiveTabInterface()));

  // Show the Glic side panel.
  coordinator().Show();

  // Now, the Glic side panel should be active for the current tab.
  tabs::TabInterface* first_tab = browser()->GetActiveTabInterface();
  EXPECT_TRUE(GlicSidePanelCoordinator::IsGlicSidePanelActive(first_tab));

  // Add a new tab and switch to it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  tabs::TabInterface* second_tab = browser()->GetActiveTabInterface();
  EXPECT_NE(first_tab, second_tab);

  // The Glic side panel should not be active for the new tab.
  EXPECT_FALSE(GlicSidePanelCoordinator::IsGlicSidePanelActive(second_tab));

  // The Glic side panel should still be considered active for the first tab,
  // even though it's backgrounded.
  EXPECT_TRUE(GlicSidePanelCoordinator::IsGlicSidePanelActive(first_tab));

  // Switch back to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // The Glic side panel should be active for the first tab again.
  EXPECT_TRUE(GlicSidePanelCoordinator::IsGlicSidePanelActive(first_tab));

  // Open another tab-scoped side panel entry (Lens). This should make Glic side
  // panel inactive for the current (first) tab.
  SidePanelCoordinator::From(browser())->Show(
      SidePanelEntry::Key(SidePanelEntry::Id::kLens));

  EXPECT_FALSE(GlicSidePanelCoordinator::IsGlicSidePanelActive(
      browser()->GetActiveTabInterface()));

  // Close the Lens side panel. Glic should still be inactive.
  SidePanelCoordinator::From(browser())->Close(
      SidePanelEntry::PanelType::kContent);
  EXPECT_FALSE(GlicSidePanelCoordinator::IsGlicSidePanelActive(
      browser()->GetActiveTabInterface()));

  // Show Glic again.
  coordinator().Show();
  EXPECT_TRUE(GlicSidePanelCoordinator::IsGlicSidePanelActive(
      browser()->GetActiveTabInterface()));
}

class GlicSidePanelCoordinatorStateTest : public GlicSidePanelCoordinatorTest {
 public:
  void SetUpOnMainThread() override {
    GlicSidePanelCoordinatorTest::SetUpOnMainThread();
    ForceSigninAndGlicCapability(profile());
    ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
    CallOnGlicEnabledChanged();
    ASSERT_TRUE(registry()->GetEntryForKey(
        SidePanelEntry::Key(SidePanelEntry::Id::kGlic)));
    state_subscription_ =
        coordinator().AddStateCallback(future_.GetRepeatingCallback());
  }

 protected:
  base::test::TestFuture<GlicSidePanelCoordinator::State> future_;
  base::CallbackListSubscription state_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorStateTest, ShowAndClose) {
  // Initial state should be kClosed.
  EXPECT_EQ(coordinator().state(), GlicSidePanelCoordinator::State::kClosed);

  // Show the panel.
  coordinator().Show();
  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kShown);
  EXPECT_TRUE(coordinator().IsShowing());

  // Close the panel.
  coordinator().Close();
  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kClosed);
  EXPECT_FALSE(coordinator().IsShowing());
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorStateTest, Backgrounded) {
  GlicSidePanelCoordinator& initial_tab_coordinator = coordinator();
  // Show the panel.
  initial_tab_coordinator.Show();
  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kShown);

  // Add a new tab and switch to it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  // The first tab's coordinator should transition to kBackgrounded.
  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kBackgrounded);
  EXPECT_EQ(initial_tab_coordinator.state(),
            GlicSidePanelCoordinator::State::kBackgrounded);

  // Switch back to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // It should transition back to kShown.
  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kShown);
  EXPECT_EQ(initial_tab_coordinator.state(),
            GlicSidePanelCoordinator::State::kShown);
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorStateTest, ShowCloseShowRace) {
  coordinator().Show();
  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kShown);

  auto* entry = registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic));
  TestSidePanelEntryObserver observer(entry);

  // Close and immediately Show. This should result in the panel remaining
  // shown.
  coordinator().Close();
  coordinator().Show();

  EXPECT_TRUE(observer.WaitForHideCancelled());
  EXPECT_EQ(coordinator().state(), GlicSidePanelCoordinator::State::kShown);
  EXPECT_TRUE(coordinator().IsShowing());
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorStateTest, Replaced) {
  coordinator().Show();
  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kShown);

  // Open another side panel entry (Lens). This should replace the Glic
  // side panel and cause it to transition to kClosed.
  SidePanelCoordinator::From(browser())->Show(
      SidePanelEntry::Key(SidePanelEntry::Id::kLens));

  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kClosed);
  EXPECT_FALSE(coordinator().IsShowing());
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorStateTest,
                       CloseFromBackgroundedResetsActiveEntry) {
  GlicSidePanelCoordinator& initial_tab_coordinator = coordinator();
  // Show the panel.
  initial_tab_coordinator.Show();
  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kShown);

  tabs::TabInterface* first_tab = browser()->GetActiveTabInterface();
  EXPECT_TRUE(GlicSidePanelCoordinator::IsGlicSidePanelActive(first_tab));

  // Add a new tab and switch to it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  // The first tab's coordinator should transition to kBackgrounded.
  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kBackgrounded);
  EXPECT_EQ(initial_tab_coordinator.state(),
            GlicSidePanelCoordinator::State::kBackgrounded);

  // Close the panel from the backgrounded state.
  initial_tab_coordinator.Close();

  // Verify the active entry is reset and the state is kClosed.
  EXPECT_FALSE(GlicSidePanelCoordinator::IsGlicSidePanelActive(first_tab));
  EXPECT_EQ(future_.Take(), GlicSidePanelCoordinator::State::kClosed);
  EXPECT_EQ(initial_tab_coordinator.state(),
            GlicSidePanelCoordinator::State::kClosed);
}

}  // namespace
}  // namespace glic
