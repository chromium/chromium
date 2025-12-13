// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <oleacc.h>
#include <windows.h>  // Must be before the UIA header.
#include <wrl/client.h>

#include <uiautomation.h>

#include "base/containers/circular_deque.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_variant.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/win/hwnd_util.h"
#include "url/gurl.h"

// We could move this into a utility file in the future if it ends up
// being useful to other tests.
class WinAccessibilityEventMonitor {
 public:
  WinAccessibilityEventMonitor(UINT event_min, UINT event_max);

  WinAccessibilityEventMonitor(const WinAccessibilityEventMonitor&) = delete;
  WinAccessibilityEventMonitor& operator=(const WinAccessibilityEventMonitor&) =
      delete;

  ~WinAccessibilityEventMonitor();

  // Blocks until the next event is received. When it's received, it
  // queries accessibility information about the object that fired the
  // event and populates the event, hwnd, role, state, and name in the
  // passed arguments.
  void WaitForNextEvent(DWORD* out_event,
                        HWND* out_hwnd,
                        UINT* out_role,
                        UINT* out_state,
                        std::string* out_name);

 private:
  void OnWinEventHook(HWINEVENTHOOK handle,
                      DWORD event,
                      HWND hwnd,
                      LONG obj_id,
                      LONG child_id,
                      DWORD event_thread,
                      DWORD event_time);

  static void CALLBACK WinEventHookThunk(HWINEVENTHOOK handle,
                                         DWORD event,
                                         HWND hwnd,
                                         LONG obj_id,
                                         LONG child_id,
                                         DWORD event_thread,
                                         DWORD event_time);

  struct EventInfo {
    DWORD event;
    HWND hwnd;
    LONG obj_id;
    LONG child_id;
  };

  base::circular_deque<EventInfo> event_queue_;
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  HWINEVENTHOOK win_event_hook_handle_;
  static WinAccessibilityEventMonitor* instance_;
};

// static
WinAccessibilityEventMonitor* WinAccessibilityEventMonitor::instance_ = NULL;

WinAccessibilityEventMonitor::WinAccessibilityEventMonitor(UINT event_min,
                                                           UINT event_max) {
  CHECK(!instance_) << "There can be only one instance of"
                    << " WinAccessibilityEventMonitor at a time.";
  instance_ = this;
  win_event_hook_handle_ = SetWinEventHook(
      event_min, event_max, NULL,
      &WinAccessibilityEventMonitor::WinEventHookThunk, GetCurrentProcessId(),
      0,  // Hook all threads
      WINEVENT_OUTOFCONTEXT);
}

WinAccessibilityEventMonitor::~WinAccessibilityEventMonitor() {
  UnhookWinEvent(win_event_hook_handle_);
  instance_ = NULL;
}

void WinAccessibilityEventMonitor::WaitForNextEvent(DWORD* out_event,
                                                    HWND* out_hwnd,
                                                    UINT* out_role,
                                                    UINT* out_state,
                                                    std::string* out_name) {
  if (event_queue_.empty()) {
    loop_runner_ = new content::MessageLoopRunner();
    loop_runner_->Run();
    loop_runner_.reset();
  }
  EventInfo event_info = event_queue_.front();
  event_queue_.pop_front();

  *out_event = event_info.event;
  *out_hwnd = event_info.hwnd;

  Microsoft::WRL::ComPtr<IAccessible> acc_obj;
  base::win::ScopedVariant child_variant;
  CHECK(S_OK == AccessibleObjectFromEvent(event_info.hwnd, event_info.obj_id,
                                          event_info.child_id, &acc_obj,
                                          child_variant.Receive()));

  base::win::ScopedVariant role_variant;
  if (S_OK == acc_obj->get_accRole(child_variant, role_variant.Receive())) {
    *out_role = V_I4(role_variant.ptr());
  } else {
    *out_role = 0;
  }

  base::win::ScopedVariant state_variant;
  if (S_OK == acc_obj->get_accState(child_variant, state_variant.Receive())) {
    *out_state = V_I4(state_variant.ptr());
  } else {
    *out_state = 0;
  }

  base::win::ScopedBstr name_bstr;
  HRESULT hr = acc_obj->get_accName(child_variant, name_bstr.Receive());
  if (S_OK == hr) {
    *out_name = base::WideToUTF8(name_bstr.Get());
  } else {
    *out_name = "";
  }
}

