// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/content/background_loader/background_loader_contents.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/window_container_type.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

namespace background_loader {

class BackgroundLoaderContentsTest : public testing::Test,
                                     public BackgroundLoaderContents::Delegate {
 public:
  BackgroundLoaderContentsTest();
  ~BackgroundLoaderContentsTest() override;

  void SetUp() override;
  void TearDown() override;

  void CanDownload(base::OnceCallback<void(bool)> callback) override;

  BackgroundLoaderContents* contents() { return contents_.get(); }

  void DownloadCallback(bool download);
  // Sets "this" as delegate to the background loader contents.
  void SetDelegate();

  bool download() { return download_; }
  bool can_download_delegate_called() { return delegate_called_; }

  void MediaAccessCallback(
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      blink::mojom::MediaStreamRequestResult result,
      std::unique_ptr<content::MediaStreamUI> ui);
  blink::mojom::StreamDevices& devices() { return devices_; }
  blink::mojom::MediaStreamRequestResult request_result() {
    return request_result_;
  }
  content::MediaStreamUI* media_stream_ui() { return media_stream_ui_.get(); }

  void WaitForSignal() { waiter_.Wait(); }

 private:
  std::unique_ptr<BackgroundLoaderContents> contents_;
  bool download_;
  bool delegate_called_ = false;
  blink::mojom::StreamDevices devices_;
  blink::mojom::MediaStreamRequestResult request_result_;
  std::unique_ptr<content::MediaStreamUI> media_stream_ui_;
  base::WaitableEvent waiter_;
};

BackgroundLoaderContentsTest::BackgroundLoaderContentsTest()
    : download_(false),
      waiter_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}

BackgroundLoaderContentsTest::~BackgroundLoaderContentsTest() = default;

void BackgroundLoaderContentsTest::SetUp() {
  contents_.reset(new BackgroundLoaderContents());
  download_ = false;
  waiter_.Reset();
}

void BackgroundLoaderContentsTest::TearDown() {
  contents_.reset();
}

void BackgroundLoaderContentsTest::CanDownload(
    base::OnceCallback<void(bool)> callback) {
  delegate_called_ = true;
  std::move(callback).Run(true);
}

void BackgroundLoaderContentsTest::DownloadCallback(bool download) {
  download_ = download;
  waiter_.Signal();
}

void BackgroundLoaderContentsTest::SetDelegate() {
  contents_->SetDelegate(this);
}

void BackgroundLoaderContentsTest::MediaAccessCallback(
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    blink::mojom::MediaStreamRequestResult result,
    std::unique_ptr<content::MediaStreamUI> ui) {
  if (result == blink::mojom::MediaStreamRequestResult::OK) {
    ASSERT_FALSE(stream_devices_set.stream_devices.empty());
    devices_ = *stream_devices_set.stream_devices[0];
  } else {
    ASSERT_TRUE(stream_devices_set.stream_devices.empty());
  }
  request_result_ = result;
  media_stream_ui_.reset(ui.get());
  waiter_.Signal();
}

TEST_F(BackgroundLoaderContentsTest, NotVisible) {
  ASSERT_TRUE(contents()->IsNeverComposited(nullptr));
}

TEST_F(BackgroundLoaderContentsTest, SuppressDialogs) {
  ASSERT_TRUE(contents()->ShouldSuppressDialogs(nullptr));
}

TEST_F(BackgroundLoaderContentsTest, DoesNotFocusAfterCrash) {
  ASSERT_FALSE(contents()->ShouldFocusPageAfterCrash(nullptr));
}

TEST_F(BackgroundLoaderContentsTest, CannotDownloadNoDelegate) {
  contents()->CanDownload(
      GURL(), std::string(),
      base::BindOnce(&BackgroundLoaderContentsTest::DownloadCallback,
                     base::Unretained(this)));
  WaitForSignal();
  ASSERT_FALSE(download());
  ASSERT_FALSE(can_download_delegate_called());
}

TEST_F(BackgroundLoaderContentsTest, CanDownload_DelegateCalledWhenSet) {
  SetDelegate();
  contents()->CanDownload(
      GURL(), std::string(),
      base::BindOnce(&BackgroundLoaderContentsTest::DownloadCallback,
                     base::Unretained(this)));
  WaitForSignal();
  ASSERT_TRUE(download());
  ASSERT_TRUE(can_download_delegate_called());
}

TEST_F(BackgroundLoaderContentsTest, ShouldNotCreateWebContents) {
  ASSERT_TRUE(contents()->IsWebContentsCreationOverridden(
      nullptr /* source_site_instance */,
      content::mojom::WindowContainerType::NORMAL /* window_container_type */,
      GURL() /* opener_url */, "foo" /* frame_name */,
      GURL() /* target_url */));
}

TEST_F(BackgroundLoaderContentsTest, ShouldNotAddNewContents) {
  bool blocked;
  contents()->AddNewContents(
      nullptr /* source */,
      std::unique_ptr<content::WebContents>() /* new_contents */,
      GURL() /* target_url */,
      WindowOpenDisposition::CURRENT_TAB /* disposition */,
      blink::mojom::WindowFeatures() /* window_features */,
      false /* user_gesture */, &blocked /* was_blocked */);
  ASSERT_TRUE(blocked);
}

TEST_F(BackgroundLoaderContentsTest, DoesNotGiveMediaAccessPermission) {
  content::MediaStreamRequest request(
      0 /* render_process_id */, 0 /* render_frame_id */,
      0 /* page_request_id */, url::Origin::Create(GURL()) /* url_origin */,
      false /* user_gesture */,
      blink::MediaStreamRequestType::MEDIA_DEVICE_ACCESS /* request_type */,
      {} /* requested_audio_device_ids */, {} /* requested_video_device_ids */,
      blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE /* audio_type */,
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE /* video_type */,
      false /* disable_local_echo */,
      false /* request_pan_tilt_zoom_permission */,
      false /* captured_surface_control_active */);
  contents()->RequestMediaAccessPermission(
      nullptr /* contents */, request /* request */,
      base::BindRepeating(&BackgroundLoaderContentsTest::MediaAccessCallback,
                          base::Unretained(this)));
  WaitForSignal();
  // No devices allowed.
  ASSERT_TRUE(!devices().audio_device.has_value() &&
              !devices().video_device.has_value());
  // Permission has been dismissed rather than denied.
  ASSERT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED,
            request_result());
  ASSERT_EQ(nullptr, media_stream_ui());
}

TEST_F(BackgroundLoaderContentsTest, CheckMediaAccessPermissionFalse) {
  ASSERT_FALSE(contents()->CheckMediaAccessPermission(
      nullptr /* contents */, url::Origin() /* security_origin */,
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE /* type */));
}

}  // namespace background_loader
