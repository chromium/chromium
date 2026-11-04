// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/uia_element_probe_impl.h"

#include <objbase.h>

#include <windows.h>

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "chrome/services/util_win/public/cpp/uia_element_probe_types.h"
#include "chrome/services/util_win/public/mojom/uia_element_probe.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "ui/gfx/geometry/rect.h"

#include <uiautomation.h>

namespace {

using MeasurementFuture =
    base::test::TestFuture<std::optional<UiaElementMeasurement>>;

// The reply callback takes its argument by const reference.
chrome::mojom::UiaElementProbe::FindElementCallback MeasurementCallback(
    MeasurementFuture& future) {
  return future.GetCallback<const std::optional<UiaElementMeasurement>&>();
}

constexpr wchar_t kChromeClass[] = L"Chrome_UiaElementProbeTestWindow";
constexpr wchar_t kPlainClass[] = L"UiaElementProbeTestWindow";

constexpr DWORD kProcessAccess = PROCESS_QUERY_LIMITED_INFORMATION;

constexpr wchar_t kTargetClass[] = L"UiaElementProbeTestTarget";
constexpr gfx::Rect kTargetBounds(100, 100, 400, 200);
// Win32 control IDs become UIA AutomationIds via UIA's HWND provider.
constexpr int kLabelId = 1200;
constexpr int kButtonId = 2300;
constexpr int kDuplicateId = 3301;
constexpr wchar_t kOutsideCaption[] = L"Outside";
constexpr wchar_t kInsideCaption[] = L"Inside";
constexpr int kLateButtonId = 4401;
constexpr UINT kAddLateButtonMessage = WM_APP;
constexpr int kMissingId = 9999;

base::Process OpenWithExpectedAccess(base::ProcessId pid) {
  return base::Process::OpenWithAccess(pid, kProcessAccess);
}

base::Process OpenCurrentProcess() {
  return OpenWithExpectedAccess(::GetCurrentProcessId());
}

std::wstring TargetTitle(base::ProcessId target_pid) {
  return L"UiaElementProbeTestTarget " + base::NumberToWString(target_pid);
}

base::win::ScopedHandle CreateReadyEvent(base::ProcessId target_pid) {
  const std::wstring name =
      L"UiaElementProbeTestTargetReady." + base::NumberToWString(target_pid);
  return base::win::ScopedHandle(::CreateEventW(
      nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE, name.c_str()));
}

void AddControl(HWND parent,
                const wchar_t* control_class,
                const wchar_t* caption,
                int id,
                const gfx::Rect& bounds) {
  CHECK(::CreateWindowExW(0, control_class, caption, WS_CHILD | WS_VISIBLE,
                          bounds.x(), bounds.y(), bounds.width(),
                          bounds.height(), parent,
                          reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
                          ::GetModuleHandle(nullptr), nullptr));
}

LRESULT CALLBACK TargetWindowProc(HWND hwnd,
                                  UINT message,
                                  WPARAM wparam,
                                  LPARAM lparam) {
  switch (message) {
    case kAddLateButtonMessage:
      AddControl(hwnd, L"Button", L"Late", kLateButtonId,
                 gfx::Rect(290, 50, 100, 30));
      return 0;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      return 0;
  }
  return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

MULTIPROCESS_TEST_MAIN(UiaElementProbeLiveProcess) {
  base::PlatformThread::Sleep(TestTimeouts::action_max_timeout());
  return 0;
}

MULTIPROCESS_TEST_MAIN(UiaElementProbeExitingProcess) {
  return 0;
}

MULTIPROCESS_TEST_MAIN(UiaElementProbeTargetProcess) {
  const HINSTANCE instance = ::GetModuleHandle(nullptr);
  WNDCLASSEXW window_class = {sizeof(window_class)};
  window_class.lpfnWndProc = &TargetWindowProc;
  window_class.hInstance = instance;
  window_class.lpszClassName = kTargetClass;
  CHECK(::RegisterClassExW(&window_class));

  // Popup without activation so the window has no title bar and steals no
  // focus.
  const std::wstring title = TargetTitle(::GetCurrentProcessId());
  HWND hwnd = ::CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kTargetClass, title.c_str(),
      WS_POPUP | WS_BORDER, kTargetBounds.x(), kTargetBounds.y(),
      kTargetBounds.width(), kTargetBounds.height(), nullptr, nullptr, instance,
      nullptr);
  CHECK(hwnd);

  AddControl(hwnd, L"Static", L"Label", kLabelId, gfx::Rect(10, 10, 120, 24));
  AddControl(hwnd, L"Button", L"Button", kButtonId, gfx::Rect(10, 50, 120, 30));
  AddControl(hwnd, L"Button", kOutsideCaption, kDuplicateId,
             gfx::Rect(-200, 90, 120, 30));
  AddControl(hwnd, L"Button", kInsideCaption, kDuplicateId,
             gfx::Rect(150, 90, 120, 30));

  ::ShowWindow(hwnd, SW_SHOWNOACTIVATE);

  base::win::ScopedHandle ready = CreateReadyEvent(::GetCurrentProcessId());
  CHECK(ready.is_valid());
  CHECK(::SetEvent(ready.get()));

  MSG message;
  while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
    ::DispatchMessageW(&message);
  }
  return 0;
}

