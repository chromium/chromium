// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "read_anything_side_panel_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockReadAnythingSidePanelControllerObserver
    : public ReadAnythingSidePanelController::Observer {
 public:
  MOCK_METHOD(void, Activate, (bool active), (override));
  MOCK_METHOD(void, OnSidePanelControllerDestroyed, (), (override));
};

class ReadAnythingSidePanelControllerTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    side_panel_controller_ =
        std::make_unique<ReadAnythingSidePanelController>(web_contents());
  }

  void TearDown() override {
    side_panel_controller_ = nullptr;
    ChromeViewsTestBase::TearDown();
  }

  // Wrapper methods around the ReadAnythingSidePanelController. These do
  // nothing more than keep the below tests less verbose (simple pass-throughs).

  void AddObserver(ReadAnythingSidePanelController::Observer* observer) {
    side_panel_controller_->AddObserver(observer);
  }
  void RemoveObserver(ReadAnythingSidePanelController::Observer* observer) {
    side_panel_controller_->RemoveObserver(observer);
  }

 protected:
  std::unique_ptr<ReadAnythingSidePanelController> side_panel_controller_;
  MockReadAnythingSidePanelControllerObserver side_panel_controller_observer_;
  raw_ptr<SidePanelRegistry> side_panel_registry_;

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(ReadAnythingSidePanelControllerTest, RegisterReadAnythingEntry) {
  // When CreateAndRegisterEntry() is called, the current tab's side
  // panel registry should contain a kReadAnythingEntry.
  side_panel_controller_->CreateAndRegisterEntry();
  auto* registry = SidePanelRegistry::Get(web_contents());
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
}

TEST_F(ReadAnythingSidePanelControllerTest, DeregisterReadAnythingEntry) {
  // When Deregister() is called, there should be no side panel entry
  // in the registry.
  side_panel_controller_->CreateAndRegisterEntry();

  auto* registry = SidePanelRegistry::Get(web_contents());
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
  side_panel_controller_->DeregisterEntry();
  EXPECT_EQ(registry->GetEntryForKey(
                SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything)),
            nullptr);
}

TEST_F(ReadAnythingSidePanelControllerTest, CreateAndRegisterMultipleTimes) {
  // When CreateAndRegisterEntry() is called multiple times, only
  // one entry should be added to the registry.
  side_panel_controller_->CreateAndRegisterEntry();
  auto* registry = SidePanelRegistry::Get(web_contents());
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
  side_panel_controller_->CreateAndRegisterEntry();
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
  side_panel_controller_->DeregisterEntry();
  EXPECT_EQ(registry->GetEntryForKey(
                SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything)),
            nullptr);
}

TEST_F(ReadAnythingSidePanelControllerTest, DeregisterEmptyReadAnythingEntry) {
  // When there is no customize chrome entry, calling deregister should
  // not crash.
  side_panel_controller_->DeregisterEntry();
}

TEST_F(ReadAnythingSidePanelControllerTest,
       OnSidePanelControllerDestroyedCalled) {
  AddObserver(&side_panel_controller_observer_);
  EXPECT_CALL(side_panel_controller_observer_, OnSidePanelControllerDestroyed())
      .Times(1);
}

TEST_F(ReadAnythingSidePanelControllerTest, OnEntryShown_ActivateObservers) {
  AddObserver(&side_panel_controller_observer_);
  side_panel_controller_->CreateAndRegisterEntry();
  auto* registry = SidePanelRegistry::Get(web_contents());
  SidePanelEntry* entry = registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(side_panel_controller_observer_, Activate(true)).Times(1);
  side_panel_controller_->OnEntryShown(entry);
}

TEST_F(ReadAnythingSidePanelControllerTest, OnEntryHidden_ActivateObservers) {
  AddObserver(&side_panel_controller_observer_);
  side_panel_controller_->CreateAndRegisterEntry();
  auto* registry = SidePanelRegistry::Get(web_contents());
  SidePanelEntry* entry = registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(side_panel_controller_observer_, Activate(false)).Times(1);
  side_panel_controller_->OnEntryHidden(entry);
}
