// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_screenshare_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/desktop_capture_test_utils.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

class FakeDesktopCapturer : public webrtc::DesktopCapturer {
 public:
  explicit FakeDesktopCapturer(
      webrtc::DesktopSize size = webrtc::DesktopSize(1, 1),
      Result result = Result::SUCCESS)
      : size_(size), result_(result) {}
  ~FakeDesktopCapturer() override = default;

  void Start(Callback* callback) override { callback_ = callback; }

  void CaptureFrame() override {
    if (result_ != Result::SUCCESS) {
      callback_->OnCaptureResult(result_, nullptr);
      return;
    }
    auto frame = std::make_unique<webrtc::BasicDesktopFrame>(size_);
    frame->SetFrameDataToBlack();
    callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
  }

  bool GetSourceList(SourceList* sources) override { return true; }
  bool SelectSource(SourceId id) override {
    last_selected_source_id_ = id;
    return true;
  }

  std::optional<SourceId> last_selected_source_id() const {
    return last_selected_source_id_;
  }

 private:
  webrtc::DesktopSize size_;
  Result result_;
  std::optional<SourceId> last_selected_source_id_;
  raw_ptr<Callback> callback_ = nullptr;
};

class MockScreenshareHost
    : public ContextualSearchboxScreenshareController::Host {
 public:
  MOCK_METHOD(void,
              UploadScreenshot,
              (std::string,
               std::string,
               mojo_base::BigBuffer,
               std::optional<lens::ImageEncodingOptions>,
               AddFileContextCallback),
              (override));
  MOCK_METHOD(void,
              AddFileContextToPage,
              (const base::UnguessableToken&,
               searchbox::mojom::SelectedFileInfoPtr),
              (override));
  MOCK_METHOD(void, OnScreenshotMenuClosed, (), (override));
};

class MockScreenshareDelegate
    : public ContextualSearchboxScreenshareController::Delegate {
 public:
  MOCK_METHOD(void,
              ShowScreenshotMenu,
              (const gfx::Rect&,
               base::WeakPtr<ContextualSearchboxScreenshareController>),
              (override));
  MOCK_METHOD(void, OnScreensharePickerOpened, (), (override));
  MOCK_METHOD(void, OnScreensharePickerClosed, (), (override));
  MOCK_METHOD(void,
              ShowRegionSelectOverlay,
              (const SkBitmap&,
               const RegionCaptureSource&,
               RegionSelectedCallback),
              (override));
};

class ControlledDesktopMediaPicker : public DesktopMediaPicker {
 public:
  ControlledDesktopMediaPicker(
      base::RepeatingClosure* on_destroying_ptr,
      DesktopMediaPicker::DoneCallback* done_callback_ptr)
      : on_destroying_ptr_(on_destroying_ptr),
        done_callback_ptr_(done_callback_ptr) {}
  ~ControlledDesktopMediaPicker() override = default;

  void Show(const Params& params,
            std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
            DoneCallback done_callback) override {
    if (on_destroying_ptr_) {
      *on_destroying_ptr_ = params.on_picker_destroying;
    }
    if (done_callback_ptr_) {
      *done_callback_ptr_ = std::move(done_callback);
    }
  }

 private:
  raw_ptr<base::RepeatingClosure> on_destroying_ptr_;
  raw_ptr<DesktopMediaPicker::DoneCallback> done_callback_ptr_;
};

class ControlledDesktopMediaPickerFactory : public DesktopMediaPickerFactory {
 public:
  ControlledDesktopMediaPickerFactory(
      base::RepeatingClosure* on_destroying_ptr,
      DesktopMediaPicker::DoneCallback* done_callback_ptr)
      : on_destroying_ptr_(on_destroying_ptr),
        done_callback_ptr_(done_callback_ptr) {}

  std::unique_ptr<DesktopMediaPicker> CreatePicker(
      const content::MediaStreamRequest* request) override {
    return std::make_unique<ControlledDesktopMediaPicker>(on_destroying_ptr_,
                                                          done_callback_ptr_);
  }

  std::vector<std::unique_ptr<DesktopMediaList>> CreateMediaList(
      const std::vector<DesktopMediaList::Type>& types,
      content::WebContents* web_contents,
      DesktopMediaList::WebContentsFilter filter) override {
    std::vector<std::unique_ptr<DesktopMediaList>> media_lists;
    for (auto source_type : types) {
      media_lists.emplace_back(new FakeDesktopMediaList(source_type));
    }
    return media_lists;
  }

