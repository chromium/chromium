// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ui_features.h"
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
  MOCK_METHOD(void, OnCoordinatorDestroyed, (), (override));
};

class ReadAnythingCoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({features::kReadAnything}, {});
    TestWithBrowserView::SetUp();

    side_panel_coordinator_ = browser_view()->side_panel_coordinator();
    side_panel_registry_ =
        SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
    read_anything_coordinator_ =
        ReadAnythingCoordinator::GetOrCreateForBrowser(browser());
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
  bool IsControllerActive() { return GetController()->IsActiveForTesting(); }

 protected:
  raw_ptr<SidePanelCoordinator> side_panel_coordinator_ = nullptr;
  raw_ptr<SidePanelRegistry> side_panel_registry_ = nullptr;
  raw_ptr<ReadAnythingCoordinator> read_anything_coordinator_ = nullptr;

  MockReadAnythingCoordinatorObserver coordinator_observer_;
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

TEST_F(ReadAnythingCoordinatorTest, ActivatesAndDeactivatesController) {
  side_panel_coordinator_->Show(SidePanelEntry::Id::kReadAnything);
  EXPECT_TRUE(IsControllerActive());
  side_panel_coordinator_->Close();
  EXPECT_FALSE(IsControllerActive());

  SidePanelEntry* entry = side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  entry->OnEntryShown();
  EXPECT_TRUE(IsControllerActive());
  entry->OnEntryHidden();
  EXPECT_FALSE(IsControllerActive());
}

#endif  // !defined(ADDRESS_SANITIZER)
