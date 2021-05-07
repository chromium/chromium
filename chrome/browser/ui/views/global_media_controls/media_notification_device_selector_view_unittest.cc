// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view.h"

#include "base/callback_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/test/button_test_api.h"

using media_router::CastDialogController;
using media_router::CastDialogModel;
using media_router::UIMediaSink;
using media_router::UIMediaSinkState;
using testing::_;

class MediaNotificationContainerObserver;

namespace {

constexpr char kSinkId[] = "sink_id";
constexpr char kSinkFriendlyName[] = "Nest Hub";

UIMediaSink CreateMediaSink(
    UIMediaSinkState state = UIMediaSinkState::AVAILABLE) {
  UIMediaSink sink;
  sink.friendly_name = base::UTF8ToUTF16(kSinkFriendlyName);
  sink.id = kSinkId;
  sink.state = state;
  sink.cast_modes = {media_router::MediaCastMode::PRESENTATION};
  return sink;
}

CastDialogModel CreateModelWithSinks(std::vector<UIMediaSink> sinks) {
  CastDialogModel model;
  model.set_media_sinks(std::move(sinks));
  return model;
}

class MockMediaNotificationDeviceProvider
    : public MediaNotificationDeviceProvider {
 public:
  MockMediaNotificationDeviceProvider() = default;
  ~MockMediaNotificationDeviceProvider() override = default;

  void AddDevice(const std::string& device_name, const std::string& device_id) {
    device_descriptions_.emplace_back(device_name, device_id, "");
  }

  void ResetDevices() { device_descriptions_.clear(); }

  void RunUICallback() { output_devices_callback_.Run(device_descriptions_); }

  base::CallbackListSubscription RegisterOutputDeviceDescriptionsCallback(
      GetOutputDevicesCallback cb) override {
    output_devices_callback_ = std::move(cb);
    RunUICallback();
    return base::CallbackListSubscription();
  }

  MOCK_METHOD(void,
              GetOutputDeviceDescriptions,
              (media::AudioSystem::OnDeviceDescriptionsCallback),
              (override));

 private:
  media::AudioDeviceDescriptions device_descriptions_;

  GetOutputDevicesCallback output_devices_callback_;
};

class MockMediaNotificationDeviceSelectorViewDelegate
    : public MediaNotificationDeviceSelectorViewDelegate {
 public:
  MockMediaNotificationDeviceSelectorViewDelegate() {
    provider_ = std::make_unique<MockMediaNotificationDeviceProvider>();
  }

  MOCK_METHOD(void,
              OnAudioSinkChosen,
              (const std::string& sink_id),
              (override));
  MOCK_METHOD(void, OnDeviceSelectorViewSizeChanged, (), (override));

  base::CallbackListSubscription RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::
          CallbackType callback) override {
    return provider_->RegisterOutputDeviceDescriptionsCallback(
        std::move(callback));
  }

  MockMediaNotificationDeviceProvider* GetProvider() { return provider_.get(); }

  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      base::RepeatingCallback<void(bool)> callback) override {
    callback.Run(supports_switching);
    supports_switching_callback_ = std::move(callback);
    return base::CallbackListSubscription();
  }

  void RunSupportsDeviceSwitchingCallback() {
    supports_switching_callback_.Run(supports_switching);
  }

  bool supports_switching = true;

 private:
  std::unique_ptr<MockMediaNotificationDeviceProvider> provider_;
  base::RepeatingCallback<void(bool)> supports_switching_callback_;
};

class MockCastDialogController : public CastDialogController {
 public:
  MOCK_METHOD1(AddObserver, void(CastDialogController::Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(CastDialogController::Observer* observer));
  MOCK_METHOD2(StartCasting,
               void(const std::string& sink_id,
                    media_router::MediaCastMode cast_mode));
  MOCK_METHOD1(StopCasting, void(const std::string& route_id));
  MOCK_METHOD1(
      ChooseLocalFile,
      void(base::OnceCallback<void(const ui::SelectedFileInfo*)> callback));
  MOCK_METHOD1(ClearIssue, void(const media_router::Issue::Id& issue_id));
};
}  // anonymous namespace

class MediaNotificationDeviceSelectorViewTest : public ChromeViewsTestBase {
 public:
  MediaNotificationDeviceSelectorViewTest() = default;
  ~MediaNotificationDeviceSelectorViewTest() override = default;

