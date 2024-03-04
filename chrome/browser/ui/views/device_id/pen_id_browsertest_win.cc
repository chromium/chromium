// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/desktop_window_tree_host_win_test_api.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/win/pen_event_processor.h"
#include "ui/views/win/pen_id_handler.h"
#include "ui/views/win/test_support/fake_ipen_device.h"
#include "ui/views/win/test_support/fake_ipen_device_statics.h"

using views::FakeIPenDevice;
using views::FakeIPenDeviceStatics;

using Microsoft::WRL::ComPtr;

namespace {

constexpr char kMainTestPageUrlPath[] = "/device_id/test.html";

constexpr int kPointerId1 = 0;
constexpr int kPointerId2 = 1;
constexpr int kPointerId3 = 2;
constexpr int kDeviceId1 = 0;
constexpr int kDeviceId2 = 1;

}  // namespace

class PenIdBrowserTest : public InProcessBrowserTest {
 public:
  PenIdBrowserTest() = default;
  ~PenIdBrowserTest() override = default;

  // Implement InProcessBrowserTest.
  void SetUpOnMainThread() override;
  void TearDown() override;

  // Helpers to execute the tests.
  content::WebContents* GetDefaultWebContents() const;
  void SimulatePenPointerDragEvent(int pointer_id);
  void SimulatePenPointerEventAndStop(
      int pointer_id,
      base::OnceCallback<void(int)> simulate_event_function);
  bool MouseEventCallback(int expected_value,
                          const base::RepeatingClosure& quit_closure,
                          const blink::WebMouseEvent& evt);

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  content::RenderWidgetHost::MouseEventCallback mouse_event_callback_;
};

void PenIdBrowserTest::SetUpOnMainThread() {
  if (base::win::OSInfo::Kernel32Version() < base::win::Version::WIN10_21H2 ||
      (base::win::OSInfo::Kernel32Version() == base::win::Version::WIN10_21H2 &&
       base::win::OSInfo::GetInstance()->version_number().patch < 1503)) {
    GTEST_SKIP() << "Pen Device Api not supported on this machine";
  }
  https_server_.reset(
      new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
  https_server_->ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_->Start());

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_->GetURL(kMainTestPageUrlPath)));
}

void PenIdBrowserTest::TearDown() {
  FakeIPenDeviceStatics::GetInstance()->SimulateAllPenDevicesRemoved();
}

content::WebContents* PenIdBrowserTest::GetDefaultWebContents() const {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void PenIdBrowserTest::SimulatePenPointerEventAndStop(
    int pointer_id,
    base::OnceCallback<void(int)> simulate_event_function) {
  base::RunLoop run_loop;
  GetDefaultWebContents()
      ->GetPrimaryMainFrame()
      ->GetRenderWidgetHost()
      ->RemoveMouseEventCallback(mouse_event_callback_);
  mouse_event_callback_ = base::BindRepeating(
      &PenIdBrowserTest::MouseEventCallback, base::Unretained(this), pointer_id,
      run_loop.QuitClosure());
  GetDefaultWebContents()
      ->GetPrimaryMainFrame()
      ->GetRenderWidgetHost()
      ->AddMouseEventCallback(mouse_event_callback_);
  std::move(simulate_event_function).Run(pointer_id);
  run_loop.Run();
}

void PenIdBrowserTest::SimulatePenPointerDragEvent(int pointer_id) {
  content::WebContents* web_contents = GetDefaultWebContents();

  gfx::Rect container_bounds = web_contents->GetContainerBounds();
  long x = container_bounds.width() / 2;
  long y = container_bounds.height() / 2;
  long offset_x = container_bounds.x();
  long offset_y = container_bounds.y();

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  pen_info.pointerInfo.pointerType = PT_PEN;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;
  // Since, SimulatePenEventForTesting considers the coordinates in relation
  // to the screen, to centralize the click in the middle of the page, it is
  // necessary to translate the pointer by the page container offset.
  pen_info.pointerInfo.ptPixelLocationRaw.x = offset_x + x;
  pen_info.pointerInfo.ptPixelLocationRaw.y = offset_y + y;

  views::test::DesktopWindowTreeHostWinTestApi desktop_window_tree_host(
      static_cast<views::DesktopWindowTreeHostWin*>(
          web_contents->GetNativeView()->GetHost()));

  desktop_window_tree_host.SimulatePenEventForTesting(WM_POINTERDOWN,
                                                      pointer_id, pen_info);

  // Drag the pointer to the first point.
  x = 9 * container_bounds.width() / 20;
  pen_info.pointerInfo.ptPixelLocationRaw.x = offset_x + x;
  desktop_window_tree_host.SimulatePenEventForTesting(WM_POINTERUPDATE,
                                                      pointer_id, pen_info);

  // Drag the pointer to the second point.
  x = 8 * container_bounds.width() / 20;
  pen_info.pointerInfo.ptPixelLocationRaw.x = offset_x + x;
  desktop_window_tree_host.SimulatePenEventForTesting(WM_POINTERUPDATE,
                                                      pointer_id, pen_info);

  // Lift the pointer device.
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;
  desktop_window_tree_host.SimulatePenEventForTesting(WM_POINTERUP, pointer_id,
                                                      pen_info);
}

bool PenIdBrowserTest::MouseEventCallback(
    int expected_value,
    const base::RepeatingClosure& quit_closure,
    const blink::WebMouseEvent& evt) {
  const int32_t id = evt.device_id;
  if (evt.pointer_type == ui::EventPointerType::kPen) {
    EXPECT_EQ(id, expected_value);
  }
  quit_closure.Run();
  return false;
}

// Perform a pen drag for a pen that has a guid. Verify the correct
// device id is propagated in the pointer event.
// The test works by simulating a pen event in the PenDeviceStatics which
// records a fake pen event for a given pointer id. Then, a pointer event is
// simulated by injecting a pen event via
// desktop_window_tree_host.SimulatePenEventForTesting. We bind a callback
// (MouseEventCallback) so that when the browser sends the pen event, it
// checks for the right device id.
IN_PROC_BROWSER_TEST_F(PenIdBrowserTest, PenDeviceTest) {
  views::PenIdHandler::ScopedPenIdStaticsForTesting scoper(
      &FakeIPenDeviceStatics::FakeIPenDeviceStaticsComPtr);
  const auto fake_pen_device = Microsoft::WRL::Make<FakeIPenDevice>();
  FakeIPenDeviceStatics::GetInstance()->SimulatePenEventGenerated(
      kPointerId1, fake_pen_device);

  const auto fake_pen_device_2 = Microsoft::WRL::Make<FakeIPenDevice>();
  FakeIPenDeviceStatics::GetInstance()->SimulatePenEventGenerated(
      kPointerId2, fake_pen_device_2);

  FakeIPenDeviceStatics::GetInstance()->SimulatePenEventGenerated(
      kPointerId3, fake_pen_device);

  // We take advantage of the current implementation of device id to simplify
  // the test. Device Id starts at 0 and iterates, so we expect the first
  // device id to be 0 and the next one to be 1.
  SimulatePenPointerEventAndStop(
      kDeviceId1, base::BindOnce(&PenIdBrowserTest::SimulatePenPointerDragEvent,
                                 base::Unretained(this)));
  SimulatePenPointerEventAndStop(
      kDeviceId2, base::BindOnce(&PenIdBrowserTest::SimulatePenPointerDragEvent,
                                 base::Unretained(this)));
  SimulatePenPointerEventAndStop(
      kDeviceId1, base::BindOnce(&PenIdBrowserTest::SimulatePenPointerDragEvent,
                                 base::Unretained(this)));
}
