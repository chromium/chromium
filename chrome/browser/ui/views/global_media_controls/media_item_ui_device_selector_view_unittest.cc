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
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/test/button_test_api.h"

using global_media_controls::test::MockDeviceListHost;
using media_router::CastDialogController;
using media_router::CastDialogModel;
using media_router::UIMediaSink;
using media_router::UIMediaSinkState;
using testing::_;
using testing::NiceMock;

namespace {

constexpr char kItemId[] = "item_id";
constexpr char kSinkFriendlyName[] = "Nest Hub";

ui::MouseEvent pressed_event(ui::EventType::kMousePressed,
                             gfx::Point(),
                             gfx::Point(),
                             ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);

global_media_controls::mojom::DevicePtr CreateDevice() {
  auto device = global_media_controls::mojom::Device::New();
  device->name = kSinkFriendlyName;
  return device;
}

std::vector<global_media_controls::mojom::DevicePtr> CreateDevices() {
  std::vector<global_media_controls::mojom::DevicePtr> devices;
  devices.push_back(CreateDevice());
  return devices;
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
  MOCK_METHOD(void,
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
  MOCK_METHOD(void,
              AddObserver,
              (CastDialogController::Observer * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (CastDialogController::Observer * observer),
              (override));
  MOCK_METHOD(void,
              StartCasting,
              (const std::string& sink_id,
               media_router::MediaCastMode cast_mode),
              (override));
  MOCK_METHOD(void, StopCasting, (const std::string& route_id), (override));
  MOCK_METHOD(void,
              ClearIssue,
              (const media_router::Issue::Id& issue_id),
              (override));
  MOCK_METHOD(void, FreezeRoute, (const std::string& route_id), (override));
  MOCK_METHOD(void, UnfreezeRoute, (const std::string& route_id), (override));
  MOCK_METHOD(std::unique_ptr<media_router::MediaRouteStarter>,
              TakeMediaRouteStarter,
              (),
              (override));
  MOCK_METHOD(void, RegisterDestructor, (base::OnceClosure), (override));
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
      const std::string& current_device = "1",
      bool has_audio_output = true,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point =
          global_media_controls::GlobalMediaControlsEntryPoint::kToolbarIcon,
      bool show_devices = false) {
    client_remote_.reset();
    device_list_host_ = std::make_unique<MockDeviceListHost>();
    auto device_selector_view = std::make_unique<MediaItemUIDeviceSelectorView>(
        kItemId, delegate, device_list_host_->PassRemote(),
        client_remote_.BindNewPipeAndPassReceiver(), has_audio_output,
        entry_point, show_devices);
    device_selector_view->UpdateCurrentAudioDevice(current_device);
    return device_selector_view;
  }

  void OnDevicesUpdated(const std::string& sink_id) {
    CHECK(view_);
    view_->OnDevicesUpdated(CreateDevices());
  }

  std::unique_ptr<MediaItemUIDeviceSelectorView> view_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<global_media_controls::mojom::DeviceListClient> client_remote_;
  std::unique_ptr<MockDeviceListHost> device_list_host_;
};

TEST_F(MediaItemUIDeviceSelectorViewTest, DeviceButtonsCreated) {
  // Buttons should be created for every device reported by the provider.
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate);
  view_->OnDevicesUpdated(CreateDevices());

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
      &delegate, "1", true,
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

  // The device entry container should be expanded if it is requested to show
  // devices.
  view_ = CreateDeviceSelectorView(
      &delegate, "1",
      /*has_audio_output=*/true,
      global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray,
      /*show_devices=*/true);
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
       StartCastingTriggersAnotherSinkUpdate) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  view_ = CreateDeviceSelectorView(&delegate);
  view_->OnDevicesUpdated(CreateDevices());
  // DeviceListHost::SelectDevice() should create a new route, which
  // triggers the MediaRouterUI to broadcast a sink update. As a result
  // MediaItemUIDeviceSelectorView::OnDevicesUpdated() may be called before
  // SelectDevice() returns. This test verifies that the second the second call
  // to OnDevicesUpdated() does not cause UaF error in
  // RecordStartCastingMetrics().
  EXPECT_CALL(*device_list_host_, SelectDevice(_))
      .WillOnce(
          Invoke(this, &MediaItemUIDeviceSelectorViewTest::OnDevicesUpdated));
  for (views::View* child : GetDeviceEntryViewsContainer()->children()) {
    SimulateButtonClick(child);
  }
  // The button click should cause the client to call SelectDevice() on the
  // host.
  client_remote_.FlushForTesting();
}

TEST_F(MediaItemUIDeviceSelectorViewTest, CurrentAudioDeviceHighlighted) {
  // The 'current' audio device should be highlighted in the UI and appear
  // before other devices.
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);
  view_ = CreateDeviceSelectorView(&delegate, "3");

  auto* first_entry = GetDeviceEntryViewsContainer()->children().front().get();
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
    for (views::View* device_view : container_children) {
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
      &delegate, media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_FALSE(view_->GetVisible());

  testing::Mock::VerifyAndClearExpectations(&delegate);

  view_->OnDevicesUpdated(CreateDevices());
  EXPECT_TRUE(view_->GetVisible());

  testing::Mock::VerifyAndClearExpectations(&delegate);
  view_->OnDevicesUpdated({});
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
      &delegate, media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_FALSE(view_->GetVisible());

  delegate.supports_switching = true;
  delegate.RunSupportsDeviceSwitchingCallback();
  EXPECT_TRUE(view_->GetVisible());
}

TEST_F(MediaItemUIDeviceSelectorViewTest, AudioDevicesCountHistogramRecorded) {
  NiceMock<MockMediaItemUIDeviceSelectorDelegate> delegate;
  AddAudioDevices(delegate);

  histogram_tester_.ExpectTotalCount(kAudioDevicesCountHistogramName, 0);

  view_ = CreateDeviceSelectorView(&delegate);
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

  view_ = CreateDeviceSelectorView(&delegate);

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
  view_ = CreateDeviceSelectorView(&delegate);

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

  view_ = CreateDeviceSelectorView(&delegate);
  EXPECT_FALSE(view_->GetVisible());
  view_.reset();

  // The histrogram should not be recorded when the device selector is not
  // available.
  histogram_tester_.ExpectTotalCount(kDeviceSelectorOpenedHistogramName, 0);

  delegate.supports_switching = true;
  view_ = CreateDeviceSelectorView(&delegate);
  view_.reset();

  histogram_tester_.ExpectTotalCount(kDeviceSelectorOpenedHistogramName, 1);
  histogram_tester_.ExpectBucketCount(kDeviceSelectorOpenedHistogramName, false,
                                      1);

  view_ = CreateDeviceSelectorView(&delegate);
  view_->ShowDevices();

  histogram_tester_.ExpectTotalCount(kDeviceSelectorOpenedHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kDeviceSelectorOpenedHistogramName, true,
                                      1);

  view_.reset();

  histogram_tester_.ExpectTotalCount(kDeviceSelectorOpenedHistogramName, 2);
}
