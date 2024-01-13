// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/mic_preview/mic_coordinator.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/system/system_monitor.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "content/public/browser/audio_service.h"
#include "media/audio/audio_device_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;

namespace {

constexpr char kDeviceId[] = "device_id";
constexpr char kDeviceName[] = "device_name";
constexpr char kGroupId[] = "group_id";
constexpr char kDeviceId2[] = "device_id_2";
constexpr char kDeviceName2[] = "device_name_2";
constexpr char kGroupId2[] = "group_id2";

MATCHER_P(HasItems, items, "") {
  if (arg.GetItemCount() != items.size()) {
    *result_listener << "item count is " << arg.GetItemCount();
    return false;
  }

  for (size_t i = 0; i < items.size(); ++i) {
    if (base::UTF8ToUTF16(items[i]) != arg.GetItemAt(i)) {
      *result_listener << "item at index " << i << " is " << arg.GetItemAt(i);
      return false;
    }
  }

  return true;
}

}  // namespace

class MicCoordinatorTest : public TestWithBrowserView {
 protected:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    auto_reset_audio_service_.emplace(
        content::OverrideAudioServiceForTesting(&fake_audio_service_));
    fake_audio_service_.SetOnGetInputStreamParametersCallback(
        on_input_stream_id_future_.GetRepeatingCallback());
    fake_audio_service_.SetBindStreamFactoryCallback(
        on_bind_stream_factory_future_.GetRepeatingCallback());

    fake_audio_service_.SetOnRepliedWithInputDeviceDescriptionsCallback(
        replied_with_device_descriptions_future_.GetCallback());

    parent_view_.emplace();
    InitializeCoordinator({});
    ASSERT_TRUE(replied_with_device_descriptions_future_.WaitAndClear());
  }

  void TearDown() override {
    coordinator_.reset();
    TestWithBrowserView::TearDown();
  }

  void InitializeCoordinator(std::vector<std::string> eligible_mic_ids) {
    coordinator_.emplace(*parent_view_,
                         /*needs_borders=*/true,
                         /*eligible_mic_ids=*/eligible_mic_ids);
  }

  const ui::SimpleComboboxModel& GetComboboxModel() const {
    return coordinator_->GetComboboxModelForTest();
  }

  void VerifyEmptyCombobox() const {
    // Our combobox model size will always be >= 1. If no mics are connected,
    // a message is shown to the user to connect a mic.
    // Verify that there is precisely one item in the combobox model.
    EXPECT_EQ(GetComboboxModel().GetItemCount(), 1u);
    EXPECT_EQ(
        GetComboboxModel().GetItemAt(/*index=*/0),
        l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_MICS_FOUND_COMBOBOX));
  }

  bool AddFakeInputDevice(const media::AudioDeviceDescription& descriptor) {
    replied_with_device_descriptions_future_.Clear();
    fake_audio_service_.SetOnRepliedWithInputDeviceDescriptionsCallback(
        replied_with_device_descriptions_future_.GetCallback());
    fake_audio_service_.AddFakeInputDevice(descriptor);
    return replied_with_device_descriptions_future_.WaitAndClear();
  }

  bool RemoveFakeInputDevice(const std::string& device_id) {
    replied_with_device_descriptions_future_.Clear();
    fake_audio_service_.SetOnRepliedWithInputDeviceDescriptionsCallback(
        replied_with_device_descriptions_future_.GetCallback());
    fake_audio_service_.RemoveFakeInputDevice(device_id);
    return replied_with_device_descriptions_future_.WaitAndClear();
  }

  base::SystemMonitor monitor_;
  media_effects::FakeAudioService fake_audio_service_;
  std::optional<base::AutoReset<audio::mojom::AudioService*>>
      auto_reset_audio_service_;

  base::test::TestFuture<const std::string&> on_input_stream_id_future_;
  base::test::TestFuture<void> on_bind_stream_factory_future_;
  base::test::TestFuture<void> replied_with_device_descriptions_future_;

  std::optional<views::View> parent_view_;
  std::optional<MicCoordinator> coordinator_;
};

