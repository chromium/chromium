// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "read_anything_side_panel_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockReadAnythingSidePanelControllerObserver
    : public ReadAnythingSidePanelController::Observer {
 public:
  MOCK_METHOD(void, Activate, (bool active), (override));
  MOCK_METHOD(void, OnSidePanelControllerDestroyed, (), (override));
};

class ReadAnythingSidePanelControllerTest : public InProcessBrowserTest {
 public:
  // Wrapper methods around the ReadAnythingSidePanelController. These do
  // nothing more than keep the below tests less verbose (simple pass-throughs).
  ReadAnythingSidePanelController* side_panel_controller() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->read_anything_side_panel_controller();
  }

  void AddObserver(ReadAnythingSidePanelController::Observer* observer) {
    side_panel_controller()->AddObserver(observer);
  }
  void RemoveObserver(ReadAnythingSidePanelController::Observer* observer) {
    side_panel_controller()->RemoveObserver(observer);
  }

 protected:
  MockReadAnythingSidePanelControllerObserver side_panel_controller_observer_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       RegisterReadAnythingEntry) {
  // The tab should have a read anything entry in its side panel.
  EXPECT_EQ(browser()
                ->GetActiveTabInterface()
                ->GetTabFeatures()
                ->side_panel_registry()
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       OnEntryShown_ActivateObservers) {
  AddObserver(&side_panel_controller_observer_);
  SidePanelEntry* entry = browser()
                              ->GetActiveTabInterface()
                              ->GetTabFeatures()
                              ->side_panel_registry()
                              ->GetEntryForKey(SidePanelEntry::Key(
                                  SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(side_panel_controller_observer_, Activate(true)).Times(1);
  side_panel_controller()->OnEntryShown(entry);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       OnEntryHidden_ActivateObservers) {
  AddObserver(&side_panel_controller_observer_);
  SidePanelEntry* entry = browser()
                              ->GetActiveTabInterface()
                              ->GetTabFeatures()
                              ->side_panel_registry()
                              ->GetEntryForKey(SidePanelEntry::Key(
                                  SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(side_panel_controller_observer_, Activate(false)).Times(1);
  side_panel_controller()->OnEntryHidden(entry);
}
