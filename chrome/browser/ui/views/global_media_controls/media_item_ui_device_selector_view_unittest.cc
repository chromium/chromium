// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_device_selector_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/test/button_test_api.h"

using media_router::CastDialogController;
using media_router::CastDialogModel;
using media_router::UIMediaSink;
using media_router::UIMediaSinkState;
using testing::_;
using testing::NiceMock;

namespace {

constexpr char kItemId[] = "item_id";
constexpr char kSinkId[] = "sink_id";
constexpr char kSinkFriendlyName[] = "Nest Hub";
constexpr char16_t kSinkFriendlyName16[] = u"Nest Hub";

ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED,
                             gfx::Point(),
                             gfx::Point(),
                             ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);

UIMediaSink CreateMediaSink(
    UIMediaSinkState state = UIMediaSinkState::AVAILABLE) {
  UIMediaSink sink{media_router::mojom::MediaRouteProviderId::CAST};
  sink.friendly_name = kSinkFriendlyName16;
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

class MockMediaItemUIDeviceSelectorDelegate
    : public MediaItemUIDeviceSelectorDelegate {
 public:
  MockMediaItemUIDeviceSelectorDelegate() {
    provider_ = std::make_unique<MockMediaNotificationDeviceProvider>();
  }

  MOCK_METHOD(void,
              OnAudioSinkChosen,
              (const std::string& item_id, const std::string& sink_id),
              (override));
  MOCK_METHOD(bool,
              OnMediaRemotingRequested,
              (const std::string& item_id),
              (override));

  base::CallbackListSubscription RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::
          CallbackType callback) override {
    return provider_->RegisterOutputDeviceDescriptionsCallback(
        std::move(callback));
  }

  MockMediaNotificationDeviceProvider* GetProvider() { return provider_.get(); }

  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      const std::string& item_id,
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
  MOCK_METHOD1(ClearIssue, void(const media_router::Issue::Id& issue_id));
  MOCK_METHOD0(TakeMediaRouteStarter,
               std::unique_ptr<media_router::MediaRouteStarter>());
  MOCK_METHOD1(RegisterDestructor, void(base::OnceClosure));
};

}  // anonymous namespace

class MediaItemUIDeviceSelectorViewTest : public ChromeViewsTestBase {
 public:
  MediaItemUIDeviceSelectorViewTest() = default;
  ~MediaItemUIDeviceSelectorViewTest() override = default;

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

  void AddAudioDevices(MockMediaItemUIDeviceSelectorDelegate& delegate) {
    auto* provider = delegate.GetProvider();
    provider->AddDevice("Speaker", "1");
    provider->AddDevice("Headphones", "2");
    provider->AddDevice("Earbuds", "3");
  }

  void SimulateButtonClick(views::View* view) {
    views::test::ButtonTestApi(static_cast<views::Button*>(view))
        .NotifyClick(pressed_event);
  }

  void SimulateMouseClick(views::View* view) {
    view->OnMousePressed(pressed_event);
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

  views::View* GetDeviceEntryViewsContainer() {
    return view_->device_entry_views_container_;
  }

  std::unique_ptr<MediaItemUIDeviceSelectorView> CreateDeviceSelectorView(
      MockMediaItemUIDeviceSelectorDelegate* delegate,
      std::unique_ptr<MockCastDialogController> controller =
          std::make_unique<NiceMock<MockCastDialogController>>(),
      const std::string& current_device = "1",
      bool has_audio_output = true,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point =
          global_media_controls::GlobalMediaControlsEntryPoint::kToolbarIcon) {
    auto device_selector_view = std::make_unique<MediaItemUIDeviceSelectorView>(
        kItemId, delegate, std::move(controller), has_audio_output,
        entry_point);
    device_selector_view->UpdateCurrentAudioDevice(current_device);
    return device_selector_view;
  }

  void CallOnModelUpdated(const std::string& sink_id,
                          media_router::MediaCastMode cast_mode) {
    auto cast_connected_sink = CreateMediaSink(UIMediaSinkState::CONNECTED);
    view_->OnModelUpdated(CreateModelWithSinks({cast_connected_sink}));
  }

  std::unique_ptr<MediaItemUIDeviceSelectorView> view_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MediaItemUIDeviceSelectorViewTest, DeviceButtonsCreated) {
  // Buttons should be created for every device reported by the provider.
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);
  view_->OnModelUpdated(CreateModelWithSinks({CreateMediaSink()}));

