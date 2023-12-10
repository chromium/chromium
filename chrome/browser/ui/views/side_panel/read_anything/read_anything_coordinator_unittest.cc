// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"

using testing::_;

class MockReadAnythingCoordinatorObserver
    : public ReadAnythingCoordinator::Observer {
 public:
  MOCK_METHOD(void, Activate, (bool active), (override));
  MOCK_METHOD(void, OnActivePageDistillable, (bool distillable), (override));
  MOCK_METHOD(void, OnCoordinatorDestroyed, (), (override));
};

class ReadAnythingCoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnything, features::kReadAnythingLocalSidePanel}, {});
    TestWithBrowserView::SetUp();

    side_panel_coordinator_ =
        SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
    read_anything_coordinator_ =
        ReadAnythingCoordinator::GetOrCreateForBrowser(browser());

    // Ensure a kReadAnything entry is added to the contextual registry for the
    // first tab.
    AddTabToBrowser(GURL("http://foo1.com"));
    auto* tab_one_registry =
        SidePanelRegistry::Get(browser_view()->GetActiveWebContents());
    contextual_registries_.push_back(tab_one_registry);

    // Ensure a kReadAnything entry is added to the contextual registry for the
    // second tab.
    AddTabToBrowser(GURL("http://foo2.com"));
    auto* tab_two_registry =
        SidePanelRegistry::Get(browser_view()->GetActiveWebContents());
    contextual_registries_.push_back(tab_two_registry);

    // Verify the first tab has one entry, kReadAnything.
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
    SidePanelRegistry* contextual_registry =
        SidePanelRegistry::Get(browser_view()->GetActiveWebContents());
    ASSERT_EQ(contextual_registry->entries().size(), 1u);
    EXPECT_EQ(contextual_registry->entries()[0]->key().id(),
              SidePanelEntry::Id::kReadAnything);

    // Verify the second tab has one entry, kReadAnything.
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
    contextual_registry =
        SidePanelRegistry::Get(browser_view()->GetActiveWebContents());
    ASSERT_EQ(contextual_registry->entries().size(), 1u);
    EXPECT_EQ(contextual_registry->entries()[0]->key().id(),
              SidePanelEntry::Id::kReadAnything);
  }

  // Wrapper methods around the ReadAnythingCoordinator. These do nothing more
  // than keep the below tests less verbose (simple pass-throughs).

  ReadAnythingController* GetController() {
    return read_anything_coordinator_->GetController();
  }
  ReadAnythingModel* GetModel() {
    return read_anything_coordinator_->GetModel();
  }
  void AddObserver(ReadAnythingCoordinator::Observer* observer) {
    read_anything_coordinator_->AddObserver(observer);
  }
  void RemoveObserver(ReadAnythingCoordinator::Observer* observer) {
    read_anything_coordinator_->RemoveObserver(observer);
  }
  std::unique_ptr<views::View> CreateContainerView() {
    return read_anything_coordinator_->CreateContainerView();
  }

  void OnBrowserSetLastActive(Browser* browser) {
    read_anything_coordinator_->OnBrowserSetLastActive(browser);
  }

  void ActivePageDistillable() {
    read_anything_coordinator_->ActivePageDistillable();
  }

  void ActivePageNotDistillable() {
    read_anything_coordinator_->ActivePageNotDistillable();
  }

  void AddTabToBrowser(const GURL& tab_url) {
    AddTab(browser_view()->browser(), tab_url);
    // Remove the companion entry if it present.
    auto* registry =
        SidePanelRegistry::Get(browser_view()->GetActiveWebContents());
    registry->Deregister(
        SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion));
  }

 protected:
  raw_ptr<SidePanelCoordinator, DanglingUntriaged> side_panel_coordinator_ =
      nullptr;
  std::vector<raw_ptr<SidePanelRegistry, DanglingUntriaged>>
      contextual_registries_;
  raw_ptr<ReadAnythingCoordinator, DanglingUntriaged>
      read_anything_coordinator_ = nullptr;

  MockReadAnythingCoordinatorObserver coordinator_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1344891): Fix the memory leak on destruction observed on these
// tests on asan mac.
#if !BUILDFLAG(IS_MAC) || !defined(ADDRESS_SANITIZER)

