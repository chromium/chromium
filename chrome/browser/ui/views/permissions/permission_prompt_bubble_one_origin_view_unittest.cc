// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_one_origin_view.h"

#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "components/media_effects/test/scoped_media_device_info.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/blink/public/common/features.h"
#endif

using base::Bucket;
using testing::ElementsAre;
using PermissionPromptBubbleOneOriginViewTest = ChromeViewsTestBase;

namespace {

class TestDelegate : public permissions::PermissionPrompt::Delegate {
 public:
  TestDelegate(
      const GURL& origin,
      const std::vector<permissions::RequestType> request_types,
      const std::vector<std::string> requested_audio_capture_device_ids,
      const std::vector<std::string> requested_video_capture_device_ids) {
    requests_ = base::ToVector(
        request_types,
        [&](auto& request_type)
            -> std::unique_ptr<permissions::PermissionRequest> {
          return std::make_unique<permissions::MockPermissionRequest>(
              origin, request_type, requested_audio_capture_device_ids,
              requested_video_capture_device_ids);
        });
    InitializeRawRequests();
  }

  explicit TestDelegate(
      const GURL& origin,
      const std::vector<permissions::RequestType> request_types) {
    requests_ = base::ToVector(
        request_types,
        [&](auto& request_type)
            -> std::unique_ptr<permissions::PermissionRequest> {
          return std::make_unique<permissions::MockPermissionRequest>(
              origin, request_type);
        });
    InitializeRawRequests();
  }

  void InitializeRawRequests() {
    raw_requests_ = base::ToVector(
        requests_,
        [](const auto& request)
            -> raw_ptr<permissions::PermissionRequest, VectorExperimental> {
          return request.get();
        });
  }

  const std::vector<
      raw_ptr<permissions::PermissionRequest, VectorExperimental>>&
  Requests() override {
    return raw_requests_;
  }

  GURL GetRequestingOrigin() const override {
    return raw_requests_.front()->requesting_origin();
  }

  GURL GetEmbeddingOrigin() const override {
    return GURL("https://embedder.example.com");
  }

  void Accept() override {}
  void AcceptThisTime() override {}
  void Deny() override {}
  void Dismiss() override {}
  void Ignore() override {}
  void FinalizeCurrentRequests() override {}
  void OpenHelpCenterLink(const ui::Event& event) override {}
  void PreIgnoreQuietPrompt() override {}
  void SetManageClicked() override {}
  void SetLearnMoreClicked() override {}
  void SetHatsShownCallback(base::OnceCallback<void()> callback) override {}

  bool WasCurrentRequestAlreadyDisplayed() override { return false; }
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override {
    return false;
  }
  bool ShouldCurrentRequestUseQuietUI() const override { return false; }
  std::optional<permissions::PermissionUiSelector::QuietUiReason>
  ReasonForUsingQuietUi() const override {
    return std::nullopt;
  }
  void SetDismissOnTabClose() override {}
  void SetPromptShown() override {}
  void SetDecisionTime() override {}
  bool RecreateView() override { return false; }

  base::WeakPtr<permissions::PermissionPrompt::Delegate> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  content::WebContents* GetAssociatedWebContents() override { return nullptr; }

 private:
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests_;
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      raw_requests_;
  base::WeakPtrFactory<TestDelegate> weak_factory_{this};
};

std::unique_ptr<PermissionPromptBubbleOneOriginView> CreateBubble(
    TestDelegate* delegate) {
  return std::make_unique<PermissionPromptBubbleOneOriginView>(
      nullptr, delegate->GetWeakPtr(), base::TimeTicks::Now(),
      PermissionPromptStyle::kBubbleOnly);
}

}  // namespace

TEST_F(PermissionPromptBubbleOneOriginViewTest,
       AccessibleTitleMentionsOriginAndPermissions) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kMicStream,
                         permissions::RequestType::kCameraStream});
  auto bubble = CreateBubble(&delegate);

  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "microphone",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "camera",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
  // The scheme is not included.
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "test.origin",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
}

TEST_F(PermissionPromptBubbleOneOriginViewTest,
       AccessibleTitleDoesNotMentionTooManyPermissions) {
  TestDelegate delegate(GURL(), {permissions::RequestType::kGeolocation,
                                 permissions::RequestType::kNotifications,
                                 permissions::RequestType::kMicStream,
                                 permissions::RequestType::kCameraStream});
  auto bubble = CreateBubble(&delegate);

  const auto title = base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "location", title);
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "notifications", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "microphone", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "camera", title);
}

