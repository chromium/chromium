// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_TEST_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_TEST_H_

#include <memory>

#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/mock_vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views_impl.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace organizer_panel::test {

// Provides a fake vertical tab strip for testing organizer panel controller
// behavior.
class FakeVerticalTabStrip : public OrganizerPanelHostView {
  METADATA_HEADER(FakeVerticalTabStrip, OrganizerPanelHostView)
 public:
  FakeVerticalTabStrip();
  ~FakeVerticalTabStrip() override;

  void SetAnimationValue(double value);

  void SetOrganizerPanelView(std::unique_ptr<views::View> panel_view) override;
  std::unique_ptr<views::View> TakeOrganizerPanelView() override;
  bool HasOrganizerPanelView() const override;

 private:
  raw_ptr<views::View> panel_view_ = nullptr;
};

// Provides a fake browser view for testing organizer panel controller behavior.
class FakeBrowserView : public views::View, public views::LayoutDelegate {
  METADATA_HEADER(FakeBrowserView, views::View)
 public:
  explicit FakeBrowserView(BrowserWindowInterface& browser);
  ~FakeBrowserView() override;

  OrganizerTrayView* tray_view() const { return tray_view_; }
  FakeVerticalTabStrip* fake_vertical_tab_strip() {
    return fake_vertical_tab_strip_;
  }

  void SetAnimationValue(double animation_value);

  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  raw_ptr<FakeVerticalTabStrip> fake_vertical_tab_strip_ = nullptr;
  raw_ptr<OrganizerTrayView> tray_view_ = nullptr;
  double animation_value_ = 0.0;
};

// Provides a base class for tests that require a functioning organizer panel
// controller.
class OrganizerPanelTestBase
    : public views::test::InteractiveViewsTestMixin<ChromeViewsTestBase> {
 public:
  OrganizerPanelTestBase();
  ~OrganizerPanelTestBase() override;

  void SetUp() override;
  void TearDown() override;

  StepBuilder TogglePanel();

  StepBuilder SetAnimationValue(double value);

  MultiStep ShowPanel();

  MultiStep HidePanel();

  StepBuilder SetExclusion(int width, int height);

  OrganizerTrayView* tray_view() { return browser_view_->tray_view(); }
  FakeVerticalTabStrip* tab_strip() {
    return browser_view_->fake_vertical_tab_strip();
  }

 protected:
  virtual void BeforeSetPanel() {}

  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> browser_;
  std::unique_ptr<BrowserElementsViewsImpl> browser_elements_;
  std::unique_ptr<BrowserActions> browser_actions_;
  std::unique_ptr<tabs::test::MockVerticalTabStripStateController>
      vertical_tab_strip_controller_;
  std::unique_ptr<BrowserAnimationController> animation_controller_;
  std::unique_ptr<OrganizerPanelController> state_controller_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<FakeBrowserView> browser_view_;
  raw_ptr<views::View> panel_;
  raw_ptr<actions::ActionItem> root_action_;
  std::unique_ptr<base::test::ScopedRunLoopTimeout> run_loop_timeout_;
};

}  // namespace organizer_panel::test

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_TEST_H_