// Test window that records whether it received WM_GETOBJECT.
class ObservedWindow {
 public:
  explicit ObservedWindow(const wchar_t* window_class) {
    WNDCLASSEXW window_class_info = {sizeof(window_class_info)};
    window_class_info.lpfnWndProc = &ObservedWindow::WindowProc;
    window_class_info.hInstance = ::GetModuleHandle(nullptr);
    window_class_info.lpszClassName = window_class;
    atom_ = ::RegisterClassExW(&window_class_info);
    CHECK(atom_);
    hwnd_ = ::CreateWindowExW(0, MAKEINTATOM(atom_), L"", WS_OVERLAPPED, 0, 0,
                              10, 10, nullptr, nullptr,
                              window_class_info.hInstance, this);
    CHECK(hwnd_);
  }

  ObservedWindow(const ObservedWindow&) = delete;
  ObservedWindow& operator=(const ObservedWindow&) = delete;

  ~ObservedWindow() {
    ::DestroyWindow(hwnd_);
    ::UnregisterClassW(MAKEINTATOM(atom_), ::GetModuleHandle(nullptr));
  }

  HWND hwnd() const { return hwnd_; }
  bool was_asked_for_accessibility() const { return saw_get_object_; }

 private:
  static LRESULT CALLBACK WindowProc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam) {
    if (message == WM_NCCREATE) {
      const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
      ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    } else if (message == WM_GETOBJECT) {
      if (auto* self = reinterpret_cast<ObservedWindow*>(
              ::GetWindowLongPtrW(hwnd, GWLP_USERDATA))) {
        self->saw_get_object_ = true;
      }
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
  }

  ATOM atom_ = 0;
  HWND hwnd_ = nullptr;
  bool saw_get_object_ = false;
};

UiaWindowIdentity MakeWindow(HWND hwnd,
                             base::Process process,
                             const std::wstring& window_class) {
  return {hwnd, std::move(process), window_class};
}

UiaElementQuery MakeQuery(int automation_id,
                          base::TimeDelta wait_for = base::TimeDelta()) {
  UiaElementQuery query;
  query.automation_id = base::NumberToWString(automation_id);
  query.wait_for = wait_for;
  return query;
}

UiaElementQuery MakeFallbackQuery(std::wstring fragment, int control_type) {
  UiaElementQuery query = MakeQuery(kMissingId);
  query.fallback_automation_id_fragment = std::move(fragment);
  query.fallback_control_type = control_type;
  return query;
}

gfx::Rect ScreenBounds(HWND hwnd) {
  RECT rect;
  CHECK(::GetWindowRect(hwnd, &rect));
  return gfx::Rect(rect);
}

