// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"
#endif

namespace views {
namespace {

#if BUILDFLAG(IS_LINUX)

// Tests window activation/deactivation ATK events (STATE-CHANGE:ACTIVE).
// Uses OnActivationChanged() to simulate activation synchronously (Wayland
// activation is async). The base class force-activates widget() after setup.
class WidgetDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  std::vector<ui::AXPropertyFilter> DefaultFilters() const override {
    std::vector<ui::AXPropertyFilter> filters =
        DumpAccessibilityEventsViewsTestBase::DefaultFilters();

    filters.emplace_back("*", ui::AXPropertyFilter::DENY);
    filters.emplace_back("STATE-CHANGE:ACTIVE:*",
                         ui::AXPropertyFilter::ALLOW_EMPTY);

    return filters;
  }

 protected:
  void SetUpTestViews() override {
    // AT-SPI is not auto-initialized in tests; mark it ready so activation
    // events are emitted rather than postponed.
    ui::AtkUtilAuraLinux::GetInstance()->SetAtSpiReady(true);
    widget()->SetContentsView(std::make_unique<View>());
    widget()->Show();
  }

  void TearDownOnMainThread() override {
    ui::AtkUtilAuraLinux::GetInstance()->SetAtSpiReady(false);
    DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

  DesktopWindowTreeHostPlatform* GetPlatformHost() {
    return static_cast<DesktopWindowTreeHostPlatform*>(
        widget()->GetNativeWindow()->GetHost());
  }
};

IN_PROC_BROWSER_TEST_P(WidgetDumpAccessibilityEventsTest, WindowActivated) {
  BEGIN_RECORDING_EVENTS_OR_SKIP("window-activated");

  GetPlatformHost()->OnActivationChanged(false);
  GetPlatformHost()->OnActivationChanged(true);
  EXPECT_TRUE(widget()->IsActive());
}

IN_PROC_BROWSER_TEST_P(WidgetDumpAccessibilityEventsTest, WindowDeactivated) {
  BEGIN_RECORDING_EVENTS_OR_SKIP("window-deactivated");

  GetPlatformHost()->OnActivationChanged(false);
}

IN_PROC_BROWSER_TEST_P(WidgetDumpAccessibilityEventsTest,
                       NoEventsWhenAtSpiNotReady) {
  ui::AtkUtilAuraLinux::GetInstance()->SetAtSpiReady(false);

  BEGIN_RECORDING_EVENTS_OR_SKIP("no-events-when-atspi-not-ready");

  GetPlatformHost()->OnActivationChanged(false);
  GetPlatformHost()->OnActivationChanged(true);
}

IN_PROC_BROWSER_TEST_P(WidgetDumpAccessibilityEventsTest,
                       PostponedEventsRunOnAtSpiReady) {
  // Deactivate first so the subsequent activation isn't idempotent.
  GetPlatformHost()->OnActivationChanged(false);
  base::RunLoop().RunUntilIdle();

  ui::AtkUtilAuraLinux::GetInstance()->SetAtSpiReady(false);

  GetPlatformHost()->OnActivationChanged(true);
  base::RunLoop().RunUntilIdle();

  auto recorder = CreateEventRecorder();
  ASSERT_TRUE(recorder);
  recorder->ListenToEvents(base::DoNothing());

  // SetAtSpiReady(true) triggers RunPostponedEvents().
  ui::AtkUtilAuraLinux::GetInstance()->SetAtSpiReady(true);

  recorder->StopListeningToEvents();

  bool found_active_true = false;
  for (const auto& log : recorder->GetEventLogs()) {
    if (log.find("STATE-CHANGE:ACTIVE:TRUE") != std::string::npos) {
      found_active_true = true;
      break;
    }
  }
  EXPECT_TRUE(found_active_true)
      << "Postponed STATE-CHANGE:ACTIVE:TRUE should fire when AT-SPI becomes "
         "ready via RunPostponedEvents";
}

IN_PROC_BROWSER_TEST_P(WidgetDumpAccessibilityEventsTest,
                       EventsFiredWhenAtSpiReady) {
  ASSERT_TRUE(ui::AtkUtilAuraLinux::GetInstance()->IsAtSpiReady());

  auto recorder = CreateEventRecorder();
  ASSERT_TRUE(recorder);
  recorder->ListenToEvents(base::DoNothing());

  GetPlatformHost()->OnActivationChanged(false);
  GetPlatformHost()->OnActivationChanged(true);
  base::RunLoop().RunUntilIdle();

  recorder->StopListeningToEvents();

  int active_event_count = 0;
  for (const auto& log : recorder->GetEventLogs()) {
    if (log.find("STATE-CHANGE:ACTIVE") != std::string::npos) {
      active_event_count++;
    }
  }
  EXPECT_GE(active_event_count, 1)
      << "STATE-CHANGE:ACTIVE events should fire when AT-SPI is ready";
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WidgetDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

#endif  // BUILDFLAG(IS_LINUX)

}  // namespace
}  // namespace views