 private:
  raw_ptr<base::RepeatingClosure> on_destroying_ptr_;
  raw_ptr<DesktopMediaPicker::DoneCallback> done_callback_ptr_;
};

}  // namespace

class ContextualSearchboxScreenshareControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if BUILDFLAG(IS_MAC)
    scoped_feature_list_.InitAndDisableFeature(
        kOmniboxEverywhereNativeScreenPicker);
    default_picker_test_flags_.expect_screens = true;
    default_picker_test_flags_.expect_windows = false;
    default_picker_test_flags_.picker_result =
        content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 42);
    default_picker_factory_.SetTestFlags(
        base::span_from_ref(default_picker_test_flags_));
    CreateController(&default_picker_factory_);
#else
    CreateController();
#endif
  }

  void TearDown() override {
    controller_.reset();
#if BUILDFLAG(IS_MAC)
    scoped_feature_list_.Reset();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateController(DesktopMediaPickerFactory* picker_factory = nullptr) {
    controller_ = std::make_unique<ContextualSearchboxScreenshareController>(
        web_contents(), &mock_host_, &mock_delegate_, picker_factory);
  }

  MockScreenshareHost& host() { return mock_host_; }
  MockScreenshareDelegate& delegate() { return mock_delegate_; }
  ContextualSearchboxScreenshareController& controller() {
    return *controller_;
  }
#if BUILDFLAG(IS_MAC)
  FakeDesktopMediaPickerFactory& default_picker_factory() {
    return default_picker_factory_;
  }
  FakeDesktopMediaPickerFactory::TestFlags& default_picker_test_flags() {
    return default_picker_test_flags_;
  }
#endif

  void SetupScreenshotUploadConfig() {
    scoped_config_.Get().config.mutable_composebox()->set_max_num_files(5);
    scoped_config_.Get()
        .config.mutable_composebox()
        ->mutable_attachment_upload()
        ->set_max_size_bytes(1024 * 1024);
    scoped_config_.Get()
        .config.mutable_composebox()
        ->mutable_image_upload()
        ->set_mime_types_allowed("image/png");
    scoped_config_.Get()
        .config.mutable_composebox()
        ->mutable_image_upload()
        ->set_downscale_max_image_size(1024 * 1024);
    scoped_config_.Get()
        .config.mutable_composebox()
        ->mutable_image_upload()
        ->set_downscale_max_image_height(1024);
    scoped_config_.Get()
        .config.mutable_composebox()
        ->mutable_image_upload()
        ->set_downscale_max_image_width(2048);
  }

 private:
  testing::NiceMock<MockScreenshareHost> mock_host_;
  testing::NiceMock<MockScreenshareDelegate> mock_delegate_;
  std::unique_ptr<ContextualSearchboxScreenshareController> controller_;
  ntp_composebox::ScopedFeatureConfigForTesting scoped_config_;
#if BUILDFLAG(IS_MAC)
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeDesktopMediaPickerFactory default_picker_factory_;
  FakeDesktopMediaPickerFactory::TestFlags default_picker_test_flags_;
#endif
};

TEST_F(ContextualSearchboxScreenshareControllerTest,
       ShowScreenshotMenu_ForwardsToDelegate) {
  EXPECT_CALL(delegate(),
              ShowScreenshotMenu(gfx::Rect(1, 2, 3, 4), testing::_));

  controller().ShowScreenshotMenu(gfx::Rect(1, 2, 3, 4));
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       ShowScreenshotMenu_NoDelegate_NotifiesClosed) {
  controller().set_delegate(nullptr);
  EXPECT_CALL(host(), OnScreenshotMenuClosed());

  controller().ShowScreenshotMenu(gfx::Rect(1, 2, 3, 4));
}