class UiaElementProbeImplTest : public base::MultiProcessTest {
 protected:
  UiaElementProbeImplTest()
      : probe_impl_(probe_.BindNewPipeAndPassReceiver()) {}

  void TearDown() override {
    if (target_.IsValid()) {
      target_.Terminate(0, /*wait=*/true);
    }
  }

  std::optional<UiaElementMeasurement> Find(UiaWindowIdentity window,
                                            const UiaElementQuery& query) {
    MeasurementFuture future;
    probe_->FindElement(std::move(window), query, MeasurementCallback(future));
    return future.Take();
  }

  std::optional<UiaElementMeasurement> Find(
      HWND hwnd,
      base::Process process,
      const std::wstring& window_class,
      base::TimeDelta wait_for = base::TimeDelta()) {
    UiaElementQuery query;
    query.automation_id = L"AnyId";
    query.wait_for = wait_for;
    return Find(MakeWindow(hwnd, std::move(process), window_class), query);
  }

  // Sends FindElement(`window`, `query`), which must be a request the service
  // rejects as a bad message, and returns the reported reason.
  std::string BadRequestReason(UiaWindowIdentity window,
                               const UiaElementQuery& query) {
    mojo::test::BadMessageObserver observer;
    probe_->FindElement(std::move(window), query, base::DoNothing());
    return observer.WaitForBadMessage();
  }

  UiaWindowIdentity PlainWindow(const ObservedWindow& window) {
    return MakeWindow(window.hwnd(), OpenCurrentProcess(), kPlainClass);
  }

  static bool IsUiaLoaded() {
    return ::GetModuleHandleW(L"uiautomationcore.dll") != nullptr;
  }

  void StartTarget() {
    target_ = SpawnChild("UiaElementProbeTargetProcess");
    ASSERT_TRUE(target_.IsValid());
    base::win::ScopedHandle ready = CreateReadyEvent(target_.Pid());
    ASSERT_TRUE(ready.is_valid());
    const DWORD timeout_ms =
        static_cast<DWORD>(TestTimeouts::action_timeout().InMilliseconds());
    ASSERT_EQ(::WaitForSingleObject(ready.get(), timeout_ms), WAIT_OBJECT_0);
    target_hwnd_ =
        ::FindWindowW(kTargetClass, TargetTitle(target_.Pid()).c_str());
    ASSERT_TRUE(target_hwnd_);
  }

  UiaWindowIdentity TargetWindow() {
    return MakeWindow(target_hwnd_, OpenWithExpectedAccess(target_.Pid()),
                      kTargetClass);
  }

  void StartFindInTarget(const UiaElementQuery& query,
                         MeasurementFuture& future) {
    probe_->FindElement(TargetWindow(), query, MeasurementCallback(future));
  }

  std::optional<UiaElementMeasurement> FindInTarget(
      const UiaElementQuery& query) {
    return Find(TargetWindow(), query);
  }

  // Posts `message` after a brief delay so the wait path is exercised.
  void PostToTargetSoon(UINT message) {
    task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](HWND hwnd, UINT message) {
              ::PostMessageW(hwnd, message, 0, 0);
            },
            target_hwnd_, message),
        TestTimeouts::tiny_timeout());
  }

  HWND TargetControl(int id) const { return ::GetDlgItem(target_hwnd_, id); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  mojo::Remote<chrome::mojom::UiaElementProbe> probe_;
  UiaElementProbeImpl probe_impl_;
  base::Process target_;
  HWND target_hwnd_ = nullptr;
};

TEST_F(UiaElementProbeImplTest, RejectsAChromeWindowWithoutAsking) {
  const bool uia_loaded_before = IsUiaLoaded();
  ObservedWindow window(kChromeClass);

  EXPECT_EQ(BadRequestReason(
                MakeWindow(window.hwnd(), OpenCurrentProcess(), kChromeClass),
                MakeQuery(kButtonId)),
            "FindElement() for a Chrome window");
  EXPECT_FALSE(window.was_asked_for_accessibility());
  EXPECT_EQ(IsUiaLoaded(), uia_loaded_before);
}

