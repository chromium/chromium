// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/capture_mode/search_results_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

std::unique_ptr<views::Widget> CreateWidget() {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  return std::make_unique<views::Widget>(std::move(params));
}

class SunfishBrowserTest : public InProcessBrowserTest {
 public:
  SunfishBrowserTest() = default;
  SunfishBrowserTest(const SunfishBrowserTest&) = delete;
  SunfishBrowserTest& operator=(const SunfishBrowserTest&) = delete;
  ~SunfishBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kSunfishFeature};
};

// Tests the basic functionalities of `SearchResultsView`.
IN_PROC_BROWSER_TEST_F(SunfishBrowserTest, SearchResultsView) {
  auto widget = CreateWidget();
  ChromeCaptureModeDelegate* delegate = ChromeCaptureModeDelegate::Get();
  auto* contents_view =
      widget->SetContentsView(delegate->CreateSearchResultsView());
  auto* search_results_view =
      views::AsViewClass<ash::SearchResultsView>(contents_view);
  ASSERT_TRUE(search_results_view);
}
