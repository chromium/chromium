// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_animation_perf_reporter.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"

class SidePanelAnimationPerfReporterUiTest : public InteractiveBrowserTest {
 public:
  SidePanelAnimationPerfReporterUiTest() = default;
  ~SidePanelAnimationPerfReporterUiTest() override = default;

  auto StartCollectingSamples() {
    return Do([this]() {
      histogram_tester_ = std::make_unique<base::HistogramTester>();
    });
  }

  auto OpenBookmarksSidePanel() {
    return Steps(
        PressButton(kToolbarAppMenuButtonElementId),
        SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
        SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkSidePanelItem),
        WaitForEvent(kSidePanelElementId,
                     SidePanel::kOpenAnimationCompletedEvent)
            .SetMustBeVisibleAtStart(false));
  }

  auto CloseSidePanel() {
    return Steps(PressButton(kSidePanelCloseButtonElementId),
                 WaitForEvent(kSidePanelElementId,
                              SidePanel::kCloseAnimationCompletedEvent),
                 WaitForHide(kSidePanelElementId));
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(SidePanelAnimationPerfReporterUiTest, OpenSidePanel) {
  bool has_fps = false;
  RunTestSequence(StartCollectingSamples(), OpenBookmarksSidePanel(),
                  Do([this, &has_fps]() {
                    histogram_tester_->ExpectTotalCount(
                        "SidePanel.TimeOfLongestAnimationStep", 1);
                    has_fps = histogram_tester_->GetTotalCountForPrefix(
                                  "SidePanel.Open.AnimationFPS") > 0;
                  }),
                  CheckResult(
                      [this]() {
                        return histogram_tester_->GetTotalSum(
                            "SidePanel.TimeOfLongestAnimationStep");
                      },
                      testing::Gt(0), "Check longest step is nonzero."),
                  If([&] { return has_fps; },
                     Then(CheckResult(
                         [this]() {
                           return histogram_tester_->GetTotalSum(
                               "SidePanel.Open.AnimationFPS");
                         },
                         testing::Gt(0), "Check fps is nonzero.")),
                     Else(Log("Compositor failed to render during test."))));
}

IN_PROC_BROWSER_TEST_F(SidePanelAnimationPerfReporterUiTest, CloseSidePanel) {
  bool has_fps = false;
  RunTestSequence(OpenBookmarksSidePanel(), StartCollectingSamples(),
                  CloseSidePanel(), Do([this, &has_fps]() {
                    histogram_tester_->ExpectTotalCount(
                        "SidePanel.TimeOfLongestAnimationStep", 1);
                    has_fps = histogram_tester_->GetTotalCountForPrefix(
                                  "SidePanel.Close.AnimationFPS") > 0;
                  }),
                  CheckResult(
                      [this]() {
                        return histogram_tester_->GetTotalSum(
                            "SidePanel.TimeOfLongestAnimationStep");
                      },
                      testing::Gt(0), "Check longest step is nonzero."),
                  If([&] { return has_fps; },
                     Then(CheckResult(
                         [this]() {
                           return histogram_tester_->GetTotalSum(
                               "SidePanel.Close.AnimationFPS");
                         },
                         testing::Gt(0), "Check fps is nonzero.")),
                     Else(Log("Compositor failed to render during test."))));
}