TEST_F(UiaElementProbeImplTest, RejectsANullWindow) {
  EXPECT_EQ(
      BadRequestReason(MakeWindow(nullptr, OpenCurrentProcess(), kPlainClass),
                       MakeQuery(kButtonId)),
      "FindElement() without a window");
}

TEST_F(UiaElementProbeImplTest, RejectsAnEmptyWindowClass) {
  ObservedWindow window(kPlainClass);

  EXPECT_EQ(BadRequestReason(
                MakeWindow(window.hwnd(), OpenCurrentProcess(), std::wstring()),
                MakeQuery(kButtonId)),
            "FindElement() without a window class");
  EXPECT_FALSE(window.was_asked_for_accessibility());
}

// A query naming nothing to find does not deserialize at all, so the pipe is
// closed before FindElement() runs.
TEST_F(UiaElementProbeImplTest, RejectsAQueryWithNothingToFind) {
  ObservedWindow window(kPlainClass);

  EXPECT_THAT(BadRequestReason(PlainWindow(window), UiaElementQuery()),
              testing::HasSubstr("VALIDATION_ERROR_DESERIALIZATION_FAILED"));
  EXPECT_FALSE(window.was_asked_for_accessibility());
}

TEST_F(UiaElementProbeImplTest, RejectsANegativeWait) {
  ObservedWindow window(kPlainClass);

  EXPECT_EQ(BadRequestReason(PlainWindow(window),
                             MakeQuery(kButtonId, base::Seconds(-1))),
            "FindElement() with a negative wait");
  EXPECT_FALSE(window.was_asked_for_accessibility());
}

TEST_F(UiaElementProbeImplTest, RefusesAWindowOfAnotherClass) {
  const bool uia_loaded_before = IsUiaLoaded();
  ObservedWindow window(kPlainClass);

  EXPECT_FALSE(Find(window.hwnd(), OpenCurrentProcess(), L"SomethingElse"));
  EXPECT_FALSE(window.was_asked_for_accessibility());
  EXPECT_EQ(IsUiaLoaded(), uia_loaded_before);
}

TEST_F(UiaElementProbeImplTest, RefusesAWindowOfAnotherProcess) {
  const bool uia_loaded_before = IsUiaLoaded();
  base::Process child = SpawnChild("UiaElementProbeLiveProcess");
  ASSERT_TRUE(child.IsValid());
  base::ScopedClosureRunner terminate_child(base::BindLambdaForTesting(
      [&child] { child.Terminate(0, /*wait=*/true); }));
  base::Process other = OpenWithExpectedAccess(child.Pid());
  ASSERT_TRUE(other.IsValid());
  ObservedWindow window(kPlainClass);

  EXPECT_FALSE(Find(window.hwnd(), std::move(other), kPlainClass));
  EXPECT_FALSE(window.was_asked_for_accessibility());
  EXPECT_EQ(IsUiaLoaded(), uia_loaded_before);
}

TEST_F(UiaElementProbeImplTest, RefusesAnExitedProcess) {
  const bool uia_loaded_before = IsUiaLoaded();
  base::Process child = SpawnChild("UiaElementProbeExitingProcess");
  ASSERT_TRUE(child.IsValid());
  int exit_code = -1;
  ASSERT_TRUE(child.WaitForExit(&exit_code));
  ObservedWindow window(kPlainClass);

  EXPECT_FALSE(Find(window.hwnd(), std::move(child), kPlainClass));
  EXPECT_FALSE(window.was_asked_for_accessibility());
  EXPECT_EQ(IsUiaLoaded(), uia_loaded_before);
}