  ASSERT_TRUE(GetDeviceEntryViewsContainer() != nullptr);

  auto container_children = GetDeviceEntryViewsContainer()->children();
  ASSERT_EQ(container_children.size(), 4u);

  EXPECT_EQ(EntryLabelText(container_children.at(0)), "Speaker");
  EXPECT_EQ(EntryLabelText(container_children.at(1)), "Headphones");
  EXPECT_EQ(EntryLabelText(container_children.at(2)), "Earbuds");
  EXPECT_EQ(EntryLabelText(container_children.at(3)), kSinkFriendlyName);
}

TEST_F(MediaItemUIDeviceSelectorViewTest, ExpandButtonAndLabelCreated) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);
  EXPECT_EQ(view_->GetExpandDeviceSelectorLabelForTesting()->GetText(),
            l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_DEVICES_LABEL));
  EXPECT_TRUE(view_->GetDropdownButtonForTesting());

  view_ = CreateDeviceSelectorView(
      &delegate, std::make_unique<NiceMock<MockCastDialogController>>(), "1",
      true,
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation);
  EXPECT_EQ(view_->GetExpandDeviceSelectorLabelForTesting()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_GLOBAL_MEDIA_CONTROLS_DEVICES_LABEL_WITH_COLON));
  EXPECT_FALSE(view_->GetDropdownButtonForTesting());
}

TEST_F(MediaItemUIDeviceSelectorViewTest, ExpandButtonOpensEntryContainer) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);

  // Clicking on the dropdown button should expand the device list.
  ASSERT_TRUE(view_->GetDropdownButtonForTesting());
  EXPECT_FALSE(view_->GetDeviceEntryViewVisibilityForTesting());
  SimulateButtonClick(view_->GetDropdownButtonForTesting());
  EXPECT_TRUE(view_->GetDeviceEntryViewVisibilityForTesting());
}

TEST_F(MediaItemUIDeviceSelectorViewTest, ExpandLabelOpensEntryContainer) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);

  // Clicking on the device selector view should expand the device list.
  ASSERT_TRUE(view_.get());
  EXPECT_FALSE(view_->GetDeviceEntryViewVisibilityForTesting());
  SimulateMouseClick(view_.get());
  EXPECT_TRUE(view_->GetDeviceEntryViewVisibilityForTesting());
}

TEST_F(MediaItemUIDeviceSelectorViewTest, DeviceEntryContainerVisibility) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);

  // The device entry container should be collapsed if the media dialog is
  // opened from the toolbar or Chrome OS system tray.
  view_ = CreateDeviceSelectorView(&delegate);
  EXPECT_FALSE(view_->GetDeviceEntryViewVisibilityForTesting());

  // The device entry container should be expanded if the media dialog is opened
  // for a presentation request.
  view_ = CreateDeviceSelectorView(
      &delegate, std::make_unique<NiceMock<MockCastDialogController>>(), "1",
      /* has_audio_output */ true,
      global_media_controls::GlobalMediaControlsEntryPoint::kPresentation);
  EXPECT_TRUE(view_->GetDeviceEntryViewVisibilityForTesting());
}

TEST_F(MediaItemUIDeviceSelectorViewTest,
       AudioDeviceButtonClickNotifiesContainer) {
  // When buttons are clicked the media notification delegate should be
  // informed.
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);

  EXPECT_CALL(delegate, OnAudioSinkChosen(kItemId, "1")).Times(1);
  EXPECT_CALL(delegate, OnAudioSinkChosen(kItemId, "2")).Times(1);
  EXPECT_CALL(delegate, OnAudioSinkChosen(kItemId, "3")).Times(1);

  for (views::View* child : GetDeviceEntryViewsContainer()->children()) {
    SimulateButtonClick(child);
  }
}

