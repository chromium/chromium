// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/accessibility/accessibility_features.h"

class ReadAnythingSidePanelControllerTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    scoped_feature_list_.InitAndEnableFeature(features::kReadAnything);

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  }

 protected:
  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAnythingSidePanelControllerTest, RegisterReadAnythingEntry) {
  // When CreateAndRegisterEntry() is called, the current tab's side
  // panel registry should contain a kReadAnythingEntry.
  ReadAnythingSidePanelController side_panel_controller(web_contents());
  side_panel_controller.CreateAndRegisterEntry();
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
  ReadAnythingSidePanelController side_panel_controller(web_contents());
  side_panel_controller.CreateAndRegisterEntry();

  auto* registry = SidePanelRegistry::Get(web_contents());
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
  side_panel_controller.DeregisterEntry();
  EXPECT_EQ(registry->GetEntryForKey(
                SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything)),
            nullptr);
}

TEST_F(ReadAnythingSidePanelControllerTest, CreateAndRegisterMultipleTimes) {
  // When CreateAndRegisterEntry() is called multiple times, only
  // one entry should be added to the registry.
  ReadAnythingSidePanelController side_panel_controller(web_contents());
  side_panel_controller.CreateAndRegisterEntry();
  auto* registry = SidePanelRegistry::Get(web_contents());
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
  side_panel_controller.CreateAndRegisterEntry();
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
  side_panel_controller.DeregisterEntry();
  EXPECT_EQ(registry->GetEntryForKey(
                SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything)),
            nullptr);
}

TEST_F(ReadAnythingSidePanelControllerTest, DeregisterEmptyReadAnythingEntry) {
  // When there is no customize chrome entry, calling deregister should
  // not crash.
  ReadAnythingSidePanelController side_panel_controller(web_contents());
  side_panel_controller.DeregisterEntry();
}
