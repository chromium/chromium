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

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "content/public/common/content_switches.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/widget/widget.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/events/platform/platform_event_source.h"
#endif

// TODO(crbug.com/40625383) support Mac for pixel tests.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
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
    cursor_client_->LockCursor();
#if BUILDFLAG(IS_WIN)
    // On Windows, cursor client disable isn't consistently respected, and it's
    // also used to handle touch -> mouse event translation, so use this
    // instead. See crbug.com/333846475 for an example of the problem this
    // solves.
    ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);
#endif
  }

  ScopedMouseDisabler(const ScopedMouseDisabler&) = delete;
  const ScopedMouseDisabler operator=(const ScopedMouseDisabler&) = delete;

  ~ScopedMouseDisabler() {
#if BUILDFLAG(IS_WIN)
    ui::PlatformEventSource::SetIgnoreNativePlatformEvents(false);
#endif
    cursor_client_->UnlockCursor();
    cursor_client_->EnableMouseEvents();
  }

 private:
  raw_ptr<aura::client::CursorClient> cursor_client_;
};
#endif

}  // namespace

TestBrowserUi::TestBrowserUi() {
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  // TODO(crbug.com/40262522): Make these pass with x64 win magic numbers.
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
          switches::kVerifyPixels)) {
    return ui::test::ActionResult::kNotAttempted;
  }

  // Disable and hide cursor to prevent any interference with the
  // screenshots.
  ScopedMouseDisabler disable(view);

  // If there is a focused view, clear it to avoid flakiness caused by
  // unpredictable widget focus (due to test parallelism). It's important to not
  // do this unless necessary, since it will close transient UI like menus,
  // which interferes with tests attempting to verify such UI.
  if (auto* const focus_manager = view->GetWidget()->GetFocusManager();
      focus_manager->GetFocusedView()) {
    focus_manager->ClearFocus();
  }

  // Request that the compositor perform a frame and then wait for it to
  // complete. Because there might not be anything left to draw after waiting
  // for the mouse move above, request compositing so we don't wait forever.
  ui::Compositor* const compositor = view->GetWidget()->GetCompositor();
  compositor->ScheduleFullRedraw();
  ui::DrawWaiterForTest::WaitForCompositingEnded(compositor);

  views::ViewSkiaGoldPixelDiff pixel_diff(screenshot_prefix);
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