  // ChromeViewsTestBase
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    feature_list_.InitAndEnableFeature(
        media::kGlobalMediaControlsSeamlessTransfer);
  }

  void TearDown() override {
    view_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void AddAudioDevices(
      MockMediaNotificationDeviceSelectorViewDelegate& delegate) {
    auto* provider = delegate.GetProvider();
    provider->AddDevice("Speaker", "1");
    provider->AddDevice("Headphones", "2");
    provider->AddDevice("Earbuds", "3");
  }

  void SimulateButtonClick(views::View* view) {
    views::test::ButtonTestApi(static_cast<views::Button*>(view))
        .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(), 0, 0));
  }

  std::string EntryLabelText(views::View* entry_view) {
    return view_->GetEntryLabelForTesting(entry_view);
  }

  bool IsHighlighted(views::View* entry_view) {
    return view_->GetEntryIsHighlightedForTesting(entry_view);
  }

  std::string GetButtonText(views::View* view) {
    return base::UTF16ToUTF8(static_cast<views::LabelButton*>(view)->GetText());
  }

  std::unique_ptr<MediaNotificationDeviceSelectorView> CreateDeviceSelectorView(
      MockMediaNotificationDeviceSelectorViewDelegate* delegate,
      std::unique_ptr<MockCastDialogController> controller =
          std::make_unique<MockCastDialogController>(),
      const std::string& device_description = "1",
      bool has_audio_output = true,
      GlobalMediaControlsEntryPoint entry_point =
          GlobalMediaControlsEntryPoint::kToolbarIcon) {
    return std::make_unique<MediaNotificationDeviceSelectorView>(
        delegate, std::move(controller), has_audio_output, device_description,
        gfx::kPlaceholderColor, gfx::kPlaceholderColor, entry_point);
  }

  std::unique_ptr<MediaNotificationDeviceSelectorView> view_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MediaNotificationDeviceSelectorViewTest, DeviceButtonsCreated) {
  // Buttons should be created for every device reported by the provider.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);
  view_->OnModelUpdated(CreateModelWithSinks({CreateMediaSink()}));

  ASSERT_TRUE(view_->device_entry_views_container_ != nullptr);

  auto container_children = view_->device_entry_views_container_->children();
  ASSERT_EQ(container_children.size(), 4u);

  EXPECT_EQ(EntryLabelText(container_children.at(0)), "Speaker");
  EXPECT_EQ(EntryLabelText(container_children.at(1)), "Headphones");
  EXPECT_EQ(EntryLabelText(container_children.at(2)), "Earbuds");
  EXPECT_EQ(EntryLabelText(container_children.at(3)), kSinkFriendlyName);
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       ExpandButtonOpensEntryContainer) {
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);

  ASSERT_TRUE(view_->expand_button_);
  EXPECT_FALSE(view_->device_entry_views_container_->GetVisible());
  SimulateButtonClick(view_->GetExpandButtonForTesting());
  EXPECT_TRUE(view_->device_entry_views_container_->GetVisible());
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       DeviceEntryContainerVisibility) {
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  AddAudioDevices(delegate);

  // The device entry container should be collapsed if the media dialog is
  // opened from the toolbar or Chrome OS system tray.
  view_ = CreateDeviceSelectorView(&delegate);
  EXPECT_FALSE(view_->device_entry_views_container_->GetVisible());

  // The device entry container should be expanded if the media dialog is opened
  // for a presentation request.
  view_ = CreateDeviceSelectorView(
      &delegate, std::make_unique<MockCastDialogController>(), "1",
      /* has_audio_output */ true,
      GlobalMediaControlsEntryPoint::kPresentation);
  EXPECT_TRUE(view_->device_entry_views_container_->GetVisible());
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       AudioDeviceButtonClickNotifiesContainer) {
  // When buttons are clicked the media notification delegate should be
  // informed.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);

  EXPECT_CALL(delegate, OnAudioSinkChosen("1")).Times(1);
  EXPECT_CALL(delegate, OnAudioSinkChosen("2")).Times(1);
  EXPECT_CALL(delegate, OnAudioSinkChosen("3")).Times(1);

  for (views::View* child : view_->device_entry_views_container_->children()) {
    SimulateButtonClick(child);
  }
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       CastDeviceButtonClickStartsCasting) {
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto cast_controller = std::make_unique<MockCastDialogController>();
  auto* cast_controller_ptr = cast_controller.get();
  view_ = CreateDeviceSelectorView(&delegate, std::move(cast_controller));

  // Clicking on connecting or disconnecting sinks will not start casting.
  view_->OnModelUpdated(
      CreateModelWithSinks({CreateMediaSink(UIMediaSinkState::CONNECTING),
                            CreateMediaSink(UIMediaSinkState::DISCONNECTING)}));
  EXPECT_CALL(*cast_controller_ptr, StartCasting(_, _)).Times(0);
  for (views::View* child : view_->device_entry_views_container_->children()) {
    SimulateButtonClick(child);
  }

  // Clicking on available or connected sinks will start casting.
  view_->OnModelUpdated(CreateModelWithSinks(
      {CreateMediaSink(), CreateMediaSink(UIMediaSinkState::CONNECTED)}));
  EXPECT_CALL(*cast_controller_ptr,
              StartCasting(_, media_router::MediaCastMode::PRESENTATION))
      .Times(2);
  for (views::View* child : view_->device_entry_views_container_->children()) {
    SimulateButtonClick(child);
  }
}

