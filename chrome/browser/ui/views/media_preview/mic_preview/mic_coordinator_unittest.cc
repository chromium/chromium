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
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "content/public/browser/audio_service.h"
#include "media/audio/audio_device_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;
using testing::ElementsAre;

namespace {

constexpr char kDeviceId[] = "device_id";
constexpr char kDeviceName[] = "device_name";
constexpr char kGroupId[] = "group_id";
constexpr char kDeviceId2[] = "device_id_2";
constexpr char kDeviceName2[] = "device_name_2";
constexpr char kGroupId2[] = "group_id2";

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
    CHECK(profile()->GetPrefs());
    coordinator_.emplace(
        *parent_view_,
        /*needs_borders=*/true, eligible_mic_ids, *profile()->GetPrefs(),
        /*allow_device_selection=*/true,
        media_preview_metrics::Context(
            media_preview_metrics::UiLocation::kPermissionPrompt));
  }

  const ui::SimpleComboboxModel& GetComboboxModel() const {
    return coordinator_->GetComboboxModelForTest();
  }

  std::vector<std::string> GetComboboxItems() const {
    std::vector<std::string> items;
    for (size_t i = 0; i < GetComboboxModel().GetItemCount(); ++i) {
      items.emplace_back(base::UTF16ToUTF8(GetComboboxModel().GetItemAt(i)));
    }
    return items;
  }

  std::vector<std::string> GetComboboxSecondaryTexts() const {
    std::vector<std::string> secondary_texts;
    for (size_t i = 0; i < GetComboboxModel().GetItemCount(); ++i) {
      secondary_texts.emplace_back(
          base::UTF16ToUTF8(GetComboboxModel().GetDropDownSecondaryTextAt(i)));
    }
    return secondary_texts;
  }

  void VerifyEmptyCombobox() const {
    // Our combobox model size will always be >= 1.
    // Verify that there is precisely one item in the combobox model.
    EXPECT_EQ(GetComboboxModel().GetItemCount(), 1u);
    EXPECT_EQ(GetComboboxModel().GetItemAt(/*index=*/0), std::u16string());
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
  EXPECT_THAT(GetComboboxItems(), ElementsAre(kDeviceName));

  // Add second mic and connection to the first is not affected.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName2, kDeviceId2, kGroupId2}));
  EXPECT_FALSE(on_input_stream_id_future_.IsReady());
  EXPECT_THAT(GetComboboxItems(), ElementsAre(kDeviceName, kDeviceName2));

  // Remove first mic, and connect to the second one.
  ASSERT_TRUE(RemoveFakeInputDevice(kDeviceId));
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId2);
  EXPECT_THAT(GetComboboxItems(), ElementsAre(kDeviceName2));

  // Re-add first mic and connect to it.
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName, kDeviceId, kGroupId}));
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId);
  EXPECT_THAT(GetComboboxItems(), ElementsAre(kDeviceName, kDeviceName2));

  // Remove second mic, and connection to the first is not affected.
  ASSERT_TRUE(RemoveFakeInputDevice(kDeviceId2));
  EXPECT_FALSE(on_input_stream_id_future_.IsReady());
  EXPECT_THAT(GetComboboxItems(), ElementsAre(kDeviceName));

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
  EXPECT_THAT(GetComboboxItems(), ElementsAre(kDeviceName2));

  // Remove first mic, nothing changes since it wasn't in the combobox.
  ASSERT_TRUE(RemoveFakeInputDevice(kDeviceId));
  EXPECT_FALSE(on_input_stream_id_future_.IsReady());
  EXPECT_THAT(GetComboboxItems(), ElementsAre(kDeviceName2));

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

  const auto kDefaultDeviceName =
      l10n_util::GetStringUTF8(IDS_MEDIA_PREVIEW_SYSTEM_DEFAULT_MIC);

  // Add 2 mics. The virtual system default, and one other.
  ASSERT_TRUE(AddFakeInputDevice(
      {/*device_name=*/"default_name",
       media::AudioDeviceDescription::kDefaultDeviceId, "default_group_id"}));
  ASSERT_TRUE(AddFakeInputDevice({kDeviceName, kDeviceId, kGroupId}));
  // The virtual default device should be included because there's no mapping
  // for the real system default device.
  EXPECT_THAT(GetComboboxItems(), ElementsAre(kDefaultDeviceName, kDeviceName));
  EXPECT_THAT(GetComboboxSecondaryTexts(),
              ElementsAre(std::string{}, std::string{}));
  on_input_stream_id_future_.Clear();

  // Add another mic marked with `is_system_default`.
  ASSERT_TRUE(AddFakeInputDevice(
      {kDeviceName2, kDeviceId2, kGroupId2, /*is_system_default=*/true}));
  // The virtual default device should be excluded because the real system
  // default was found in the list.
  EXPECT_THAT(GetComboboxItems(), ElementsAre(kDeviceName, kDeviceName2));
  // The system default device should have the secondary text.
  EXPECT_THAT(GetComboboxSecondaryTexts(),
              ElementsAre(std::string{}, kDefaultDeviceName));
}

TEST_F(MicCoordinatorTest, UpdateDevicePreferenceRanking) {
  VerifyEmptyCombobox();
  const media::AudioDeviceDescription kDevice1{kDeviceName, kDeviceId,
                                               kGroupId};
  const media::AudioDeviceDescription kDevice2{kDeviceName2, kDeviceId2,
                                               kGroupId2};

  // Add first mic, and connect to it.
  ASSERT_TRUE(AddFakeInputDevice(kDevice1));
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId);

  // Add second mic and connection to the first is not affected.
  ASSERT_TRUE(AddFakeInputDevice(kDevice2));
  on_input_stream_id_future_.Clear();

  //  Connect to the mic 2.
  coordinator_->OnAudioSourceChanged(/*selected_index=*/1);
  ASSERT_TRUE(on_bind_stream_factory_future_.WaitAndClear());
  EXPECT_EQ(on_input_stream_id_future_.Take(), kDeviceId2);

  // Preference ranking defaults to noop.
  std::vector device_infos{kDevice1, kDevice2};
  media_prefs::PreferenceRankAudioDeviceInfos(*profile()->GetPrefs(),
                                              device_infos);
  EXPECT_THAT(device_infos, ElementsAre(kDevice1, kDevice2));

  // Ranking is updated to make the currently selected device most preferred.
  coordinator_->UpdateDevicePreferenceRanking();
  media_prefs::PreferenceRankAudioDeviceInfos(*profile()->GetPrefs(),
                                              device_infos);
  EXPECT_THAT(device_infos, ElementsAre(kDevice2, kDevice1));
}