void WinAccessibilityEventMonitor::OnWinEventHook(HWINEVENTHOOK handle,
                                                  DWORD event,
                                                  HWND hwnd,
                                                  LONG obj_id,
                                                  LONG child_id,
                                                  DWORD event_thread,
                                                  DWORD event_time) {
  EventInfo event_info;
  event_info.event = event;
  event_info.hwnd = hwnd;
  event_info.obj_id = obj_id;
  event_info.child_id = child_id;
  event_queue_.push_back(event_info);
  if (loop_runner_.get()) {
    loop_runner_->Quit();
  }
}

// static
void CALLBACK
WinAccessibilityEventMonitor::WinEventHookThunk(HWINEVENTHOOK handle,
                                                DWORD event,
                                                HWND hwnd,
                                                LONG obj_id,
                                                LONG child_id,
                                                DWORD event_thread,
                                                DWORD event_time) {
  if (instance_) {
    instance_->OnWinEventHook(handle, event, hwnd, obj_id, child_id,
                              event_thread, event_time);
  }
}

class NavigationAccessibilityTest : public InProcessBrowserTest {
 public:
  NavigationAccessibilityTest(const NavigationAccessibilityTest&) = delete;
  NavigationAccessibilityTest& operator=(const NavigationAccessibilityTest&) =
      delete;

 protected:
  NavigationAccessibilityTest() = default;
  ~NavigationAccessibilityTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SendKeyPress(ui::KeyboardCode key) {
    gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        native_window, key, false, false, false, false)));
  }

 private:
  base::win::ScopedCOMInitializer com_initializer_;
};

// Tests that when focus is in the omnibox and the user types a url and
// presses enter, no focus events are sent on the old document.
// Disabled due to flaky CHECK failures in
// WinAccessibilityEventMonitor::WaitForNextEvent; see https://crbug.com/791981.
IN_PROC_BROWSER_TEST_F(NavigationAccessibilityTest,
                       DISABLED_TestNavigateToNewUrl) {
  content::ScopedAccessibilityModeOverride scoped_mode(ui::kAXModeComplete);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html;charset=utf-8,"
                      "<head><title>First Page</title></head>")));

  chrome::ExecuteCommand(browser(), IDC_FOCUS_LOCATION);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(embedded_test_server()->GetURL("/english_page.html"));

  OmniboxViewViews* omnibox_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->location_bar()
          ->omnibox_view();
  omnibox_view->SetUserText(base::UTF8ToUTF16(main_url.spec()), false);

  WinAccessibilityEventMonitor monitor(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS);
  SendKeyPress(ui::VKEY_RETURN);

  for (;;) {
    DWORD event;
    HWND hwnd;
    UINT role;
    UINT state;
    std::string name;
    monitor.WaitForNextEvent(&event, &hwnd, &role, &state, &name);

    LOG(INFO) << "Got event: " << " event=" << event << " hwnd=" << hwnd
              << " role=" << role << " state=" << state << " name=" << name;

    // We should get only focus events.
    EXPECT_EQ(static_cast<DWORD>(EVENT_OBJECT_FOCUS), event);

    // We should get only focus events on document objects. (On a page with
    // JavaScript or autofocus, additional focus events would be expected.)
    EXPECT_EQ(static_cast<DWORD>(ROLE_SYSTEM_DOCUMENT), role);

    // We shouldn't get any events on the first page because from the time
    // we start monitoring, the user has already initiated a load to the
    // second page.
    EXPECT_NE("First Page", name);

    // Finish when we get an event on the second page.
    if (name == "This page is in English") {
      LOG(INFO) << "Got event on second page, finishing test.";
      break;
    }
  }
}