TEST_F(PermissionPromptBubbleOneOriginViewTest,
       AccessibleTitleFileSchemeMentionsThisFile) {
  TestDelegate delegate(GURL("file:///tmp/index.html"),
                        {permissions::RequestType::kMicStream});
  auto bubble = CreateBubble(&delegate);

  EXPECT_PRED_FORMAT2(::testing::IsSubstring,
                      base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                          IDS_PERMISSIONS_BUBBLE_PROMPT_THIS_FILE)),
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
}

TEST_F(PermissionPromptBubbleOneOriginViewTest,
       AccessibleTitleIncludesOnlyVisiblePermissions) {
  TestDelegate delegate(GURL(), {permissions::RequestType::kMicStream,
                                 permissions::RequestType::kCameraStream,
                                 permissions::RequestType::kCameraPanTiltZoom});
  auto bubble = CreateBubble(&delegate);

  const auto title = base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "microphone", title);
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "move your camera", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "use your camera", title);
}

#if !BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr char kCameraId[] = "camera_id";
constexpr char kCameraName[] = "camera_name";
constexpr char kCameraId2[] = "camera_id_2";
constexpr char kCameraName2[] = "camera_name_2";

constexpr char kMicId[] = "mic_id";
constexpr char kMicName[] = "mic_name";
constexpr char kGroupId[] = "group_id";
constexpr char kMicId2[] = "mic_id_2";
constexpr char kMicName2[] = "mic_name_2";
constexpr char kGroupId2[] = "group_id_2";

constexpr char kOriginTrialAllowedHistogramName[] =
    "MediaPreviews.UI.Permissions.OriginTrialAllowed";

}  // namespace

// Takes care of all setup needed to initialize page info permission prompt one
// origin view (e.g. permission prompt delegate, ...), as well as media previews
// (e.g. audio service, video service, ...).
class PermissionPromptBubbleOneOriginViewTestMediaPreview
    : public TestWithBrowserView {
 protected:
  PermissionPromptBubbleOneOriginViewTestMediaPreview() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kCameraMicPreview);
  }

  void SetUp() override {
    TestWithBrowserView::SetUp();
    base::test::TestFuture<void> mic_infos, camera_infos;
    audio_service_.SetOnRepliedWithInputDeviceDescriptionsCallback(
        mic_infos.GetCallback());
    video_service_.SetOnRepliedWithSourceInfosCallback(
        camera_infos.GetCallback());

    media_device_info_.emplace();

    ASSERT_TRUE(mic_infos.WaitAndClear());
    ASSERT_TRUE(camera_infos.WaitAndClear());
  }

  void InitializePremissionPrompt(
      const std::vector<permissions::RequestType>& request_types) {
    test_delegate_.emplace(GURL("https://test.origin"), request_types,
                           std::vector<std::string>{kMicId},
                           std::vector<std::string>{kCameraId});
    permission_prompt_ = std::make_unique<PermissionPromptBubbleOneOriginView>(
        browser(), test_delegate_->GetWeakPtr(), base::TimeTicks::Now(),
        PermissionPromptStyle::kBubbleOnly);
  }

  void TearDown() override {
    permission_prompt_.reset();
    test_delegate_.reset();
    TestWithBrowserView::TearDown();
  }

  std::u16string GetExpectedCameraLabelText(size_t devices) {
    return l10n_util::GetStringFUTF16(
        IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT_WITH_COUNT,
        base::NumberToString16(devices));
  }

  std::u16string GetExpectedPTZCameraLabelText(size_t devices) {
    return l10n_util::GetStringFUTF16(
        IDS_MEDIA_CAPTURE_CAMERA_PAN_TILT_ZOOM_PERMISSION_FRAGMENT_WITH_COUNT,
        base::NumberToString16(devices));
  }

  std::u16string GetExpectedMicLabelText(size_t devices) {
    return l10n_util::GetStringFUTF16(
        IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT_WITH_COUNT,
        base::NumberToString16(devices));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  media_effects::ScopedFakeAudioService audio_service_;
  media_effects::ScopedFakeVideoCaptureService video_service_;
  std::optional<media_effects::ScopedMediaDeviceInfo> media_device_info_;

  std::optional<TestDelegate> test_delegate_;
  std::unique_ptr<PermissionPromptBubbleOneOriginView> permission_prompt_;
  base::HistogramTester histogram_tester_;
};