TEST_F(UiaElementProbeImplTest, RefusesAWindowThatIsGone) {
  const bool uia_loaded_before = IsUiaLoaded();
  HWND hwnd;
  {
    ObservedWindow window(kPlainClass);
    hwnd = window.hwnd();
  }
  ASSERT_FALSE(::IsWindow(hwnd));

  EXPECT_FALSE(Find(hwnd, OpenCurrentProcess(), kPlainClass));
  EXPECT_EQ(IsUiaLoaded(), uia_loaded_before);
}

TEST_F(UiaElementProbeImplTest, RefusesAtOnceEvenWhenAskedToWait) {
  const bool uia_loaded_before = IsUiaLoaded();
  ObservedWindow window(kPlainClass);

  base::TimeTicks start = base::TimeTicks::Now();
  EXPECT_FALSE(Find(window.hwnd(), OpenCurrentProcess(), L"SomethingElse",
                    base::Seconds(10)));
  EXPECT_LT(base::TimeTicks::Now() - start, base::Seconds(5));
  EXPECT_FALSE(window.was_asked_for_accessibility());
  EXPECT_EQ(IsUiaLoaded(), uia_loaded_before);
}

TEST_F(UiaElementProbeImplTest, FindsAnElementByAutomationId) {
  ASSERT_NO_FATAL_FAILURE(StartTarget());

  std::optional<UiaElementMeasurement> measurement =
      FindInTarget(MakeQuery(kButtonId));

  ASSERT_TRUE(measurement);
  EXPECT_EQ(measurement->element_bounds,
            ScreenBounds(TargetControl(kButtonId)));
  EXPECT_EQ(measurement->window_bounds, ScreenBounds(target_hwnd_));
}

TEST_F(UiaElementProbeImplTest, SkipsAnUnusableMatchForAUsableOne) {
  ASSERT_NO_FATAL_FAILURE(StartTarget());
  HWND inside =
      ::FindWindowExW(target_hwnd_, nullptr, L"Button", kInsideCaption);
  ASSERT_TRUE(inside);

  std::optional<UiaElementMeasurement> measurement =
      FindInTarget(MakeQuery(kDuplicateId));

  ASSERT_TRUE(measurement);
  EXPECT_EQ(measurement->element_bounds, ScreenBounds(inside));
}

TEST_F(UiaElementProbeImplTest, FallsBackToAnIdFragment) {
  ASSERT_NO_FATAL_FAILURE(StartTarget());

  std::optional<UiaElementMeasurement> measurement =
      FindInTarget(MakeFallbackQuery(L"230", UIA_ButtonControlTypeId));

  ASSERT_TRUE(measurement);
  EXPECT_EQ(measurement->element_bounds,
            ScreenBounds(TargetControl(kButtonId)));
}

TEST_F(UiaElementProbeImplTest, FallsBackOnlyToTheGivenControlType) {
  ASSERT_NO_FATAL_FAILURE(StartTarget());

  EXPECT_FALSE(
      FindInTarget(MakeFallbackQuery(L"120", UIA_ButtonControlTypeId)));

  std::optional<UiaElementMeasurement> measurement =
      FindInTarget(MakeFallbackQuery(L"120", /*control_type=*/0));
  ASSERT_TRUE(measurement);
  EXPECT_EQ(measurement->element_bounds, ScreenBounds(TargetControl(kLabelId)));
}

TEST_F(UiaElementProbeImplTest, WaitsForAnElementToAppear) {
  ASSERT_NO_FATAL_FAILURE(StartTarget());
  ASSERT_FALSE(TargetControl(kLateButtonId));

  MeasurementFuture future;
  StartFindInTarget(MakeQuery(kLateButtonId, TestTimeouts::action_timeout()),
                    future);
  PostToTargetSoon(kAddLateButtonMessage);

  std::optional<UiaElementMeasurement> measurement = future.Take();
  ASSERT_TRUE(measurement);
  EXPECT_EQ(measurement->element_bounds,
            ScreenBounds(TargetControl(kLateButtonId)));
}