TEST_F(MediaNotificationDeviceSelectorViewTest, CurrentAudioDeviceHighlighted) {
  // The 'current' audio device should be highlighted in the UI and appear
  // before other devices.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(
      &delegate, std::make_unique<MockCastDialogController>(), "3");

  auto* first_entry = view_->device_entry_views_container_->children().front();
  EXPECT_EQ(EntryLabelText(first_entry), "Earbuds");
  EXPECT_TRUE(IsHighlighted(first_entry));
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       AudioDeviceHighlightedOnChange) {
  // When the audio output device changes, the UI should highlight that one.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);

  auto& container_children = view_->device_entry_views_container_->children();

  // There should be only one highlighted button. It should be the first button.
  // It's text should be "Speaker"
  auto highlight_pred = [this](views::View* v) { return IsHighlighted(v); };
  EXPECT_EQ(base::ranges::count_if(container_children, highlight_pred), 1);
  EXPECT_EQ(base::ranges::find_if(container_children, highlight_pred),
            container_children.begin());
  EXPECT_EQ(EntryLabelText(container_children.front()), "Speaker");

  // Simulate a device change
  view_->UpdateCurrentAudioDevice("3");

  // The button for "Earbuds" should come before all others & be highlighted.
  EXPECT_EQ(base::ranges::count_if(container_children, highlight_pred), 1);
  EXPECT_EQ(base::ranges::find_if(container_children, highlight_pred),
            container_children.begin());
  EXPECT_EQ(EntryLabelText(container_children.front()), "Earbuds");
}

TEST_F(MediaNotificationDeviceSelectorViewTest, AudioDeviceButtonsChange) {
  // If the device provider reports a change in connect audio devices, the UI
  // should update accordingly.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);

  auto* provider = delegate.GetProvider();
  provider->ResetDevices();
  // Make "Monitor" the default device.
  provider->AddDevice("Monitor",
                      media::AudioDeviceDescription::kDefaultDeviceId);
  provider->RunUICallback();

  {
    auto& container_children = view_->device_entry_views_container_->children();
    EXPECT_EQ(container_children.size(), 1u);
    ASSERT_FALSE(container_children.empty());
    EXPECT_EQ(EntryLabelText(container_children.front()), "Monitor");

    // When the device highlighted in the UI is removed, the UI should fall back
    // to highlighting the default device.
    EXPECT_TRUE(IsHighlighted(container_children.front()));
  }

  provider->ResetDevices();
  AddAudioDevices(delegate);
  provider->RunUICallback();

  {
    auto& container_children = view_->device_entry_views_container_->children();
    EXPECT_EQ(container_children.size(), 3u);
    ASSERT_FALSE(container_children.empty());

    // When the device highlighted in the UI is removed, and there is no default
    // device, the UI should not highlight any of the devices.
    for (auto* device_view : container_children) {
      EXPECT_FALSE(IsHighlighted(device_view));
    }
  }
}

