// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test_internal.h"

#include <memory>
#include <sstream>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/devtools_agent_coverage_observer.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/aura/window.h"
#endif

namespace internal {

// Focus supplier that watches for browser activation specifically. For some
// reason, on some platforms, in some circumstances, native widget activation
// isn't properly communicated, so this serves as a backup.
class BrowserWidgetFocusSupplier
    : public views::test::internal::WidgetFocusSupplier,
      public BrowserListObserver {
 public:
  BrowserWidgetFocusSupplier() {
    observation_.Observe(BrowserList::GetInstance());
  }
  ~BrowserWidgetFocusSupplier() override = default;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  void OnBrowserSetLastActive(Browser* browser) override {
    if (auto* const view = BrowserView::GetBrowserViewForBrowser(browser)) {
      if (auto* const widget = view->GetWidget()) {
        if (gfx::NativeView native_view = widget->GetNativeView()) {
          OnWidgetFocusChanged(native_view);
        }
      }
    }
  }

 protected:
  views::Widget::Widgets GetAllWidgets() const override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // On Ash, this call is required to include shell/desktop widgets in
    // addition to other widgets - see documentation in widget_test_aura.cc.
    views::Widget::Widgets result;
    for (const auto& window : ash::Shell::GetAllRootWindows()) {
      views::Widget::GetAllChildWidgets(window->GetRootWindow(), &result);
    }
    return result;
#else
    return views::Widget::Widgets();
#endif
  }

 private:
  base::ScopedObservation<BrowserList, BrowserListObserver> observation_{this};
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(BrowserWidgetFocusSupplier)

InteractiveBrowserTestPrivate::InteractiveBrowserTestPrivate(
    std::unique_ptr<InteractionTestUtilBrowser> test_util)
    : InteractiveViewsTestPrivate(std::move(test_util)) {}

InteractiveBrowserTestPrivate::~InteractiveBrowserTestPrivate() = default;

void InteractiveBrowserTestPrivate::DoTestSetUp() {
  InteractiveViewsTestPrivate::DoTestSetUp();
  widget_focus_suppliers().MaybeRegister<BrowserWidgetFocusSupplier>();
}

void InteractiveBrowserTestPrivate::DoTestTearDown() {
  // Release any remaining instrumented WebContents.
  instrumented_web_contents_.clear();

  // If the test has elected to engage WebUI code coverage, write out the
  // resulting data.
  if (coverage_observer_) {
    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name =
        base::StrCat({test_info->test_suite_name(), ".", test_info->name()});
    // Parameterized tests tend to have slashes in them, which can interfere
    // with file system paths. Change them to something else.
    std::replace(test_name.begin(), test_name.end(), '/', '_');

    LOG(INFO) << "Writing out WebUI code coverage data. If this causes the "
                 "test to time out (b/273290598), you may want to disable "
                 "coverage until  the performance can be improved. If the "
                 "test crashes, a  page touched by the test is likely still "
                 "incompatible with coverage (see b/273545898).";

    coverage_observer_->CollectCoverage(test_name);
  }

  InteractiveViewsTestPrivate::DoTestTearDown();
}

void InteractiveBrowserTestPrivate::MaybeStartWebUICodeCoverage() {
  if (coverage_observer_) {
    return;
  }

  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDevtoolsCodeCoverage)) {
    base::FilePath devtools_code_coverage_dir =
        command_line->GetSwitchValuePath(switches::kDevtoolsCodeCoverage);
    coverage_observer_ = std::make_unique<DevToolsAgentCoverageObserver>(
        devtools_code_coverage_dir);

    LOG(WARNING) << "Starting WebUI code coverage. This may cause the test to "
                    "take longer, possibly resulting in timeouts. Also, due to "
                    "issues with the coverage logic, some WebUI pages may not "
                    "be compatible with WebUI code coverage.";
  }
}

void InteractiveBrowserTestPrivate::AddInstrumentedWebContents(
    std::unique_ptr<WebContentsInteractionTestUtil> instrumented_web_contents) {
  for (const auto& existing : instrumented_web_contents_) {
    CHECK_NE(instrumented_web_contents->page_identifier(),
             existing->page_identifier());
  }
  instrumented_web_contents_.emplace_back(std::move(instrumented_web_contents))
      .get();
}

bool InteractiveBrowserTestPrivate::IsInstrumentedWebContents(
    ui::ElementIdentifier element_id) const {
  for (const auto& existing : instrumented_web_contents_) {
    if (existing->page_identifier() == element_id) {
      return true;
    }
  }
  return false;
}

std::string InteractiveBrowserTestPrivate::DeepQueryToString(
    const WebContentsInteractionTestUtil::DeepQuery& deep_query) {
  std::ostringstream oss;
  oss << "{";
  for (size_t i = 0; i < deep_query.size(); ++i) {
    if (i) {
      oss << ", ";
    }
    oss << "\"" << deep_query[i] << "\"";
  }
  oss << "}";
  return oss.str();
}

gfx::NativeWindow InteractiveBrowserTestPrivate::GetNativeWindowFromElement(
    ui::TrackedElement* el) const {
  gfx::NativeWindow window = gfx::NativeWindow();

  // For instrumented WebContents, we can get the native window directly from
  // the contents object.
  if (el->IsA<TrackedElementWebContents>()) {
    auto* const util = el->AsA<TrackedElementWebContents>()->owner();
    window = util->web_contents()->GetTopLevelNativeWindow();
  }

  // If that did not work, fall back to the base implementation.
  if (!window)
    window = InteractiveViewsTestPrivate::GetNativeWindowFromElement(el);
  return window;
}

gfx::NativeWindow InteractiveBrowserTestPrivate::GetNativeWindowFromContext(
    ui::ElementContext context) const {
  // Defer to the base implementation first, since there may be a cached value
  // that is more accurate than what can be inferred from the context.
  gfx::NativeWindow window =
      InteractiveViewsTestPrivate::GetNativeWindowFromContext(context);

  // If that didn't work, fall back to the top-level browser window for the
  // context (assuming there is one).
  if (!window) {
    if (Browser* const browser =
            InteractionTestUtilBrowser::GetBrowserFromContext(context)) {
      if (BrowserView* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser)) {
        window = browser_view->GetNativeWindow();
      }
    }
  }
  return window;
}

}  // namespace internal