// Verify the device counter as well as the tooltip for the mic permission
// label.
TEST_F(PermissionPromptBubbleOneOriginViewTestMediaPreview,
       MediaPreviewMicOnly) {
  InitializePremissionPrompt({permissions::RequestType::kMicStream});
  ASSERT_TRUE(permission_prompt_->GetMediaPreviewsForTesting());
  ASSERT_FALSE(permission_prompt_->GetCameraPermissionLabelForTesting());
  ASSERT_FALSE(permission_prompt_->GetPtzCameraPermissionLabelForTesting());
  auto mic_label = permission_prompt_->GetMicPermissionLabelForTesting();
  ASSERT_TRUE(mic_label);

  EXPECT_EQ(mic_label->GetText(), GetExpectedMicLabelText(0));
  EXPECT_EQ(mic_label->GetTooltipText(), std::u16string());

  ASSERT_TRUE(
      audio_service_.AddFakeInputDeviceBlocking({kMicName, kMicId, kGroupId}));
  EXPECT_EQ(mic_label->GetText(), GetExpectedMicLabelText(1));
  EXPECT_EQ(mic_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kMicName)));

  ASSERT_TRUE(audio_service_.AddFakeInputDeviceBlocking(
      {kMicName2, kMicId2, kGroupId2}));
  EXPECT_EQ(mic_label->GetText(), GetExpectedMicLabelText(2));
  EXPECT_EQ(mic_label->GetTooltipText(),
            base::UTF8ToUTF16(kMicName + std::string("\n") + kMicName2));

  ASSERT_TRUE(audio_service_.RemoveFakeInputDeviceBlocking(kMicId));
  EXPECT_EQ(mic_label->GetText(), GetExpectedMicLabelText(1));
  EXPECT_EQ(mic_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kMicName2)));

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOriginTrialAllowedHistogramName),
              ElementsAre(Bucket(1, 1)));
}

// Verify the device counter as well as the tooltip for the camera permission
// label.
TEST_F(PermissionPromptBubbleOneOriginViewTestMediaPreview,
       MediaPreviewCameraOnly) {
  InitializePremissionPrompt({permissions::RequestType::kCameraStream});
  ASSERT_TRUE(permission_prompt_->GetMediaPreviewsForTesting());
  auto camera_label = permission_prompt_->GetCameraPermissionLabelForTesting();
  ASSERT_TRUE(camera_label);
  ASSERT_FALSE(permission_prompt_->GetPtzCameraPermissionLabelForTesting());
  ASSERT_FALSE(permission_prompt_->GetMicPermissionLabelForTesting());

  EXPECT_EQ(camera_label->GetText(), GetExpectedCameraLabelText(0));
  EXPECT_EQ(camera_label->GetTooltipText(), std::u16string());

  ASSERT_TRUE(video_service_.AddFakeCameraBlocking({kCameraName, kCameraId}));
  EXPECT_EQ(camera_label->GetText(), GetExpectedCameraLabelText(1));
  EXPECT_EQ(camera_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kCameraName)));

  ASSERT_TRUE(video_service_.AddFakeCameraBlocking({kCameraName2, kCameraId2}));
  EXPECT_EQ(camera_label->GetText(), GetExpectedCameraLabelText(2));
  EXPECT_EQ(camera_label->GetTooltipText(),
            base::UTF8ToUTF16(kCameraName + std::string("\n") + kCameraName2));

  ASSERT_TRUE(video_service_.RemoveFakeCameraBlocking(kCameraId2));
  EXPECT_EQ(camera_label->GetText(), GetExpectedCameraLabelText(1));
  EXPECT_EQ(camera_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kCameraName)));

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOriginTrialAllowedHistogramName),
              ElementsAre(Bucket(1, 1)));
}

// Verify the device counter as well as the tooltip for the ptz camera
// permission label.
TEST_F(PermissionPromptBubbleOneOriginViewTestMediaPreview,
       MediaPreviewPTZCameraOnly) {
  InitializePremissionPrompt({permissions::RequestType::kCameraPanTiltZoom});
  ASSERT_TRUE(permission_prompt_->GetMediaPreviewsForTesting());
  ASSERT_FALSE(permission_prompt_->GetCameraPermissionLabelForTesting());
  auto ptz_camera_label =
      permission_prompt_->GetPtzCameraPermissionLabelForTesting();
  ASSERT_TRUE(ptz_camera_label);
  ASSERT_FALSE(permission_prompt_->GetMicPermissionLabelForTesting());

  EXPECT_EQ(ptz_camera_label->GetText(), GetExpectedPTZCameraLabelText(0));
  EXPECT_EQ(ptz_camera_label->GetTooltipText(), std::u16string());

  ASSERT_TRUE(video_service_.AddFakeCameraBlocking({kCameraName, kCameraId}));
  EXPECT_EQ(ptz_camera_label->GetText(), GetExpectedPTZCameraLabelText(1));
  EXPECT_EQ(ptz_camera_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kCameraName)));

  ASSERT_TRUE(video_service_.AddFakeCameraBlocking({kCameraName2, kCameraId2}));
  EXPECT_EQ(ptz_camera_label->GetText(), GetExpectedPTZCameraLabelText(2));
  EXPECT_EQ(ptz_camera_label->GetTooltipText(),
            base::UTF8ToUTF16(kCameraName + std::string("\n") + kCameraName2));

  ASSERT_TRUE(video_service_.RemoveFakeCameraBlocking(kCameraId2));
  EXPECT_EQ(ptz_camera_label->GetText(), GetExpectedPTZCameraLabelText(1));
  EXPECT_EQ(ptz_camera_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kCameraName)));
}