TEST_F(MediaNotificationDeviceSelectorViewTest, VisibilityChanges) {
  // The device selector view should become hidden when there is only one
  // unique device, unless there exists a cast device.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice(media::AudioDeviceDescription::GetDefaultDeviceName(),
                      media::AudioDeviceDescription::kDefaultDeviceId);

  EXPECT_CALL(delegate, OnDeviceSelectorViewSizeChanged).Times(2);
  view_ = CreateDeviceSelectorView(
      &delegate, std::make_unique<MockCastDialogController>(),
      media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_FALSE(view_->GetVisible());

  testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, OnDeviceSelectorViewSizeChanged);
  view_->OnModelUpdated(CreateModelWithSinks({CreateMediaSink()}));
  EXPECT_TRUE(view_->GetVisible());

  testing::Mock::VerifyAndClearExpectations(&delegate);
  view_->OnModelUpdated(CreateModelWithSinks({}));
  provider->ResetDevices();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones",
                      media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_CALL(delegate, OnDeviceSelectorViewSizeChanged).Times(1);
  provider->RunUICallback();
  EXPECT_TRUE(view_->GetVisible());
  testing::Mock::VerifyAndClearExpectations(&delegate);

  provider->ResetDevices();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice(media::AudioDeviceDescription::GetDefaultDeviceName(),
                      media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_CALL(delegate, OnDeviceSelectorViewSizeChanged).Times(1);
  provider->RunUICallback();
  EXPECT_TRUE(view_->GetVisible());
  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       AudioDeviceChangeIsNotSupported) {
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  AddAudioDevices(delegate);
  delegate.supports_switching = false;

  view_ = CreateDeviceSelectorView(
      &delegate, std::make_unique<MockCastDialogController>(),
      media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_FALSE(view_->GetVisible());

  delegate.supports_switching = true;
  delegate.RunSupportsDeviceSwitchingCallback();
  EXPECT_TRUE(view_->GetVisible());
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       CastDeviceButtonClickClearsIssue) {
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto cast_controller = std::make_unique<MockCastDialogController>();
  auto* cast_controller_ptr = cast_controller.get();
  view_ = CreateDeviceSelectorView(&delegate, std::move(cast_controller));

  // Clicking on sinks with issue will clear up the issue instead of starting a
  // cast session.
  auto sink = CreateMediaSink();
  media_router::IssueInfo issue_info(
      "Issue Title", media_router::IssueInfo::Action::DISMISS,
      media_router::IssueInfo::Severity::WARNING);
  media_router::Issue issue(issue_info);
  sink.issue = issue;

  view_->OnModelUpdated(CreateModelWithSinks({sink}));
  EXPECT_CALL(*cast_controller_ptr, StartCasting(_, _)).Times(0);
  EXPECT_CALL(*cast_controller_ptr, ClearIssue(issue.id()));
  for (views::View* child : view_->device_entry_views_container_->children()) {
    SimulateButtonClick(child);
  }
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       AudioDevicesCountHistogramRecorded) {
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  AddAudioDevices(delegate);

  histogram_tester_.ExpectTotalCount(kAudioDevicesCountHistogramName, 0);

  view_ =
      CreateDeviceSelectorView(&delegate, /* CastDialogController */ nullptr);
  view_->ShowDevices();

  histogram_tester_.ExpectTotalCount(kAudioDevicesCountHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kAudioDevicesCountHistogramName, 3, 1);

  auto* provider = delegate.GetProvider();
  provider->AddDevice("Monitor", "4");
  provider->RunUICallback();

  histogram_tester_.ExpectTotalCount(kAudioDevicesCountHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kAudioDevicesCountHistogramName, 3, 1);
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       DeviceSelectorAvailableHistogramRecorded) {
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker",
                      media::AudioDeviceDescription::kDefaultDeviceId);
  delegate.supports_switching = false;

  histogram_tester_.ExpectTotalCount(kDeviceSelectorAvailableHistogramName, 0);

  view_ =
      CreateDeviceSelectorView(&delegate, /* CastDialogController */ nullptr);

  EXPECT_FALSE(view_->GetVisible());
  histogram_tester_.ExpectTotalCount(kDeviceSelectorAvailableHistogramName, 0);

  provider->AddDevice("Headphones", "2");
  provider->RunUICallback();

  EXPECT_FALSE(view_->GetVisible());
  histogram_tester_.ExpectTotalCount(kDeviceSelectorAvailableHistogramName, 0);

  view_.reset();
  histogram_tester_.ExpectTotalCount(kDeviceSelectorAvailableHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kDeviceSelectorAvailableHistogramName,
                                      false, 1);

  delegate.supports_switching = true;
  view_ =
      CreateDeviceSelectorView(&delegate, /* CastDialogController */ nullptr);

  EXPECT_TRUE(view_->GetVisible());
  histogram_tester_.ExpectTotalCount(kDeviceSelectorAvailableHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kDeviceSelectorAvailableHistogramName,
                                      true, 1);

  view_.reset();

  histogram_tester_.ExpectTotalCount(kDeviceSelectorAvailableHistogramName, 2);
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       DeviceSelectorOpenedHistogramRecorded) {
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker",
                      media::AudioDeviceDescription::kDefaultDeviceId);
  provider->AddDevice("Headphones", "2");
  delegate.supports_switching = false;

  histogram_tester_.ExpectTotalCount(kDeviceSelectorOpenedHistogramName, 0);

  view_ =
      CreateDeviceSelectorView(&delegate, /* CastDialogController */ nullptr);
  EXPECT_FALSE(view_->GetVisible());
  view_.reset();

  // The histrogram should not be recorded when the device selector is not
  // available.
  histogram_tester_.ExpectTotalCount(kDeviceSelectorOpenedHistogramName, 0);

  delegate.supports_switching = true;
  view_ =
      CreateDeviceSelectorView(&delegate, /* CastDialogController */ nullptr);
  view_.reset();

  histogram_tester_.ExpectTotalCount(kDeviceSelectorOpenedHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kDeviceSelectorOpenedHistogramName, false,
                                      1);

  view_ =
      CreateDeviceSelectorView(&delegate, /* CastDialogController */ nullptr);
  view_->ShowDevices();

  histogram_tester_.ExpectTotalCount(kDeviceSelectorOpenedHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kDeviceSelectorOpenedHistogramName, true,
                                      1);

  view_.reset();

  histogram_tester_.ExpectTotalCount(kDeviceSelectorOpenedHistogramName, 2);
}