TEST_F(MicCoordinatorTest, RelevantAudioInputDeviceInfoExtraction) {
  VerifyEmptyCombobox();

  // Add first mic, and connect to it.
  // Mic connection is done automatically to the device at combobox's default
  // index (i.e. 0).
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName, kDeviceId, kGroupId}));
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId);
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName}));

  // Add second mic and connection to the first is not affected.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName2, kDeviceId2, kGroupId2}));
  EXPECT_FALSE(on_input_stream_id_future_.IsReady());
  EXPECT_THAT(GetComboboxModel(),
              HasItems(std::vector{kDeviceName, kDeviceName2}));

  // Remove first mic, and connect to the second one.
  ASSERT_TRUE(RemoveFakeInputDevice(kDeviceId));
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId2);
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName2}));

  // Re-add first mic and connect to it.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName, kDeviceId, kGroupId}));
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId);
  EXPECT_THAT(GetComboboxModel(),
              HasItems(std::vector{kDeviceName, kDeviceName2}));

  // Remove second mic, and connection to the first is not affected.
  ASSERT_TRUE(RemoveFakeInputDevice(kDeviceId2));
  EXPECT_FALSE(on_input_stream_id_future_.IsReady());
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName}));

  // Remove first mic.
  ASSERT_TRUE(RemoveFakeInputDevice(kDeviceId));
  VerifyEmptyCombobox();
}

TEST_F(MicCoordinatorTest,
       RelevantAudioInputDeviceInfoExtraction_ConstrainedToEligibleDevices) {
  InitializeCoordinator({kDeviceId2});
  VerifyEmptyCombobox();

  // Add first mic. It won't be added to the combobox because it's not in the
  // eligible list.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName, kDeviceId, kGroupId}));
  EXPECT_FALSE(on_input_stream_id_future_.IsReady());
  VerifyEmptyCombobox();

  // Add second mic and connect to it since it's in the eligible list.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName2, kDeviceId2, kGroupId2}));
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId2);
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName2}));

  // Remove first mic, nothing changes since it wasn't in the combobox.
  ASSERT_TRUE(RemoveFakeInputDevice(kDeviceId));
  EXPECT_FALSE(on_input_stream_id_future_.IsReady());
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName2}));

  // Remove second mic.
  ASSERT_TRUE(RemoveFakeInputDevice(kDeviceId2));
  VerifyEmptyCombobox();
}

TEST_F(MicCoordinatorTest, ConnectToDifferentDevice) {
  VerifyEmptyCombobox();

  // Add first mic, and connect to it.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName, kDeviceId, kGroupId}));
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId);

  // Add second mic and connection to the first is not affected.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName2, kDeviceId2, kGroupId2}));
  EXPECT_FALSE(on_input_stream_id_future_.IsReady());

  //  Connect to the second mic.
  coordinator_->OnAudioSourceChanged(/*selected_index=*/1);
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId2);
}

TEST_F(MicCoordinatorTest, TryConnectToSameDevice) {
  VerifyEmptyCombobox();

  // Add mic, and connect to it.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName, kDeviceId, kGroupId}));
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId);

  //  Try connect to the mic.
  // Nothing is expected because we are already connected.
  coordinator_->OnAudioSourceChanged(/*selected_index=*/0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(on_input_stream_id_future_.IsReady());

  // Remove mic.
  ASSERT_TRUE(RemoveFakeInputDevice(kDeviceId));
  VerifyEmptyCombobox();

  // Add mic, and connect to it again.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName, kDeviceId, kGroupId}));
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId);
}

TEST_F(MicCoordinatorTest, DefaultMicHandling) {
  VerifyEmptyCombobox();

  // Add two mics, one that's the default and one that isn't.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName, kDeviceId, kGroupId}));
  ASSERT_TRUE(AddFakeInputDevice(
      {kDeviceName2, kDeviceId2, kGroupId2, /*is_system_default=*/true}));
  EXPECT_THAT(GetComboboxModel(),
              HasItems(std::vector{kDeviceName, kDeviceName2}));
  EXPECT_EQ(GetComboboxModel().GetDropDownSecondaryTextAt(/*index=*/0),
            std::u16string{});
  EXPECT_EQ(GetComboboxModel().GetDropDownSecondaryTextAt(/*index=*/1),
            l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_SYSTEM_DEFAULT_MIC));
}