// Verify the device counter as well as the tooltip for both the camera and the
// mic permission labels.
TEST_F(PermissionPromptBubbleOneOriginViewTestMediaPreview,
       MediaPreviewCameraAndMic) {
  InitializePremissionPrompt({permissions::RequestType::kMicStream,
                              permissions::RequestType::kCameraStream});
  ASSERT_TRUE(permission_prompt_->GetMediaPreviewsForTesting());
  auto camera_label = permission_prompt_->GetCameraPermissionLabelForTesting();
  ASSERT_TRUE(camera_label);
  auto mic_label = permission_prompt_->GetMicPermissionLabelForTesting();
  ASSERT_TRUE(mic_label);
  ASSERT_FALSE(permission_prompt_->GetPtzCameraPermissionLabelForTesting());

  EXPECT_EQ(camera_label->GetText(), GetExpectedCameraLabelText(0));
  EXPECT_EQ(camera_label->GetTooltipText(), std::u16string());
  EXPECT_EQ(mic_label->GetText(), GetExpectedMicLabelText(0));
  EXPECT_EQ(mic_label->GetTooltipText(), std::u16string());

  ASSERT_TRUE(video_service_.AddFakeCameraBlocking({kCameraName, kCameraId}));
  ASSERT_TRUE(video_service_.AddFakeCameraBlocking({kCameraName2, kCameraId2}));
  ASSERT_TRUE(
      audio_service_.AddFakeInputDeviceBlocking({kMicName, kMicId, kGroupId}));
  ASSERT_TRUE(audio_service_.AddFakeInputDeviceBlocking(
      {kMicName2, kMicId2, kGroupId2}));

  EXPECT_EQ(camera_label->GetText(), GetExpectedCameraLabelText(2));
  EXPECT_EQ(camera_label->GetTooltipText(),
            base::UTF8ToUTF16(kCameraName + std::string("\n") + kCameraName2));
  EXPECT_EQ(mic_label->GetText(), GetExpectedMicLabelText(2));
  EXPECT_EQ(mic_label->GetTooltipText(),
            base::UTF8ToUTF16(kMicName + std::string("\n") + kMicName2));

  ASSERT_TRUE(video_service_.RemoveFakeCameraBlocking(kCameraId));
  ASSERT_TRUE(audio_service_.RemoveFakeInputDeviceBlocking(kMicId));
  ASSERT_TRUE(video_service_.RemoveFakeCameraBlocking(kCameraId2));

  EXPECT_EQ(camera_label->GetText(), GetExpectedCameraLabelText(0));
  EXPECT_EQ(camera_label->GetTooltipText(), std::u16string());
  EXPECT_EQ(mic_label->GetText(), GetExpectedMicLabelText(1));
  EXPECT_EQ(mic_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kMicName2)));

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOriginTrialAllowedHistogramName),
              ElementsAre(Bucket(1, 1)));
}

// Verify there is no preview created when there is no camera or mic permissions
// requested.
TEST_F(PermissionPromptBubbleOneOriginViewTestMediaPreview,
       MediaPreviewNoCameraOrMic) {
  InitializePremissionPrompt({permissions::RequestType::kGeolocation});
  ASSERT_FALSE(permission_prompt_->GetMediaPreviewsForTesting());
  ASSERT_FALSE(permission_prompt_->GetCameraPermissionLabelForTesting());
  ASSERT_FALSE(permission_prompt_->GetPtzCameraPermissionLabelForTesting());
  ASSERT_FALSE(permission_prompt_->GetMicPermissionLabelForTesting());

  histogram_tester_.ExpectTotalCount(kOriginTrialAllowedHistogramName, 0);
}

#endif