TEST_F(MediaItemUIDeviceSelectorViewTest,
       CastDeviceButtonClickStartsCasting_Presentation) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  auto cast_controller = std::make_unique<NiceMock<MockCastDialogController>>();
  auto* cast_controller_ptr = cast_controller.get();
  view_ = CreateDeviceSelectorView(&delegate, std::move(cast_controller));

  // Clicking on connecting or disconnecting sinks will not start casting.
  view_->OnModelUpdated(
      CreateModelWithSinks({CreateMediaSink(UIMediaSinkState::CONNECTING),
                            CreateMediaSink(UIMediaSinkState::DISCONNECTING)}));
  EXPECT_CALL(*cast_controller_ptr, StartCasting(_, _)).Times(0);
  for (views::View* child : GetDeviceEntryViewsContainer()->children()) {
    SimulateButtonClick(child);
  }

  // Clicking on available or connected CAST sinks will start casting.
  auto cast_connected_sink = CreateMediaSink(UIMediaSinkState::CONNECTED);
  cast_connected_sink.provider =
      media_router::mojom::MediaRouteProviderId::CAST;
  auto cast_remote_playback_sink = CreateMediaSink();
  cast_remote_playback_sink.cast_modes = {
      media_router::MediaCastMode::REMOTE_PLAYBACK};
  view_->OnModelUpdated(CreateModelWithSinks(
      {CreateMediaSink(), cast_connected_sink, cast_remote_playback_sink}));
  EXPECT_CALL(*cast_controller_ptr,
              StartCasting(_, media_router::MediaCastMode::PRESENTATION))
      .Times(2);
  EXPECT_CALL(*cast_controller_ptr,
              StartCasting(_, media_router::MediaCastMode::REMOTE_PLAYBACK));
  for (views::View* child : GetDeviceEntryViewsContainer()->children()) {
    SimulateButtonClick(child);
  }

  // Clicking on connected DIAL sinks will terminate casting.
  // TODO(crbug.com/1206830): change test cases after DIAL MRP supports
  // launching session on a connected sink.
  auto dial_connected_sink = CreateMediaSink(UIMediaSinkState::CONNECTED);
  dial_connected_sink.provider =
      media_router::mojom::MediaRouteProviderId::DIAL;
  dial_connected_sink.route =
      media_router::MediaRoute("routeId1", media_router::MediaSource("source1"),
                               "sinkId1", "description", true);
  view_->OnModelUpdated(CreateModelWithSinks({dial_connected_sink}));
  EXPECT_CALL(*cast_controller_ptr, StopCasting("routeId1"));
  for (views::View* child : GetDeviceEntryViewsContainer()->children()) {
    SimulateButtonClick(child);
  }
}

TEST_F(MediaItemUIDeviceSelectorViewTest,
       StartCastingTriggersAnotherSinkUpdate) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  auto cast_controller = std::make_unique<NiceMock<MockCastDialogController>>();
  auto* cast_controller_ptr = cast_controller.get();
  view_ = CreateDeviceSelectorView(&delegate, std::move(cast_controller));

  view_->OnModelUpdated(CreateModelWithSinks({CreateMediaSink()}));
  EXPECT_CALL(*cast_controller_ptr,
              StartCasting(_, media_router::MediaCastMode::PRESENTATION));
  // CastDialogController::StartCasting() should create a new route, which
  // triggers the MediaRouterUI to broadcast a sink update. As a result
  // MediaItemUIDeviceSelectorView::OnModelUpdated() should be called before
  // StartCasting() returns. This test verifies that the second the second call
  // to OnModelUpdated() does not cause UaF error in
  // RecordStartCastingMetrics().
  ON_CALL(*cast_controller_ptr, StartCasting(_, _))
      .WillByDefault(
          Invoke(this, &MediaItemUIDeviceSelectorViewTest::CallOnModelUpdated));
  for (views::View* child : GetDeviceEntryViewsContainer()->children()) {
    SimulateButtonClick(child);
  }
}

TEST_F(MediaItemUIDeviceSelectorViewTest, CurrentAudioDeviceHighlighted) {
  // The 'current' audio device should be highlighted in the UI and appear
  // before other devices.
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(
      &delegate, std::make_unique<NiceMock<MockCastDialogController>>(), "3");

  auto* first_entry = GetDeviceEntryViewsContainer()->children().front();
  EXPECT_EQ(EntryLabelText(first_entry), "Earbuds");
  EXPECT_TRUE(IsHighlighted(first_entry));
}

TEST_F(MediaItemUIDeviceSelectorViewTest, AudioDeviceHighlightedOnChange) {
  // When the audio output device changes, the UI should highlight that one.
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);

  auto& container_children = GetDeviceEntryViewsContainer()->children();

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

