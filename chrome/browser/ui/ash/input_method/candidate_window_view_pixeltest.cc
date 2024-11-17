// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/i18n/base_i18n_switches.h"
#include "chrome/browser/ui/ash/input_method/candidate_view.h"
#include "chrome/browser/ui/ash/input_method/candidate_window_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace ime {
namespace {

constexpr char kScreenshotPrefix[] =
    "input_method.CandidateWindowViewPixelBrowserTest";

void InitCandidateWindow(size_t page_size, CandidateWindow* candidate_window) {
  candidate_window->set_cursor_position(0);
  candidate_window->set_page_size(page_size);
  candidate_window->mutable_candidates()->clear();
  candidate_window->set_orientation(CandidateWindow::VERTICAL);
}

void InitCandidateWindowWithCandidatesFilled(
    size_t page_size,
    CandidateWindow* candidate_window) {
  InitCandidateWindow(page_size, candidate_window);
  for (size_t i = 0; i < page_size; ++i) {
    CandidateWindow::Entry entry;
    entry.value = u"value " + base::NumberToString16(i);
    entry.label = base::NumberToString16(i);
    entry.annotation = u"ann" + base::NumberToString16(i);
    candidate_window->mutable_candidates()->push_back(entry);
  }
}

// To run a pixel test locally:
//
// browser_tests --gtest_filter="*CandidateWindowViewPixelBrowserTest.*"
//   --enable-pixel-output-in-tests
//   --browser-ui-tests-verify-pixels
//   --skia-gold-local-png-write-directory=/tmp/qa_pixel_test
class CandidateWindowViewPixelBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    if (!command_line->HasSwitch(switches::kVerifyPixels)) {
      GTEST_SKIP() << "A pixel test requires kVerifyPixels flag.";
    }

    pixel_diff_.emplace(kScreenshotPrefix);
  }

 protected:
  std::optional<views::ViewSkiaGoldPixelDiff> pixel_diff_;
};

IN_PROC_BROWSER_TEST_F(CandidateWindowViewPixelBrowserTest, Render) {
  CandidateWindowView view(BrowserView::GetBrowserViewForBrowser(browser())
                               ->GetWidget()
                               ->GetNativeView());
  CandidateWindow candidate_window;
  views::Widget* widget = view.InitWidget();
  const int candidate_window_size = 9;
  InitCandidateWindowWithCandidatesFilled(candidate_window_size,
                                          &candidate_window);
  view.UpdateCandidates(candidate_window);
  view.ShowLookupTable();

  widget->Show();

  views::test::WidgetVisibleWaiter(widget).Wait();

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot("CandidateWindowView", &view));

  widget->CloseNow();
}
}  // namespace

}  // namespace ime
}  // namespace ui
