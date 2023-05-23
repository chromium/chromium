// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_ui.h"

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"

#if defined(USE_AURA)
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"  // nogncheck
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/widget/widget.h"
#endif

// TODO(https://crbug.com/958242) support Mac for pixel tests.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#define SUPPORTS_PIXEL_TEST
#endif

namespace {

// Extracts the |name| argument for ShowUi() from the current test case name.
// E.g. for InvokeUi_name (or DISABLED_InvokeUi_name) returns "name".
std::string NameFromTestCase() {
  const std::string name = base::TestNameWithoutDisabledPrefix(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  size_t underscore = name.find('_');
  return underscore == std::string::npos ? std::string()
                                         : name.substr(underscore + 1);
}

#if defined(USE_AURA)
class ScopedMouseDisabler {
 public:
  explicit ScopedMouseDisabler(views::View* view)
      : cursor_client_(aura::client::GetCursorClient(
            view->GetWidget()->GetNativeWindow()->GetRootWindow())) {
    // Generate a mouse move event to remove any effects caused by mouse enter
    // (e.g. hover). This is necessary as hiding cursor may not emit mouse exit
    // event. (crbug.com/723535).
    ui::test::EventGenerator generator(
        view->GetWidget()->GetNativeWindow()->GetRootWindow());
    generator.MoveMouseTo({0, 0});
    cursor_client_->DisableMouseEvents();
  }
  ScopedMouseDisabler(const ScopedMouseDisabler&) = delete;
  const ScopedMouseDisabler operator=(const ScopedMouseDisabler&) = delete;
  ~ScopedMouseDisabler() { cursor_client_->EnableMouseEvents(); }

 private:
  base::raw_ptr<aura::client::CursorClient> cursor_client_;
};
#endif

}  // namespace

TestBrowserUi::TestBrowserUi() {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  // TODO(1429079): Make these pass with x64 win magic numbers.
  SetPixelMatchAlgorithm(
      std::make_unique<ui::test::FuzzySkiaGoldMatchingAlgorithm>(
          /*max_different_pixels=*/1000, /*pixel_delta_threshold=*/255 * 3));
#elif defined(SUPPORTS_PIXEL_TEST)
  // Default to fuzzy diff. The magic number is chosen based on
  // past experiments.
  SetPixelMatchAlgorithm(
      std::make_unique<ui::test::FuzzySkiaGoldMatchingAlgorithm>(20, 255 * 3));
#endif
}

TestBrowserUi::~TestBrowserUi() = default;

ui::test::ActionResult TestBrowserUi::VerifyPixelUi(
    views::Widget* widget,
    const std::string& screenshot_prefix,
    const std::string& screenshot_name) {
  return VerifyPixelUi(widget->GetContentsView(), screenshot_prefix,
                       screenshot_name);
}

ui::test::ActionResult TestBrowserUi::VerifyPixelUi(
    views::View* view,
    const std::string& screenshot_prefix,
    const std::string& screenshot_name) {
#ifdef SUPPORTS_PIXEL_TEST
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          "browser-ui-tests-verify-pixels"))
    return ui::test::ActionResult::kNotAttempted;

  // Disable and hide cursor to prvent any interference with the
  // screenshots.
  ScopedMouseDisabler disable(view);

  // Clear widget focus to avoid flakiness caused by some widgets having focus
  // and some not due to tests being run in parallel.
  view->GetWidget()->GetFocusManager()->ClearFocus();

  // Request that the compositor perform a frame and then wait for it to
  // complete. Because there might not be anything left to draw after waiting
  // for the mouse move above, request compositing so we don't wait forever.
  ui::Compositor* const compositor = view->GetWidget()->GetCompositor();
  compositor->ScheduleFullRedraw();
  ui::DrawWaiterForTest::WaitForCompositingEnded(compositor);

  views::ViewSkiaGoldPixelDiff pixel_diff;
  pixel_diff.Init(
      // For the CR2023 screenshots add a "CR2023" prefix so that they are
      // compared exclusively with previous CR2023 screenshots. We would like
      // Skia Gold to catch regressions in both CR2023 and non-CR2023.
      // TODO(crbug.com/1444466): remove this after CR2023 launch.
      features::IsChromeRefresh2023() ? "CR2023_" + screenshot_prefix
                                      : screenshot_prefix);
  bool success = pixel_diff.CompareViewScreenshot(screenshot_name, view,
                                                  GetPixelMatchAlgorithm());
  return success ? ui::test::ActionResult::kSucceeded
                 : ui::test::ActionResult::kFailed;
#else
  return ui::test::ActionResult::kKnownIncompatible;
#endif
}

void TestBrowserUi::SetPixelMatchAlgorithm(
    std::unique_ptr<ui::test::SkiaGoldMatchingAlgorithm> algorithm) {
  algorithm_ = std::move(algorithm);
}

void TestBrowserUi::ShowAndVerifyUi() {
  PreShow();
#if BUILDFLAG(IS_WIN)
  // Gold files for pixel tests are for light mode, so if dark mode is not
  // forced, and host is in dark mode, skip test.
  if (!IsInteractiveUi() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode) &&
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    GTEST_SKIP() << "Host is in dark mode; skipping test";
  }
#endif  // BUILDFLAG(IS_WIN)
  ShowUi(NameFromTestCase());
  ASSERT_TRUE(VerifyUi());
  if (IsInteractiveUi())
    WaitForUserDismissal();
  else
    DismissUi();
}

bool TestBrowserUi::IsInteractiveUi() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTestLauncherInteractive);
}
