// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/uia_accessibility_event_waiter.h"

#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder_win_uia.h"
#include "ui/accessibility/platform/uia_registrar_win.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

#include <uiautomation.h>

namespace {

class UiaAccessibilityEventWaiterBrowserTest : public InProcessBrowserTest {
 protected:
  UiaAccessibilityEventWaiterBrowserTest() {
    // Ensure uiautomationcore.dll releases COM references to AXPlatformNodeWin
    // when the test Widget HWND is destroyed during teardown.
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kUiaDisconnectRootProviders);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    scoped_ax_mode_ = content::BrowserAccessibilityState::GetInstance()
                          ->CreateScopedModeForProcess(ui::AXMode::kNativeAPIs);

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 100);
    widget_->Init(std::move(params));

    auto container = std::make_unique<views::View>();
    container->SetLayoutManager(std::make_unique<views::FillLayout>());
    auto button = std::make_unique<views::LabelButton>(
        views::Button::PressedCallback(), u"Test Button");
    button->GetViewAccessibility().SetName(u"Test Button");
    button_ = container->AddChildView(std::move(button));
    widget_->SetContentsView(std::move(container));
    widget_->Show();
  }

  void TearDownOnMainThread() override {
    button_ = nullptr;
    widget_.reset();
    scoped_ax_mode_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  HWND GetHwnd() const { return views::HWNDForWidget(widget_.get()); }

  Microsoft::WRL::ComPtr<IRawElementProviderSimple> GetButtonProvider() const {
    Microsoft::WRL::ComPtr<IRawElementProviderSimple> provider;
    HRESULT hr = button_->GetNativeViewAccessible()->QueryInterface(
        IID_PPV_ARGS(&provider));
    CHECK(SUCCEEDED(hr));
    CHECK(provider);
    return provider;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_ax_mode_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::LabelButton> button_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(UiaAccessibilityEventWaiterBrowserTest,
                       ConcurrentNotificationEventsAndReads) {
  Microsoft::WRL::ComPtr<IRawElementProviderSimple> provider =
      GetButtonProvider();

  UiaAccessibilityWaiterInfo info = {GetHwnd(), L"button", L"Test Button",
                                     ax::mojom::Event::kNone,
                                     UIA_NotificationEventId};
  UiaAccessibilityEventWaiter waiter(info);

  base::win::ScopedBstr activity_id(L"test-activity");
  for (int i = 0; i < 10; ++i) {
    std::wstring text = L"Notification " + base::NumberToWString(i);
    base::win::ScopedBstr display_str(text);
    ::UiaRaiseNotificationEvent(
        provider.Get(), NotificationKind_ActionCompleted,
        NotificationProcessing_All, display_str.Get(), activity_id.Get());
    // Concurrent read on main thread while MTA worker callbacks may be active.
    std::wstring snapshot = waiter.GetNotificationString();
    EXPECT_TRUE(snapshot.empty() || snapshot.starts_with(L"Notification "));
  }

  waiter.Wait();
  EXPECT_TRUE(waiter.GetNotificationString().starts_with(L"Notification "));
}

IN_PROC_BROWSER_TEST_F(UiaAccessibilityEventWaiterBrowserTest,
                       DestructionDuringInFlightEvents) {
  Microsoft::WRL::ComPtr<IRawElementProviderSimple> provider =
      GetButtonProvider();
  base::win::ScopedBstr activity_id(L"test-activity");
  base::win::ScopedBstr display_str(L"In-flight notification");

  // Repeatedly fire automation and notification events right as the waiter is
  // destroyed without calling Wait(), exercising concurrent MatchesNameRole()
  // COM calls on MTA threads against CleanUp() and main-thread STA teardown.
  for (int i = 0; i < 5; ++i) {
    UiaAccessibilityWaiterInfo info = {GetHwnd(), L"button", L"Test Button",
                                       ax::mojom::Event::kAlert, 0};
    auto waiter = std::make_unique<UiaAccessibilityEventWaiter>(info);
    for (int j = 0; j < 5; ++j) {
      ::UiaRaiseAutomationEvent(provider.Get(), UIA_SystemAlertEventId);
      ::UiaRaiseNotificationEvent(
          provider.Get(), NotificationKind_ActionCompleted,
          NotificationProcessing_All, display_str.Get(), activity_id.Get());
    }
    waiter.reset();
  }
}

IN_PROC_BROWSER_TEST_F(UiaAccessibilityEventWaiterBrowserTest,
                       WaitWithTimeoutExpiresCleanly) {
  UiaAccessibilityWaiterInfo info = {
      GetHwnd(), L"button", L"Nonexistent Button", ax::mojom::Event::kAlert, 0};
  UiaAccessibilityEventWaiter waiter(info);
  waiter.WaitWithTimeout(base::Milliseconds(25));
  EXPECT_TRUE(waiter.GetNotificationString().empty());
}

IN_PROC_BROWSER_TEST_F(UiaAccessibilityEventWaiterBrowserTest,
                       WaitRunLoopTimeoutCleansUpWithoutHanging) {
  UiaAccessibilityWaiterInfo info = {
      GetHwnd(), L"button", L"Nonexistent Button", ax::mojom::Event::kAlert, 0};
  UiaAccessibilityEventWaiter waiter(info);
  base::test::ScopedRunLoopTimeout short_timeout(FROM_HERE,
                                                 base::Milliseconds(25));
  EXPECT_NONFATAL_FAILURE(waiter.Wait(), "timed out");
}

IN_PROC_BROWSER_TEST_F(UiaAccessibilityEventWaiterBrowserTest,
                       RecorderSentinelWaitsForPreSentinelEvents) {
  Microsoft::WRL::ComPtr<IRawElementProviderSimple> provider =
      GetButtonProvider();

  ui::AXEventRecorderWinUia recorder{ui::AXTreeSelector(GetHwnd())};
  base::win::ScopedVariant old_name(L"Old Name");
  base::win::ScopedVariant new_name(L"Test Button");
  for (int i = 0; i < 5; ++i) {
    ::UiaRaiseAutomationPropertyChangedEvent(provider.Get(), UIA_NamePropertyId,
                                             old_name, new_name);
    ::UiaRaiseAutomationEvent(provider.Get(), UIA_SystemAlertEventId);
  }
  ::UiaRaiseAutomationEvent(
      provider.Get(),
      ui::UiaRegistrarWin::GetInstance().GetTestCompleteEventId());
  recorder.WaitForDoneRecording();

  int alert_count = 0;
  int property_count = 0;
  for (const std::string& log : recorder.GetEventLogs()) {
    if (log.starts_with("SystemAlert ")) {
      ++alert_count;
    } else if (log.starts_with("Name changed ")) {
      ++property_count;
    }
  }
  // Windows UIA may dispatch the sentinel on another MTA thread before
  // trailing pre-sentinel callbacks enter their handlers, so assert at least
  // one event of each type completed and was recorded before shutdown.
  EXPECT_GE(property_count, 1);
  EXPECT_GE(alert_count, 1);
}

}  // namespace
