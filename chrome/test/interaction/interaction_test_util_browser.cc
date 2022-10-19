// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_test_util_browser.h"

#include <memory>

#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/interaction/interaction_test_util_mac.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define SUPPORTS_PIXEL_TESTS 1
#include "chrome/browser/ui/test/test_browser_ui.h"
#else
#define SUPPORTS_PIXEL_TESTS 0
#endif

namespace {

#if SUPPORTS_PIXEL_TESTS

// Facilitates pixel testing with more versatile naming than TestBrowserUi.
class PixelTestUi : public TestBrowserUi {
 public:
  PixelTestUi(views::View* view,
              const std::string& screenshot_name,
              const std::string& baseline)
      : view_(view), screenshot_name_(screenshot_name), baseline_(baseline) {}
  ~PixelTestUi() override = default;

  // TestBrowserUi:
  void ShowUi(const std::string& name) override { NOTREACHED(); }
  void WaitForUserDismissal() override { NOTREACHED(); }

  bool VerifyUi() override {
    auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const std::string test_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});
    const std::string screenshot_name =
        screenshot_name_.empty()
            ? baseline_
            : base::StrCat({screenshot_name_, "_", baseline_});
    return VerifyPixelUi(view_, test_name, screenshot_name);
  }

 private:
  base::raw_ptr<views::View> view_ = nullptr;
  std::string screenshot_name_;
  std::string baseline_;
};

#endif  // SUPPORTS_PIXEL_TESTS

}  // namespace

InteractionTestUtilBrowser::InteractionTestUtilBrowser() {
  AddSimulator(
      std::make_unique<views::test::InteractionTestUtilSimulatorViews>());
#if BUILDFLAG(IS_MAC)
  AddSimulator(std::make_unique<ui::test::InteractionTestUtilSimulatorMac>());
#endif
}

InteractionTestUtilBrowser::~InteractionTestUtilBrowser() = default;

// static
Browser* InteractionTestUtilBrowser::GetBrowserFromContext(
    ui::ElementContext context) {
  BrowserList* const browsers = BrowserList::GetInstance();
  for (Browser* const browser : *browsers) {
    if (browser->window()->GetElementContext() == context)
      return browser;
  }
  return nullptr;
}

// static
bool InteractionTestUtilBrowser::CompareScreenshot(
    ui::TrackedElement* element,
    const std::string& screenshot_name,
    const std::string& baseline) {
#if SUPPORTS_PIXEL_TESTS
  views::View* view = nullptr;
  if (auto* const view_el = element->AsA<views::TrackedElementViews>()) {
    view = view_el->view();
  } else if (auto* const page_el = element->AsA<TrackedElementWebContents>()) {
    view = page_el->owner()->GetWebView();
  }

  CHECK(view);

  PixelTestUi pixel_test_ui(view, screenshot_name, baseline);
  return pixel_test_ui.VerifyUi();
#else  // !SUPPORTS_PIXEL_TESTS
  return true;
#endif
}