TEST_F(ContextualSearchboxScreenshareControllerTest, StartScreenshare_Success) {
  SetupScreenshotUploadConfig();

  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());

  FakeDesktopMediaPickerFactory picker_factory;
  CreateController(&picker_factory);
  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>(webrtc::DesktopSize(4000, 2000)));

  FakeDesktopMediaPickerFactory::TestFlags test_flags;
  test_flags.expect_screens = true;
  test_flags.expect_windows = true;
  test_flags.picker_result =
      content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, 42);
  picker_factory.SetTestFlags(base::span_from_ref(test_flags));

  base::UnguessableToken expected_token = base::UnguessableToken::Create();
  EXPECT_CALL(host(), UploadScreenshot)
      .WillOnce([&](std::string file_name, std::string mime_type,
                    mojo_base::BigBuffer file_bytes,
                    std::optional<lens::ImageEncodingOptions> image_options,
                    MockScreenshareHost::AddFileContextCallback callback) {
        EXPECT_EQ(file_name, "Screenshot.png");
        EXPECT_EQ(mime_type, "image/png");
        EXPECT_TRUE(image_options.has_value());

        // Verify oversized image downscaling constraint (<= 2048px, aspect
        // ratio preserved 2048x1024).
        SkBitmap main_bitmap = gfx::PNGCodec::Decode(file_bytes);
        EXPECT_FALSE(main_bitmap.isNull());
        EXPECT_EQ(main_bitmap.width(), 2048);
        EXPECT_EQ(main_bitmap.height(), 1024);

        std::move(callback).Run(expected_token);
      });

  EXPECT_CALL(host(), AddFileContextToPage(expected_token, testing::_))
      .WillOnce([&](const base::UnguessableToken& token,
                    searchbox::mojom::SelectedFileInfoPtr file_info) {
        EXPECT_EQ(file_info->file_name, "Screenshot.png");
        EXPECT_EQ(file_info->mime_type, "image/png");
        EXPECT_TRUE(file_info->image_data_url.has_value());
        if (file_info->image_data_url.has_value()) {
          EXPECT_TRUE(base::StartsWith(*file_info->image_data_url,
                                       "data:image/png;base64,"));

          // Verify thumbnail dimensions constraint (<= 120px, aspect ratio
          // preserved 120x60).
          std::string base64_payload = file_info->image_data_url->substr(
              std::string_view("data:image/png;base64,").length());
          std::optional<std::vector<uint8_t>> thumb_bytes =
              base::Base64Decode(base64_payload);
          EXPECT_TRUE(thumb_bytes.has_value());
          if (thumb_bytes) {
            SkBitmap thumb_bitmap = gfx::PNGCodec::Decode(*thumb_bytes);
            EXPECT_FALSE(thumb_bitmap.isNull());
            EXPECT_EQ(thumb_bitmap.width(), 120);
            EXPECT_EQ(thumb_bitmap.height(), 60);
          }
        }
      });

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().StartScreenshare(/*prefer_entire_screen=*/false,
                                future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(*future.Get(), expected_token);
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       StartScreenshare_SmallImageNotResized) {
  SetupScreenshotUploadConfig();

  FakeDesktopMediaPickerFactory picker_factory;
  CreateController(&picker_factory);
  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>(webrtc::DesktopSize(50, 30)));

  FakeDesktopMediaPickerFactory::TestFlags test_flags;
  test_flags.expect_screens = true;
  test_flags.expect_windows = true;
  test_flags.picker_result =
      content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, 42);
  picker_factory.SetTestFlags(base::span_from_ref(test_flags));

  base::UnguessableToken expected_token = base::UnguessableToken::Create();
  EXPECT_CALL(host(), UploadScreenshot)
      .WillOnce([&](std::string file_name, std::string mime_type,
                    mojo_base::BigBuffer file_bytes,
                    std::optional<lens::ImageEncodingOptions> image_options,
                    MockScreenshareHost::AddFileContextCallback callback) {
        SkBitmap main_bitmap = gfx::PNGCodec::Decode(file_bytes);
        EXPECT_FALSE(main_bitmap.isNull());
        EXPECT_EQ(main_bitmap.width(), 50);
        EXPECT_EQ(main_bitmap.height(), 30);
        std::move(callback).Run(expected_token);
      });

  EXPECT_CALL(host(), AddFileContextToPage(expected_token, testing::_))
      .WillOnce([&](const base::UnguessableToken& token,
                    searchbox::mojom::SelectedFileInfoPtr file_info) {
        EXPECT_TRUE(file_info->image_data_url.has_value());
        if (file_info->image_data_url.has_value()) {
          std::string base64_payload = file_info->image_data_url->substr(
              std::string_view("data:image/png;base64,").length());
          std::optional<std::vector<uint8_t>> thumb_bytes =
              base::Base64Decode(base64_payload);
          EXPECT_TRUE(thumb_bytes.has_value());
          if (thumb_bytes) {
            SkBitmap thumb_bitmap = gfx::PNGCodec::Decode(*thumb_bytes);
            EXPECT_FALSE(thumb_bitmap.isNull());
            EXPECT_EQ(thumb_bitmap.width(), 50);
            EXPECT_EQ(thumb_bitmap.height(), 30);
          }
        }
      });

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().StartScreenshare(/*prefer_entire_screen=*/false,
                                future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(*future.Get(), expected_token);
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       StartScreenshare_Cancelled) {
  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());

  FakeDesktopMediaPickerFactory picker_factory;
  CreateController(&picker_factory);

  FakeDesktopMediaPickerFactory::TestFlags test_flags;
  test_flags.expect_screens = true;
  test_flags.expect_windows = true;
  test_flags.picker_result = content::DesktopMediaID();
  picker_factory.SetTestFlags(base::span_from_ref(test_flags));

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().StartScreenshare(/*prefer_entire_screen=*/true,
                                future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       StartScreenshare_ResultBeforeDialogDestroyed) {
  SetupScreenshotUploadConfig();

  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());

  base::RepeatingClosure on_destroying;
  DesktopMediaPicker::DoneCallback done_callback;
  ControlledDesktopMediaPickerFactory picker_factory(&on_destroying,
                                                     &done_callback);
  CreateController(&picker_factory);

  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>(webrtc::DesktopSize(100, 100)));

  base::UnguessableToken expected_token = base::UnguessableToken::Create();
  EXPECT_CALL(host(), UploadScreenshot)
      .WillOnce([&](std::string file_name, std::string mime_type,
                    mojo_base::BigBuffer file_bytes,
                    std::optional<lens::ImageEncodingOptions> image_options,
                    MockScreenshareHost::AddFileContextCallback callback) {
        std::move(callback).Run(expected_token);
      });
  EXPECT_CALL(host(), AddFileContextToPage(expected_token, testing::_));

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().StartScreenshare(/*prefer_entire_screen=*/false,
                                future.GetCallback());

  ASSERT_TRUE(done_callback);
  ASSERT_TRUE(on_destroying);

  // 1. Selection result arrives while dialog is still closing/destroying.
  std::move(done_callback)
      .Run(content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, 42));

  // Screenshot capture should not have resolved yet.
  EXPECT_FALSE(future.IsReady());

  // 2. Dialog widget completes destruction.
  on_destroying.Run();

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(*future.Get(), expected_token);
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       StartScreenshare_DialogDestroyedBeforeResult) {
  SetupScreenshotUploadConfig();

  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());

  base::RepeatingClosure on_destroying;
  DesktopMediaPicker::DoneCallback done_callback;
  ControlledDesktopMediaPickerFactory picker_factory(&on_destroying,
                                                     &done_callback);
  CreateController(&picker_factory);

  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>(webrtc::DesktopSize(100, 100)));

  base::UnguessableToken expected_token = base::UnguessableToken::Create();
  EXPECT_CALL(host(), UploadScreenshot)
      .WillOnce([&](std::string file_name, std::string mime_type,
                    mojo_base::BigBuffer file_bytes,
                    std::optional<lens::ImageEncodingOptions> image_options,
                    MockScreenshareHost::AddFileContextCallback callback) {
        std::move(callback).Run(expected_token);
      });
  EXPECT_CALL(host(), AddFileContextToPage(expected_token, testing::_));

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().StartScreenshare(/*prefer_entire_screen=*/false,
                                future.GetCallback());

  ASSERT_TRUE(done_callback);
  ASSERT_TRUE(on_destroying);

  // 1. Dialog widget destroys first.
  on_destroying.Run();

  EXPECT_FALSE(future.IsReady());

  // 2. Selection result arrives afterwards.
  std::move(done_callback)
      .Run(content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, 42));

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(*future.Get(), expected_token);
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       StartScreenshare_DestructionWhilePendingCallback) {
  base::RepeatingClosure on_destroying;
  DesktopMediaPicker::DoneCallback done_callback;
  ControlledDesktopMediaPickerFactory picker_factory(&on_destroying,
                                                     &done_callback);
  CreateController(&picker_factory);

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().StartScreenshare(/*prefer_entire_screen=*/false,
                                future.GetCallback());

  ASSERT_TRUE(done_callback);

  // Selection result arrives, so pending callback is waiting for destruction.
  std::move(done_callback)
      .Run(content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, 42));

  EXPECT_FALSE(future.IsReady());

  // Resetting controller (e.g. tab closed or navigation) should safely resolve
  // the pending Mojo callback with nullopt.
  CreateController(nullptr);

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       CaptureRegionScreenshot_Success) {
  SetupScreenshotUploadConfig();

  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());
  EXPECT_CALL(delegate(), ShowRegionSelectOverlay)
      .WillOnce([](const SkBitmap& screenshot,
                   const ContextualSearchboxScreenshareController::
                       RegionCaptureSource& source,
                   ContextualSearchboxScreenshareController::Delegate::
                       RegionSelectedCallback callback) {
#if BUILDFLAG(IS_MAC)
        EXPECT_EQ(source.type, ContextualSearchboxScreenshareController::
                                   RegionCaptureSource::Type::kSpecificDisplay);
        EXPECT_EQ(source.display_id, 42);
#else
        EXPECT_EQ(source.type, ContextualSearchboxScreenshareController::
                                   RegionCaptureSource::Type::kAllDisplays);
#endif
        std::move(callback).Run(screenshot);
      });

  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>(webrtc::DesktopSize(1000, 800)));

  base::UnguessableToken expected_token = base::UnguessableToken::Create();
  EXPECT_CALL(host(), UploadScreenshot)
      .WillOnce([&](std::string file_name, std::string mime_type,
                    mojo_base::BigBuffer file_bytes,
                    std::optional<lens::ImageEncodingOptions> image_options,
                    MockScreenshareHost::AddFileContextCallback callback) {
        EXPECT_EQ(file_name, "Screenshot.png");
        EXPECT_EQ(mime_type, "image/png");
        EXPECT_TRUE(image_options.has_value());
        std::move(callback).Run(expected_token);
      });

  EXPECT_CALL(host(), AddFileContextToPage(expected_token, testing::_))
      .WillOnce([&](const base::UnguessableToken& token,
                    searchbox::mojom::SelectedFileInfoPtr file_info) {
        EXPECT_EQ(file_info->file_name, "Screenshot.png");
        EXPECT_EQ(file_info->mime_type, "image/png");
        EXPECT_TRUE(file_info->image_data_url.has_value());
      });

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().CaptureRegionScreenshot(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(*future.Get(), expected_token);
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       CaptureRegionScreenshot_UserCancelledOverlay) {
  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());
  EXPECT_CALL(delegate(), ShowRegionSelectOverlay)
      .WillOnce([](const SkBitmap& screenshot,
                   const ContextualSearchboxScreenshareController::
                       RegionCaptureSource& source,
                   ContextualSearchboxScreenshareController::Delegate::
                       RegionSelectedCallback callback) {
        // Simulates user hitting Escape or closing overlay (empty bitmap).
        std::move(callback).Run(SkBitmap());
      });

  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>(webrtc::DesktopSize(1000, 800)));

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().CaptureRegionScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       CaptureRegionScreenshot_AlreadyCapturing) {
  SetupScreenshotUploadConfig();

  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());
  EXPECT_CALL(delegate(), ShowRegionSelectOverlay)
      .WillOnce([](const SkBitmap& screenshot,
                   const ContextualSearchboxScreenshareController::
                       RegionCaptureSource& source,
                   ContextualSearchboxScreenshareController::Delegate::
                       RegionSelectedCallback callback) {
        std::move(callback).Run(screenshot);
      });

  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>(webrtc::DesktopSize(100, 100)));

  base::UnguessableToken expected_token = base::UnguessableToken::Create();
  EXPECT_CALL(host(), UploadScreenshot)
      .WillOnce([&](std::string file_name, std::string mime_type,
                    mojo_base::BigBuffer file_bytes,
                    std::optional<lens::ImageEncodingOptions> image_options,
                    MockScreenshareHost::AddFileContextCallback callback) {
        std::move(callback).Run(expected_token);
      });
  EXPECT_CALL(host(), AddFileContextToPage(expected_token, testing::_));

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future1;
  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future2;

  controller().CaptureRegionScreenshot(future1.GetCallback());
  controller().CaptureRegionScreenshot(future2.GetCallback());

  EXPECT_FALSE(future2.Get().has_value());
  EXPECT_TRUE(future1.Get().has_value());
  EXPECT_EQ(*future1.Get(), expected_token);
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       CaptureRegionScreenshot_EmptyBitmap) {
  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());

  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>(
          webrtc::DesktopSize(1, 1),
          webrtc::DesktopCapturer::Result::ERROR_PERMANENT));

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().CaptureRegionScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       CaptureRegionScreenshot_NullDelegateFailsSafely) {
  controller().set_delegate(nullptr);

  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>(webrtc::DesktopSize(1000, 800)));

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().CaptureRegionScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

