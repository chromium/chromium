// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/common/extensions/extension_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
    scoped_feature_list_.InitWithFeatures({features::kReadAnything}, {});
    TestWithBrowserView::SetUp();

    side_panel_coordinator_ =
        SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
    side_panel_registry_ =
        SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
    read_anything_coordinator_ =
        ReadAnythingCoordinator::GetOrCreateForBrowser(browser());

    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
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
  raw_ptr<SidePanelRegistry, DanglingUntriaged> side_panel_registry_ = nullptr;
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

TEST_F(ReadAnythingCoordinatorTest,
       ActivateCalled_ShowAndHideReadAnythingEntry) {
  AddObserver(&coordinator_observer_);
  SidePanelEntry* entry = side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(coordinator_observer_, Activate(true)).Times(1);
  entry->OnEntryShown();

  EXPECT_CALL(coordinator_observer_, Activate(false)).Times(1);
  entry->OnEntryHidden();
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(
    ReadAnythingCoordinatorTest,
    // TODO(crbug.com/324143642): Re-enable this test when the docs integration
    // flag is enabled.
    DISABLED_SidePanelShowAndHide_NonLacros_CallEmbeddedA11yExtensionLoader) {
  SidePanelEntry* entry = side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  EXPECT_FALSE(EmbeddedA11yExtensionLoader::GetInstance()->IsExtensionInstalled(
      extension_misc::kReadingModeGDocsHelperExtensionId));

  entry->OnEntryShown();
  EXPECT_TRUE(EmbeddedA11yExtensionLoader::GetInstance()->IsExtensionInstalled(
      extension_misc::kReadingModeGDocsHelperExtensionId));

  // Called once when calling OnEntryHidden and once on destruction.
  entry->OnEntryHidden();
  EXPECT_FALSE(EmbeddedA11yExtensionLoader::GetInstance()->IsExtensionInstalled(
      extension_misc::kReadingModeGDocsHelperExtensionId));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(
    ReadAnythingCoordinatorTest,
    // TODO(crbug.com/324143642): Re-enable this test when the docs integration
    // flag is enabled.
    DISABLED_SidePanelShowAndHide_Lacros_EmbeddedA11yManagerLacrosUpdateReadingModeState) {
  SidePanelEntry* entry = side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  EXPECT_FALSE(
      EmbeddedA11yManagerLacros::GetInstance()->IsReadingModeEnabled());

  entry->OnEntryShown();
  EXPECT_TRUE(EmbeddedA11yManagerLacros::GetInstance()->IsReadingModeEnabled());

  entry->OnEntryHidden();
  EXPECT_FALSE(
      EmbeddedA11yManagerLacros::GetInstance()->IsReadingModeEnabled());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

TEST_F(ReadAnythingCoordinatorTest, WithWebUIFlagDisabled_ShowsViewsToolbar) {
  ASSERT_STREQ("ReadAnythingContainerView",
               CreateContainerView()->GetClassName());
}

class ReadAnythingCoordinatorWebUIToolbarTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnything, features::kReadAnythingWebUIToolbar}, {});
    TestWithBrowserView::SetUp();

    read_anything_coordinator_ =
        ReadAnythingCoordinator::GetOrCreateForBrowser(browser());
  }

  void TearDown() override {
    read_anything_coordinator_ = nullptr;
    TestWithBrowserView::TearDown();
  }

  std::unique_ptr<views::View> CreateContainerView() {
    return read_anything_coordinator_->CreateContainerView();
  }

 protected:
  raw_ptr<ReadAnythingCoordinator> read_anything_coordinator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAnythingCoordinatorWebUIToolbarTest,
       WithWebUIFlagEnabled_ShowsWebUIToolbar) {
  ASSERT_STREQ("ReadAnythingSidePanelWebView",
               CreateContainerView()->GetClassName());
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