class NarratorContainmentEnabledBrowserTest : public InProcessBrowserTest {
 public:
  NarratorContainmentEnabledBrowserTest() = default;

 protected:
  void SetUp() override {
    features_.InitAndEnableFeature(
        ::features::kFixNarratorWebContentContainment);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(NarratorContainmentEnabledBrowserTest,
                       ParentClassNameIsChrome_WidgetWin_1) {
  content::ScopedAccessibilityModeOverride scoped_mode(ui::kAXModeComplete);
  WinAccessibilityEventMonitor monitor(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<!doctype html><html></html>")));

  // Wait until we see the new document appear.
  DWORD ev;
  HWND ev_hwnd;
  UINT role;
  UINT state;
  std::string name;
  do {
    monitor.WaitForNextEvent(&ev, &ev_hwnd, &role, &state, &name);
  } while (!(ev == EVENT_OBJECT_SHOW && role == ROLE_SYSTEM_DOCUMENT));

  // Query UIA starting from the top-level Chrome HWND.
  Microsoft::WRL::ComPtr<IUIAutomation> uia;
  ASSERT_HRESULT_SUCCEEDED(CoCreateInstance(
      CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uia)));

  HWND top_hwnd =
      views::HWNDForNativeWindow(browser()->window()->GetNativeWindow());
  ASSERT_NE(nullptr, top_hwnd);

  Microsoft::WRL::ComPtr<IUIAutomationElement> hwnd_elem;
  ASSERT_HRESULT_SUCCEEDED(uia->ElementFromHandle(top_hwnd, &hwnd_elem));
  ASSERT_TRUE(hwnd_elem);

  // Find the document element in the subtree.
  Microsoft::WRL::ComPtr<IUIAutomationCondition> is_document;
  {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_I4;
    v.lVal = UIA_DocumentControlTypeId;
    ASSERT_HRESULT_SUCCEEDED(uia->CreatePropertyCondition(
        UIA_ControlTypePropertyId, v, &is_document));
  }
  Microsoft::WRL::ComPtr<IUIAutomationElement> document;
  ASSERT_HRESULT_SUCCEEDED(
      hwnd_elem->FindFirst(TreeScope_Subtree, is_document.Get(), &document));
  ASSERT_TRUE(document);

  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> control_walker;
  ASSERT_HRESULT_SUCCEEDED(uia->get_ControlViewWalker(&control_walker));
  Microsoft::WRL::ComPtr<IUIAutomationElement> parent_elem;
  ASSERT_HRESULT_SUCCEEDED(
      control_walker->GetParentElement(document.Get(), &parent_elem));
  ASSERT_TRUE(parent_elem);

  base::win::ScopedBstr class_name;
  ASSERT_HRESULT_SUCCEEDED(
      parent_elem->get_CurrentClassName(class_name.Receive()));
  ASSERT_TRUE(class_name.Get());

  // Windows Narrator’s Scan Mode only contains navigation within web content when the UIA
  // parent of the document reports the class name "Chrome_WidgetWin_1". Chromium currently
  // supplies that via a temporary mitigation in ViewAccessibility::OnViewAddedToWidget(),
  // gated by features::kFixNarratorWebContentContainment.
  //
  // If this test fails:
  //  1) You likely broke Narrator’s web-content containment (users may arrow
  //     out of the page in Scan Mode).
  //  2) If the failure is due to a class name change, update the string we set
  //     in ViewAccessibility::OnViewAddedToWidget() to the new expected value,
  //     and adjust this assertion to match.
  //  3) If the behavior changed or you’re unsure, reach out to the Accessibility team.
  //
  // Notes:
  //  - This intentionally asserts the *exact* UIA class name.
  //  - This is a stopgap until Narrator updates its tab-boundary heuristic.
  //  - See https://crbug.com/443225250 for background.
  EXPECT_STREQ(L"Chrome_WidgetWin_1", class_name.Get());
}