#if BUILDFLAG(IS_MAC)
using content::desktop_capture::ScopedNativePickerForTesting;

TEST_F(ContextualSearchboxScreenshareControllerTest,
       StartScreenshare_NativePicker_Success) {
  if (base::mac::MacOSVersion() < 26'04'00) {
    GTEST_SKIP() << "Native picker only supported on macOS 26.4+";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOmniboxEverywhereNativeScreenPicker);
  SetupScreenshotUploadConfig();

  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());

  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>());
  ScopedNativePickerForTesting scoped_picker(
      ScopedNativePickerForTesting::Action::kSelectSource);

  base::UnguessableToken expected_token = base::UnguessableToken::Create();
  EXPECT_CALL(host(), UploadScreenshot)
      .WillOnce([&](std::string file_name, std::string mime_type,
                    mojo_base::BigBuffer file_bytes,
                    std::optional<lens::ImageEncodingOptions> image_options,
                    MockScreenshareHost::AddFileContextCallback callback) {
        EXPECT_EQ(file_name, "Screenshot.png");
        EXPECT_EQ(mime_type, "image/png");
        EXPECT_TRUE(image_options.has_value());
        std::move(callback).Run(expected_token);
      });

  EXPECT_CALL(host(), AddFileContextToPage(expected_token, testing::_))
      .WillOnce([&](const base::UnguessableToken& token,
                    searchbox::mojom::SelectedFileInfoPtr file_info) {
        EXPECT_EQ(file_info->file_name, "Screenshot.png");
        EXPECT_EQ(file_info->mime_type, "image/png");
        EXPECT_TRUE(file_info->image_data_url.has_value());
        if (file_info->image_data_url.has_value()) {
          EXPECT_TRUE(base::StartsWith(*file_info->image_data_url,
                                       "data:image/png;base64,"));
        }
      });

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().StartScreenshare(/*prefer_entire_screen=*/false,
                                future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(*future.Get(), expected_token);
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       StartScreenshare_NativePicker_Cancelled) {
  if (base::mac::MacOSVersion() < 26'04'00) {
    GTEST_SKIP() << "Native picker only supported on macOS 26.4+";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOmniboxEverywhereNativeScreenPicker);

  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());

  ScopedNativePickerForTesting scoped_picker(
      ScopedNativePickerForTesting::Action::kCancel);

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().StartScreenshare(/*prefer_entire_screen=*/false,
                                future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       StartScreenshare_NativePicker_Error_FallsBackToDefaultPicker) {
  if (base::mac::MacOSVersion() < 26'04'00) {
    GTEST_SKIP() << "Native picker only supported on macOS 26.4+";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOmniboxEverywhereNativeScreenPicker);
  SetupScreenshotUploadConfig();

  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());

  FakeDesktopMediaPickerFactory picker_factory;
  CreateController(&picker_factory);

  FakeDesktopMediaPickerFactory::TestFlags test_flags;
  test_flags.expect_screens = true;
  test_flags.expect_windows = true;
  test_flags.picker_result =
      content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, 42);
  picker_factory.SetTestFlags(base::span_from_ref(test_flags));

  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>());
  ScopedNativePickerForTesting scoped_picker(
      ScopedNativePickerForTesting::Action::kError);

  base::UnguessableToken expected_token = base::UnguessableToken::Create();
  EXPECT_CALL(host(), UploadScreenshot)
      .WillOnce([&](std::string file_name, std::string mime_type,
                    mojo_base::BigBuffer file_bytes,
                    std::optional<lens::ImageEncodingOptions> image_options,
                    MockScreenshareHost::AddFileContextCallback callback) {
        EXPECT_EQ(file_name, "Screenshot.png");
        EXPECT_EQ(mime_type, "image/png");
        EXPECT_TRUE(image_options.has_value());
        std::move(callback).Run(expected_token);
      });

  EXPECT_CALL(host(), AddFileContextToPage(expected_token, testing::_))
      .WillOnce([&](const base::UnguessableToken& token,
                    searchbox::mojom::SelectedFileInfoPtr file_info) {
        EXPECT_EQ(file_info->file_name, "Screenshot.png");
        EXPECT_EQ(file_info->mime_type, "image/png");
        EXPECT_TRUE(file_info->image_data_url.has_value());
        if (file_info->image_data_url.has_value()) {
          EXPECT_TRUE(base::StartsWith(*file_info->image_data_url,
                                       "data:image/png;base64,"));
        }
      });

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().StartScreenshare(/*prefer_entire_screen=*/false,
                                future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(*future.Get(), expected_token);
}
TEST_F(ContextualSearchboxScreenshareControllerTest,
       CaptureRegionScreenshot_NativePicker_Success) {
  if (base::mac::MacOSVersion() < 26'04'00) {
    GTEST_SKIP()
        << "Native picker for region capture only supported on macOS 26.4+";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOmniboxEverywhereNativeScreenPicker);
  SetupScreenshotUploadConfig();

  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());
  EXPECT_CALL(delegate(), ShowRegionSelectOverlay)
      .WillOnce([](const SkBitmap& screenshot,
                   const ContextualSearchboxScreenshareController::
                       RegionCaptureSource& source,
                   ContextualSearchboxScreenshareController::Delegate::
                       RegionSelectedCallback callback) {
        EXPECT_EQ(source.type, ContextualSearchboxScreenshareController::
                                   RegionCaptureSource::Type::kSpecificDisplay);
        EXPECT_EQ(source.display_id, 42);
        std::move(callback).Run(screenshot);
      });

  content::desktop_capture::ScopedDesktopCapturerForTesting scoped_capturer(
      std::make_unique<FakeDesktopCapturer>());
  ScopedNativePickerForTesting scoped_picker(
      ScopedNativePickerForTesting::Action::kSelectSource, /*session_id=*/1,
      webrtc::DesktopCapturer::Source{1, "Mock Screen", 42});

  base::UnguessableToken expected_token = base::UnguessableToken::Create();
  EXPECT_CALL(host(), UploadScreenshot)
      .WillOnce([&](std::string file_name, std::string mime_type,
                    mojo_base::BigBuffer file_bytes,
                    std::optional<lens::ImageEncodingOptions> image_options,
                    MockScreenshareHost::AddFileContextCallback callback) {
        EXPECT_EQ(file_name, "Screenshot.png");
        EXPECT_EQ(mime_type, "image/png");
        EXPECT_TRUE(image_options.has_value());
        std::move(callback).Run(expected_token);
      });

  EXPECT_CALL(host(), AddFileContextToPage(expected_token, testing::_))
      .WillOnce([&](const base::UnguessableToken& token,
                    searchbox::mojom::SelectedFileInfoPtr file_info) {
        EXPECT_EQ(file_info->file_name, "Screenshot.png");
        EXPECT_EQ(file_info->mime_type, "image/png");
        EXPECT_TRUE(file_info->image_data_url.has_value());
      });

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().CaptureRegionScreenshot(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(*future.Get(), expected_token);
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       CaptureRegionScreenshot_NativePicker_Cancelled) {
  if (base::mac::MacOSVersion() < 26'04'00) {
    GTEST_SKIP()
        << "Native picker for region capture only supported on macOS 26.4+";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOmniboxEverywhereNativeScreenPicker);

  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());

  ScopedNativePickerForTesting scoped_picker(
      ScopedNativePickerForTesting::Action::kCancel);

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().CaptureRegionScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ContextualSearchboxScreenshareControllerTest,
       CaptureRegionScreenshot_DefaultPicker_Cancelled) {
  EXPECT_CALL(delegate(), OnScreensharePickerOpened());
  EXPECT_CALL(delegate(), OnScreensharePickerClosed());

  default_picker_test_flags().expect_screens = true;
  default_picker_test_flags().expect_windows = false;
  default_picker_test_flags().picker_result = content::DesktopMediaID();
  default_picker_factory().SetTestFlags(
      base::span_from_ref(default_picker_test_flags()));

  base::test::TestFuture<const std::optional<base::UnguessableToken>&> future;
  controller().CaptureRegionScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}
#endif