TEST_F(UiaElementProbeImplTest, ANewerCallEndsAPendingWait) {
  ASSERT_NO_FATAL_FAILURE(StartTarget());

  MeasurementFuture waiting;
  StartFindInTarget(
      MakeQuery(kLateButtonId, TestTimeouts::action_max_timeout()), waiting);

  MeasurementFuture newer;
  StartFindInTarget(MakeQuery(kButtonId), newer);

  EXPECT_TRUE(newer.Take());
  ASSERT_TRUE(waiting.IsReady());
  EXPECT_FALSE(waiting.Take());
}

TEST_F(UiaElementProbeImplTest, GivesUpWhenTheWaitRunsOut) {
  ASSERT_NO_FATAL_FAILURE(StartTarget());

  EXPECT_FALSE(
      FindInTarget(MakeQuery(kLateButtonId, TestTimeouts::tiny_timeout())));
}

TEST_F(UiaElementProbeImplTest, GivesUpWhenTheWindowCloses) {
  ASSERT_NO_FATAL_FAILURE(StartTarget());
  const base::TimeTicks start = base::TimeTicks::Now();

  MeasurementFuture future;
  StartFindInTarget(
      MakeQuery(kLateButtonId, TestTimeouts::action_max_timeout()), future);
  PostToTargetSoon(WM_CLOSE);

  EXPECT_FALSE(future.Take());
  EXPECT_LT(base::TimeTicks::Now() - start, TestTimeouts::action_timeout());
}

TEST(UiaElementProbeFragmentTest, MatchesAtIdentifierWordStart) {
  using uia_element_probe_internal::ContainsFragmentAtWordStart;
  EXPECT_TRUE(ContainsFragmentAtWordStart(L"SetDefaultButton", L"setdefault"));
  EXPECT_TRUE(ContainsFragmentAtWordStart(L"setdefault", L"setdefault"));
  EXPECT_TRUE(
      ContainsFragmentAtWordStart(L"Browser_SetDefault_Button", L"setdefault"));
  EXPECT_TRUE(
      ContainsFragmentAtWordStart(L"BrowserSetDefaultButton", L"setdefault"));
  EXPECT_TRUE(ContainsFragmentAtWordStart(L"SETDEFAULT", L"setdefault"));
  EXPECT_TRUE(ContainsFragmentAtWordStart(L"SetDefaultButton", L"SetDefault"));
}

TEST(UiaElementProbeFragmentTest, MatchesAfterAnAcronym) {
  using uia_element_probe_internal::ContainsFragmentAtWordStart;
  EXPECT_TRUE(ContainsFragmentAtWordStart(L"UISetDefault", L"setdefault"));
}

TEST(UiaElementProbeFragmentTest, IgnoresFragmentInsideAWord) {
  using uia_element_probe_internal::ContainsFragmentAtWordStart;
  EXPECT_FALSE(ContainsFragmentAtWordStart(
      L"SystemSettings_DefaultApps_ResetDefaultsAction_Button", L"setdefault"));
  EXPECT_FALSE(ContainsFragmentAtWordStart(L"resetdefault", L"setdefault"));
  EXPECT_FALSE(ContainsFragmentAtWordStart(L"", L"setdefault"));
  EXPECT_FALSE(ContainsFragmentAtWordStart(L"SetDefault", L""));
}

TEST(UiaElementProbeFragmentTest, IgnoresFragmentInsideAnAllCapsWord) {
  using uia_element_probe_internal::ContainsFragmentAtWordStart;
  EXPECT_FALSE(ContainsFragmentAtWordStart(L"RESETDEFAULTS", L"setdefault"));
}

TEST(UiaElementProbeFragmentTest, KeepsLookingPastAnInWordMatch) {
  using uia_element_probe_internal::ContainsFragmentAtWordStart;
  EXPECT_TRUE(
      ContainsFragmentAtWordStart(L"ResetDefaults_SetDefault", L"setdefault"));
}

}  // namespace
