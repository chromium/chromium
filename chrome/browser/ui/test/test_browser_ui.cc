// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_ui.h"

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/base/test/ui_controls.h"

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

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
void InstallUIControlsAura() {
#if BUILDFLAG(IS_WIN)
  ui_controls::InstallUIControlsAura(aura::test::CreateUIControlsAura(nullptr));
#else
  ui_controls::EnableUIControls();
#endif
}
#endif

}  // namespace

TestBrowserUi::TestBrowserUi() {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  // Default to fuzzy diff. The magic number is chosen based on
  // past experiments.
  SetPixelMatchAlgorithm(
      std::make_unique<ui::test::FuzzySkiaGoldMatchingAlgorithm>(20, 255 * 3));
#endif
}

TestBrowserUi::~TestBrowserUi() = default;

// TODO(https://crbug.com/958242) support Mac for pixel tests.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
bool TestBrowserUi::VerifyPixelUi(views::Widget* widget,
                                  const std::string& screenshot_prefix,
                                  const std::string& screenshot_name) {
  return VerifyPixelUi(widget->GetContentsView(), screenshot_prefix,
                       screenshot_name);
}

bool TestBrowserUi::VerifyPixelUi(views::View* view,
                                  const std::string& screenshot_prefix,
                                  const std::string& screenshot_name) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          "browser-ui-tests-verify-pixels"))
    return true;

  // Move the mouse away from the dialog to prvent any interference with the
  // screenshots.
  InstallUIControlsAura();
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  EXPECT_TRUE(
      ui_controls::SendMouseMoveNotifyWhenDone(0, 0, run_loop.QuitClosure()));
  run_loop.Run();

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
  pixel_diff.Init(screenshot_prefix);
  return pixel_diff.CompareViewScreenshot(screenshot_name, view,
                                          GetPixelMatchAlgorithm());
}

void TestBrowserUi::SetPixelMatchAlgorithm(
    std::unique_ptr<ui::test::SkiaGoldMatchingAlgorithm> algorithm) {
  algorithm_ = std::move(algorithm);
}
#endif

void TestBrowserUi::ShowAndVerifyUi() {
  PreShow();
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