TEST_F(ReadAnythingCoordinatorTest, ModelAndControllerPersist) {
  // Model and controller are constructed when ReadAnythingCoordinator is
  // constructed, before Side Panel is shown.
  EXPECT_NE(nullptr, GetModel());
  EXPECT_NE(nullptr, GetController());

  side_panel_coordinator_->Show(SidePanelEntry::Id::kReadAnything);
  EXPECT_NE(nullptr, GetModel());
  EXPECT_NE(nullptr, GetController());

  // Model and controller are not destroyed when Side Panel is closed.
  side_panel_coordinator_->Close();
  EXPECT_NE(nullptr, GetModel());
  EXPECT_NE(nullptr, GetController());
}

TEST_F(ReadAnythingCoordinatorTest, ContainerViewsAreUnique) {
  auto view1 = CreateContainerView();
  auto view2 = CreateContainerView();
  EXPECT_NE(view1, view2);
}

TEST_F(ReadAnythingCoordinatorTest, OnCoordinatorDestroyedCalled) {
  AddObserver(&coordinator_observer_);
  EXPECT_CALL(coordinator_observer_, OnCoordinatorDestroyed()).Times(1);
}

TEST_F(ReadAnythingCoordinatorTest, ActivateCalled_ShowAndCloseSidePanel) {
  AddObserver(&coordinator_observer_);

  EXPECT_CALL(coordinator_observer_, Activate(true)).Times(1);
  side_panel_coordinator_->Show(SidePanelEntry::Id::kReadAnything);

  EXPECT_CALL(coordinator_observer_, Activate(false)).Times(1);
  side_panel_coordinator_->Close();
}

TEST_F(ReadAnythingCoordinatorTest,
       ActivateCalled_ShowAndHideReadAnythingEntry) {
  AddObserver(&coordinator_observer_);
  ASSERT_EQ(contextual_registries_.size(), 2u);
  SidePanelEntry* entry = contextual_registries_[0]->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(coordinator_observer_, Activate(true)).Times(1);
  entry->OnEntryShown();

  EXPECT_CALL(coordinator_observer_, Activate(false)).Times(1);
  entry->OnEntryHidden();
}

TEST_F(ReadAnythingCoordinatorTest,
       OnBrowserSetLastActive_SidePanelIsNotVisible) {
  Browser* browser = browser_view()->browser();
  OnBrowserSetLastActive(browser);

  EXPECT_FALSE(side_panel_coordinator_->IsSidePanelShowing());
}

TEST_F(ReadAnythingCoordinatorTest, OnActivePageDistillableCalled) {
  AddObserver(&coordinator_observer_);

  EXPECT_CALL(coordinator_observer_, OnActivePageDistillable(true)).Times(1);
  // Called once when calling ActivePageDistillable and once on destruction.
  EXPECT_CALL(coordinator_observer_, OnActivePageDistillable(false)).Times(2);

  ActivePageDistillable();
  ActivePageNotDistillable();
}

class ReadAnythingCoordinatorScreen2xDataCollectionModeTest
    : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnything, features::kDataCollectionModeForScreen2x},
        {});
    TestWithBrowserView::SetUp();

    side_panel_coordinator_ =
        SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
    read_anything_coordinator_ =
        ReadAnythingCoordinator::GetOrCreateForBrowser(browser());

    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  }

  void OnBrowserSetLastActive(Browser* browser) {
    read_anything_coordinator_->OnBrowserSetLastActive(browser);
  }

 protected:
  raw_ptr<SidePanelCoordinator, DanglingUntriaged> side_panel_coordinator_ =
      nullptr;
  raw_ptr<ReadAnythingCoordinator, DanglingUntriaged>
      read_anything_coordinator_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAnythingCoordinatorScreen2xDataCollectionModeTest,
       OnBrowserSetLastActive_SidePanelIsVisible) {
  Browser* browser = browser_view()->browser();
  OnBrowserSetLastActive(browser);

  EXPECT_TRUE(side_panel_coordinator_->IsSidePanelShowing());
  EXPECT_EQ(SidePanelUI::GetSidePanelUIForBrowser(browser)->GetCurrentEntryId(),
            SidePanelEntryId::kReadAnything);
}

#endif  // !defined(ADDRESS_SANITIZER)