TEST_F(MediaItemUIDeviceSelectorViewTest, AudioDeviceButtonsChange) {
  // If the device provider reports a change in connect audio devices, the UI
  // should update accordingly.
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);

  auto* provider = delegate.GetProvider();
  provider->ResetDevices();
  // Make "Monitor" the default device.
  provider->AddDevice("Monitor",
                      media::AudioDeviceDescription::kDefaultDeviceId);
  provider->RunUICallback();

  {
    auto& container_children = GetDeviceEntryViewsContainer()->children();
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
    auto& container_children = GetDeviceEntryViewsContainer()->children();
    EXPECT_EQ(container_children.size(), 3u);
    ASSERT_FALSE(container_children.empty());

    // When the device highlighted in the UI is removed, and there is no default
    // device, the UI should not highlight any of the devices.
    for (auto* device_view : container_children) {
      EXPECT_FALSE(IsHighlighted(device_view));
    }
  }
}

TEST_F(MediaItemUIDeviceSelectorViewTest, VisibilityChanges) {
  // The device selector view should become hidden when there is only one
  // unique device, unless there exists a cast device.
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice(media::AudioDeviceDescription::GetDefaultDeviceName(),
                      media::AudioDeviceDescription::kDefaultDeviceId);

  view_ = CreateDeviceSelectorView(
      &delegate, std::make_unique<NiceMock<MockCastDialogController>>(),
      media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_FALSE(view_->GetVisible());

  testing::Mock::VerifyAndClearExpectations(&delegate);

  view_->OnModelUpdated(CreateModelWithSinks({CreateMediaSink()}));
  EXPECT_TRUE(view_->GetVisible());

  testing::Mock::VerifyAndClearExpectations(&delegate);
  view_->OnModelUpdated(CreateModelWithSinks({}));
  provider->ResetDevices();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones",
                      media::AudioDeviceDescription::kDefaultDeviceId);
  provider->RunUICallback();
  EXPECT_TRUE(view_->GetVisible());
  testing::Mock::VerifyAndClearExpectations(&delegate);

  provider->ResetDevices();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice(media::AudioDeviceDescription::GetDefaultDeviceName(),
                      media::AudioDeviceDescription::kDefaultDeviceId);
  provider->RunUICallback();
  EXPECT_TRUE(view_->GetVisible());
  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(MediaItemUIDeviceSelectorViewTest, AudioDeviceChangeIsNotSupported) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  delegate.supports_switching = false;

  view_ = CreateDeviceSelectorView(
      &delegate, std::make_unique<NiceMock<MockCastDialogController>>(),
      media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_FALSE(view_->GetVisible());

  delegate.supports_switching = true;
  delegate.RunSupportsDeviceSwitchingCallback();
  EXPECT_TRUE(view_->GetVisible());
}

TEST_F(MediaItemUIDeviceSelectorViewTest, CastDeviceButtonClickClearsIssue) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  auto cast_controller = std::make_unique<NiceMock<MockCastDialogController>>();
  auto* cast_controller_ptr = cast_controller.get();
  view_ = CreateDeviceSelectorView(&delegate, std::move(cast_controller));

  // Clicking on sinks with issue will clear up the issue instead of starting a
  // cast session.
  auto sink = CreateMediaSink();
  media_router::IssueInfo issue_info(
      "Issue Title", media_router::IssueInfo::Severity::WARNING);
  media_router::Issue issue(issue_info);
  sink.issue = issue;

  view_->OnModelUpdated(CreateModelWithSinks({sink}));
  EXPECT_CALL(*cast_controller_ptr, StartCasting(_, _)).Times(0);
  EXPECT_CALL(*cast_controller_ptr, ClearIssue(issue.id()));
  for (views::View* child : GetDeviceEntryViewsContainer()->children()) {
    SimulateButtonClick(child);
  }
}

TEST_F(MediaItemUIDeviceSelectorViewTest, AudioDevicesCountHistogramRecorded) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
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

TEST_F(MediaItemUIDeviceSelectorViewTest,
       DeviceSelectorAvailableHistogramRecorded) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
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

TEST_F(MediaItemUIDeviceSelectorViewTest,
       DeviceSelectorOpenedHistogramRecorded) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
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
