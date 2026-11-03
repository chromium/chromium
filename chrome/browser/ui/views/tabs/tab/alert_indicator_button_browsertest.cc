
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab/alert_indicator_button.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/views/view_utils.h"

namespace {

class AlertIndicatorButtonBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<blink::mojom::MediaStreamType> {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Assign the alert_indicator_button_.
    TabStripRegionView* tab_strip_view =
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view();
    views::View* tab = tab_strip_view->GetTabAnchorView(
        browser()->GetTabStripModel()->GetActiveTab()->GetHandle());
    alert_indicator_button_ = views::AsViewClass<AlertIndicatorButton>(
        tab->GetViewByElementId(kTabAlertIndicatorButtonElementId));
  }

  void TearDownOnMainThread() override {
    alert_indicator_button_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  AlertIndicatorButton* alert_indicator_button() {
    return alert_indicator_button_;
  }

 private:
  raw_ptr<AlertIndicatorButton> alert_indicator_button_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(
    ,  // Empty to simplify gtest output
    AlertIndicatorButtonBrowserTest,
    testing::Values(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                    blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));

IN_PROC_BROWSER_TEST_P(AlertIndicatorButtonBrowserTest,
                       CapturingShouldShowAlertIndicatorButton) {
  // alert_indicator_button_ should be invisible in the beginning.
  EXPECT_FALSE(alert_indicator_button()->GetVisible());

  // Prepare for the visibility change notification.
  base::RunLoop run_loop;
  auto callback_subscription =
      alert_indicator_button()->AddVisibleChangedCallback(
          run_loop.QuitClosure());

  // Start capturing.
  blink::mojom::StreamDevices devices;
  blink::MediaStreamDevice device(GetParam(), "fake_media_device",
                                  "fake_media_device");
  if (GetParam() == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    devices.video_device = device;
  } else {
    devices.audio_device = device;
  }

  std::unique_ptr<content::MediaStreamUI> stream_ui =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          ->RegisterMediaStream(web_contents(), devices);
  stream_ui->OnStarted(
      base::RepeatingClosure(), content::MediaStreamUI::SourceCallback(),
      /*label=*/std::string(),
      /*screen_capture_ids=*/{}, content::MediaStreamUI::StateChangeCallback());

  // Wait for the visibility change to trigger.
  run_loop.Run();

  // alert_indicator_button_ should be visible now.
  EXPECT_TRUE(alert_indicator_button()->GetVisible());

  stream_ui.reset();
}

}  // namespace
