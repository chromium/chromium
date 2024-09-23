// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/hid_detection/hid_detection_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/hid_detection/bluetooth_hid_detector.h"
#include "chromeos/ash/components/hid_detection/fake_bluetooth_hid_detector.h"
#include "chromeos/ash/components/hid_detection/hid_detection_utils.h"
#include "services/device/public/cpp/hid/fake_input_service_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::hid_detection {

namespace {

using BluetoothHidMetadata = BluetoothHidDetector::BluetoothHidMetadata;
using BluetoothHidType = BluetoothHidDetector::BluetoothHidType;
using InputMetadata = HidDetectionManager::InputMetadata;
using InputState = HidDetectionManager::InputState;
using InputDeviceType = device::mojom::InputDeviceType;
using InputDevicesStatus = BluetoothHidDetector::InputDevicesStatus;

const char kTestHidName[] = "testName";
const char kTestPinCode[] = "123456";

enum TestHidType {
  kMouse,
  kTouchpad,
  kKeyboard,
  kTouchscreen,
  kTablet,
};

class FakeHidDetectionManagerDelegate : public HidDetectionManager::Delegate {
 public:
  ~FakeHidDetectionManagerDelegate() override = default;

  size_t num_hid_detection_status_changed_calls() const {
    return num_hid_detection_status_changed_calls_;
  }

  const std::optional<HidDetectionManager::HidDetectionStatus>&
  last_hid_detection_status() const {
    return last_hid_detection_status_;
  }

 private:
  // HidDetectionManager::Delegate:
  void OnHidDetectionStatusChanged(
      HidDetectionManager::HidDetectionStatus status) override {
    ++num_hid_detection_status_changed_calls_;
    last_hid_detection_status_ = std::move(status);
  }

  size_t num_hid_detection_status_changed_calls_ = 0u;
  std::optional<HidDetectionManager::HidDetectionStatus>
      last_hid_detection_status_;
};

}  // namespace

class HidDetectionManagerImplTest : public testing::Test {
 protected:
  HidDetectionManagerImplTest() = default;
  HidDetectionManagerImplTest(const HidDetectionManagerImplTest&) = delete;
  HidDetectionManagerImplTest& operator=(const HidDetectionManagerImplTest&) =
      delete;
  ~HidDetectionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    auto fake_bluetooth_hid_detector =
        std::make_unique<FakeBluetoothHidDetector>();
    fake_bluetooth_hid_detector_ = fake_bluetooth_hid_detector.get();
    hid_detection_manager_ = std::make_unique<HidDetectionManagerImpl>(
        /*device_service=*/nullptr);
    hid_detection_manager_->SetBluetoothHidDetectorForTest(
        std::move(fake_bluetooth_hid_detector));

    HidDetectionManagerImpl::SetInputDeviceManagerBinderForTest(
        base::BindRepeating(&device::FakeInputServiceLinux::Bind,
                            base::Unretained(&fake_input_service_)));
  }

  void TearDown() override {
    HidDetectionManagerImpl::SetInputDeviceManagerBinderForTest(
        base::NullCallback());
  }

  std::optional<bool> GetIsHidDetectionRequired() {
    std::optional<bool> result;
    hid_detection_manager_->GetIsHidDetectionRequired(
        base::BindLambdaForTesting(
            [&result](bool is_required) { result = is_required; }));
    base::RunLoop().RunUntilIdle();
    return result;
  }

  void StartHidDetection() {
    EXPECT_FALSE(
        fake_bluetooth_hid_detector_->is_bluetooth_hid_detection_active());
    hid_detection_manager_->StartHidDetection(&delegate_);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(
        fake_bluetooth_hid_detector_->is_bluetooth_hid_detection_active());
  }

  void StopHidDetection(bool should_be_using_bluetooth) {
    EXPECT_TRUE(
        fake_bluetooth_hid_detector_->is_bluetooth_hid_detection_active());
    hid_detection_manager_->StopHidDetection();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(
        fake_bluetooth_hid_detector_->is_bluetooth_hid_detection_active());
    EXPECT_EQ(should_be_using_bluetooth,
              fake_bluetooth_hid_detector_->is_using_bluetooth());
  }

  size_t GetNumHidDetectionStatusChangedCalls() {
    return delegate_.num_hid_detection_status_changed_calls();
  }

  const std::optional<HidDetectionManager::HidDetectionStatus>&
  GetLastHidDetectionStatus() {
    return delegate_.last_hid_detection_status();
  }

  void AddDevice(TestHidType hid_type,
                 InputDeviceType device_type,
                 std::string* id_out = nullptr,
                 const char* name = NULL) {
    AddDevice(std::vector{hid_type}, device_type, id_out, name);
  }

  void AddDevice(const std::vector<TestHidType>& hid_types,
                 InputDeviceType device_type,
                 std::string* id_out = nullptr,
                 const char* name = NULL) {
    auto device = device::mojom::InputDeviceInfo::New();
    device->id = num_devices_created_++;
    if (id_out)
      *id_out = device->id;

    device->name = name == NULL ? device->id : name;
    device->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
    device->type = device_type;

    for (const auto& hid_type : hid_types) {
      switch (hid_type) {
        case kMouse:
          device->is_mouse = true;
          break;
        case kTouchpad:
          device->is_touchpad = true;
          break;
        case kKeyboard:
          device->is_keyboard = true;
          break;
        case kTouchscreen:
          device->is_touchscreen = true;
          break;
        case kTablet:
          device->is_tablet = true;
          break;
      }
    }
    fake_input_service_.AddDevice(std::move(device));
    base::RunLoop().RunUntilIdle();
  }

  void RemoveDevice(const std::string& id) {
    fake_input_service_.RemoveDevice(id);
    base::RunLoop().RunUntilIdle();
  }

  void SimulatePairingStarted(
      BluetoothHidDetector::BluetoothHidMetadata pairing_device) {
    fake_bluetooth_hid_detector_->SimulatePairingStarted(
        std::move(pairing_device));
    base::RunLoop().RunUntilIdle();
  }

  void SimulatePairingCodeRequired(
      const BluetoothHidPairingState& pairing_state) {
    fake_bluetooth_hid_detector_->SetPairingState(BluetoothHidPairingState{
        pairing_state.code, pairing_state.num_keys_entered});
    base::RunLoop().RunUntilIdle();
  }

  void SimulatePairingSessionEnded() {
    fake_bluetooth_hid_detector_->SimulatePairingSessionEnded();
    base::RunLoop().RunUntilIdle();
  }

  void AssertHidDetectionStatus(
      InputMetadata pointer_metadata,
      InputMetadata keyboard_metadata,
      bool touchscreen_detected,
      const std::optional<BluetoothHidPairingState>& pairing_state) {
    EXPECT_EQ(pointer_metadata.state,
              GetLastHidDetectionStatus()->pointer_metadata.state);
    EXPECT_EQ(pointer_metadata.detected_hid_name,
              GetLastHidDetectionStatus()->pointer_metadata.detected_hid_name);
    EXPECT_EQ(keyboard_metadata.state,
              GetLastHidDetectionStatus()->keyboard_metadata.state);
    EXPECT_EQ(keyboard_metadata.detected_hid_name,
              GetLastHidDetectionStatus()->keyboard_metadata.detected_hid_name);
    EXPECT_EQ(touchscreen_detected,
              GetLastHidDetectionStatus()->touchscreen_detected);
    EXPECT_EQ(pairing_state.has_value(),
              GetLastHidDetectionStatus()->pairing_state.has_value());

    if (pairing_state.has_value()) {
      EXPECT_EQ(pairing_state->code,
                GetLastHidDetectionStatus()->pairing_state->code);
      EXPECT_EQ(pairing_state->num_keys_entered,
                GetLastHidDetectionStatus()->pairing_state->num_keys_entered);
    }
  }

  void AssertInputDevicesStatus(InputDevicesStatus input_devices_status) {
    EXPECT_EQ(input_devices_status.pointer_is_missing,
              fake_bluetooth_hid_detector_->input_devices_status()
                  .pointer_is_missing);
    EXPECT_EQ(input_devices_status.keyboard_is_missing,
              fake_bluetooth_hid_detector_->input_devices_status()
                  .keyboard_is_missing);
  }

  void AssertInitialHidsMissingCount(HidsMissing hids_missing, int count) {
    histogram_tester_.ExpectBucketCount(
        "OOBE.HidDetectionScreen.InitialHidsMissing", hids_missing, count);
    histogram_tester_.ExpectTotalCount(
        "OOBE.HidDetectionScreen.InitialHidsMissing", count);
  }

  void AssertHidConnectedCount(HidType hid_type, int count, int total_count) {
    histogram_tester_.ExpectBucketCount("OOBE.HidDetectionScreen.HidConnected",
                                        hid_type, count);
    histogram_tester_.ExpectTotalCount("OOBE.HidDetectionScreen.HidConnected",
                                       total_count);
  }

  void AssertHidDisconnectedCount(HidType hid_type,
                                  int count,
                                  int total_count) {
    histogram_tester_.ExpectBucketCount(
        "OOBE.HidDetectionScreen.HidDisconnected", hid_type, count);
    histogram_tester_.ExpectTotalCount(
        "OOBE.HidDetectionScreen.HidDisconnected", total_count);
  }

  size_t GetNumSetInputDevicesStatusCalls() {
    return fake_bluetooth_hid_detector_->num_set_input_devices_status_calls();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

  device::FakeInputServiceLinux fake_input_service_;

  size_t num_devices_created_ = 0;

  FakeHidDetectionManagerDelegate delegate_;
  raw_ptr<FakeBluetoothHidDetector, DanglingUntriaged>
      fake_bluetooth_hid_detector_ = nullptr;

  std::unique_ptr<hid_detection::HidDetectionManagerImpl>
      hid_detection_manager_;
};

TEST_F(HidDetectionManagerImplTest,
       GetIsHidDetectionRequired_NoDevicesConnected) {
  AssertInitialHidsMissingCount(HidsMissing::kPointerAndKeyboard, /*count=*/0);

  std::optional<bool> is_hid_detection_required = GetIsHidDetectionRequired();
  ASSERT_TRUE(is_hid_detection_required.has_value());
  ASSERT_TRUE(is_hid_detection_required.value());
  AssertInitialHidsMissingCount(HidsMissing::kPointerAndKeyboard, /*count=*/1);
}

TEST_F(HidDetectionManagerImplTest,
       GetIsHidDetectionRequired_OnlyPointerConnected) {
  AddDevice(TestHidType::kMouse, InputDeviceType::TYPE_USB);
  AssertInitialHidsMissingCount(HidsMissing::kKeyboard, /*count=*/0);

  std::optional<bool> is_hid_detection_required = GetIsHidDetectionRequired();
  ASSERT_TRUE(is_hid_detection_required.has_value());
  ASSERT_TRUE(is_hid_detection_required.value());
  AssertInitialHidsMissingCount(HidsMissing::kKeyboard, /*count=*/1);
}

TEST_F(HidDetectionManagerImplTest,
       GetIsHidDetectionRequired_OnlyKeyboardConnected) {
  AddDevice(TestHidType::kKeyboard, InputDeviceType::TYPE_USB);
  AssertInitialHidsMissingCount(HidsMissing::kPointer, /*count=*/0);

  std::optional<bool> is_hid_detection_required = GetIsHidDetectionRequired();
  ASSERT_TRUE(is_hid_detection_required.has_value());
  ASSERT_TRUE(is_hid_detection_required.value());
  AssertInitialHidsMissingCount(HidsMissing::kPointer, /*count=*/1);
}

TEST_F(HidDetectionManagerImplTest,
       GetIsHidDetectionRequired_PointerAndKeyboardConnected) {
  AddDevice(TestHidType::kTouchpad, InputDeviceType::TYPE_USB);
  AddDevice(TestHidType::kKeyboard, InputDeviceType::TYPE_USB);
  AssertInitialHidsMissingCount(HidsMissing::kPointer, /*count=*/0);

  std::optional<bool> is_hid_detection_required = GetIsHidDetectionRequired();
  ASSERT_TRUE(is_hid_detection_required.has_value());
  ASSERT_FALSE(is_hid_detection_required.value());
  AssertInitialHidsMissingCount(HidsMissing::kNone, /*count=*/1);
}

TEST_F(HidDetectionManagerImplTest, StartDetection_TouchscreenPreConnected) {
  AddDevice(TestHidType::kTouchscreen, InputDeviceType::TYPE_SERIO);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());
  AssertHidConnectedCount(HidType::kTouchscreen, /*count=*/0,
                          /*total_count=*/0);

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/true,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

TEST_F(HidDetectionManagerImplTest, StartDetection_PointerPreConnected) {
  std::string device_id;
  AddDevice(TestHidType::kMouse, InputDeviceType::TYPE_SERIO, &device_id);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnected, device_id},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

TEST_F(HidDetectionManagerImplTest, StartDetection_KeyboardPreConnected) {
  std::string device_id;
  AddDevice(TestHidType::kKeyboard, InputDeviceType::TYPE_SERIO, &device_id);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kConnected, device_id},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_NonHidConnectedDisconnected) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kTouchscreen, /*count=*/0,
                          /*total_count=*/0);
  AssertHidDisconnectedCount(HidType::kTouchscreen, /*count=*/0,
                             /*total_count=*/0);

  // Add a device without a HID type. This should not inform the delegate
  // and no state changed.
  std::vector<TestHidType> hid_types{};
  std::string device_id;
  AddDevice(hid_types, InputDeviceType::TYPE_USB, &device_id);
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kBluetoothPointer, /*count=*/0,
                          /*total_count=*/0);
  AssertHidDisconnectedCount(HidType::kTouchscreen, /*count=*/0,
                             /*total_count=*/0);

  RemoveDevice(device_id);
  StopHidDetection(/*should_be_using_bluetooth=*/false);
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidDisconnectedCount(HidType::kTouchscreen, /*count=*/0,
                             /*total_count=*/0);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_TouchscreenConnectedDisconnected) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kTouchscreen, /*count=*/0,
                          /*total_count=*/0);

  std::string touchscreen_id1;
  AddDevice(TestHidType::kTouchscreen, InputDeviceType::TYPE_SERIO,
            &touchscreen_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/true,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kTouchscreen, /*count=*/1,
                          /*total_count=*/1);
  AssertHidDisconnectedCount(HidType::kTouchscreen, /*count=*/0,
                             /*total_count=*/0);

  RemoveDevice(touchscreen_id1);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidDisconnectedCount(HidType::kTouchscreen, /*count=*/1,
                             /*total_count=*/1);

  StopHidDetection(/*should_be_using_bluetooth=*/false);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Add another touchscreen device. This should not inform the delegate.
  std::string touchscreen_id2;
  AddDevice(TestHidType::kTouchscreen, InputDeviceType::TYPE_SERIO,
            &touchscreen_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kTouchscreen, /*count=*/1,
                          /*total_count=*/1);

  // Remove the touchscreen device. This should not inform the delegate.
  RemoveDevice(touchscreen_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kTouchscreen, /*count=*/1,
                          /*total_count=*/1);
  AssertHidDisconnectedCount(HidType::kTouchscreen, /*count=*/1,
                             /*total_count=*/1);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_PointerConnectedDisconnected) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kBluetoothPointer, /*count=*/0,
                          /*total_count=*/0);

  std::string pointer_id1;
  AddDevice(TestHidType::kMouse, InputDeviceType::TYPE_BLUETOOTH, &pointer_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairedViaBluetooth, pointer_id1},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kBluetoothPointer, /*count=*/1,
                          /*total_count=*/1);
  AssertHidDisconnectedCount(HidType::kBluetoothPointer, /*count=*/0,
                             /*total_count=*/0);

  RemoveDevice(pointer_id1);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidDisconnectedCount(HidType::kBluetoothPointer, /*count=*/1,
                             /*total_count=*/1);

  StopHidDetection(/*should_be_using_bluetooth=*/false);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Add another pointer device. This should not inform the delegate.
  std::string pointer_id2;
  AddDevice(TestHidType::kMouse, InputDeviceType::TYPE_USB, &pointer_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kBluetoothPointer, /*count=*/1,
                          /*total_count=*/1);

  // Remove the pointer device. This should not inform the delegate.
  RemoveDevice(pointer_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidDisconnectedCount(HidType::kBluetoothPointer, /*count=*/1,
                             /*total_count=*/1);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_KeyboardConnectedDisconnected) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kUsbKeyboard, /*count=*/0,
                          /*total_count=*/0);

  std::string keyboard_id1;
  AddDevice(TestHidType::kKeyboard, InputDeviceType::TYPE_USB, &keyboard_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kConnectedViaUsb, keyboard_id1},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});
  AssertHidConnectedCount(HidType::kUsbKeyboard, /*count=*/1,
                          /*total_count=*/1);
  AssertHidDisconnectedCount(HidType::kUsbKeyboard, /*count=*/0,
                             /*total_count=*/0);

  RemoveDevice(keyboard_id1);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidDisconnectedCount(HidType::kUsbKeyboard, /*count=*/1,
                             /*total_count=*/1);

  StopHidDetection(/*should_be_using_bluetooth=*/false);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Add another keyboard device. This should not inform the delegate.
  std::string keyboard_id2;
  AddDevice(TestHidType::kMouse, InputDeviceType::TYPE_USB, &keyboard_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kUsbKeyboard, /*count=*/1,
                          /*total_count=*/1);

  // Remove the keyboard device. This should not inform the delegate.
  RemoveDevice(keyboard_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidDisconnectedCount(HidType::kUsbKeyboard, /*count=*/1,
                             /*total_count=*/1);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_MultipleTouchscreensDisconnected) {
  std::string device_id1;
  AddDevice(TestHidType::kTablet, InputDeviceType::TYPE_SERIO, &device_id1);
  std::string device_id2;
  AddDevice(TestHidType::kTouchscreen, InputDeviceType::TYPE_SERIO,
            &device_id2);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/true,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidDisconnectedCount(HidType::kTouchscreen, /*count=*/0,
                             /*total_count=*/0);

  // Remove the first touchscreen device. The second touchscreen should be
  // detected and delegate notified.
  RemoveDevice(device_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/true,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidDisconnectedCount(HidType::kTouchscreen, /*count=*/1,
                             /*total_count=*/1);

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_MultiplePointersDisconnected) {
  std::string device_id1;
  AddDevice(TestHidType::kTouchpad, InputDeviceType::TYPE_UNKNOWN, &device_id1);
  std::string device_id2;
  AddDevice(TestHidType::kMouse, InputDeviceType::TYPE_BLUETOOTH, &device_id2);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnected, device_id1},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});
  AssertHidDisconnectedCount(HidType::kUnknownPointer, /*count=*/0,
                             /*total_count=*/0);

  // Remove the first pointer. The second pointer should be detected and
  // delegate notified.
  RemoveDevice(device_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairedViaBluetooth, device_id2},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});
  AssertHidDisconnectedCount(HidType::kUnknownPointer, /*count=*/1,
                             /*total_count=*/1);

  StopHidDetection(/*should_be_using_bluetooth=*/true);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_MultipleKeyboardsDisconnected) {
  std::string device_id1;
  AddDevice(TestHidType::kKeyboard, InputDeviceType::TYPE_BLUETOOTH,
            &device_id1);
  std::string device_id2;
  AddDevice(TestHidType::kKeyboard, InputDeviceType::TYPE_SERIO, &device_id2);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, device_id1},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});
  AssertHidDisconnectedCount(HidType::kBluetoothKeyboard, /*count=*/0,
                             /*total_count=*/0);

  // Remove the first keyboard. The second keyboard should be detected and
  // delegate notified.
  RemoveDevice(device_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kConnected, device_id2},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});
  AssertHidDisconnectedCount(HidType::kBluetoothKeyboard, /*count=*/1,
                             /*total_count=*/1);

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_DeviceMultipleHidTypesDisconnected) {
  std::string device_id1;
  AddDevice(TestHidType::kTouchpad, InputDeviceType::TYPE_USB, &device_id1);

  std::string device_id2;
  std::vector<TestHidType> hid_types{TestHidType::kKeyboard,
                                     TestHidType::kTouchpad};
  AddDevice(hid_types, InputDeviceType::TYPE_SERIO, &device_id2);

  std::string device_id3;
  AddDevice(TestHidType::kKeyboard, InputDeviceType::TYPE_UNKNOWN, &device_id3);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/{InputState::kConnected, device_id2},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});
  AssertHidDisconnectedCount(HidType::kUsbPointer, /*count=*/0,
                             /*total_count=*/0);

  RemoveDevice(device_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnected, device_id2},
      /*keyboard_metadata=*/{InputState::kConnected, device_id2},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});
  AssertHidDisconnectedCount(HidType::kUsbPointer, /*count=*/1,
                             /*total_count=*/1);
  AssertHidDisconnectedCount(HidType::kSerialPointer, /*count=*/0,
                             /*total_count=*/1);

  RemoveDevice(device_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kConnected, device_id3},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});
  AssertHidDisconnectedCount(HidType::kUsbPointer, /*count=*/1,
                             /*total_count=*/2);
  AssertHidDisconnectedCount(HidType::kSerialPointer, /*count=*/1,
                             /*total_count=*/2);

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

// TODO(gordonseto): Test add device for type already connected, remove device
// for type already connected.

TEST_F(HidDetectionManagerImplTest, StartDetection_BluetoothPointerSuccess) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kBluetoothPointer, /*count=*/0,
                          /*total_count=*/0);

  SimulatePairingStarted(
      BluetoothHidMetadata{kTestHidName, BluetoothHidType::kPointer});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing succeeding.
  AddDevice(TestHidType::kMouse, InputDeviceType::TYPE_BLUETOOTH,
            /*id_out=*/nullptr, kTestHidName);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kBluetoothPointer, /*count=*/1,
                          /*total_count=*/1);

  SimulatePairingSessionEnded();
  EXPECT_EQ(4u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});

  StopHidDetection(/*should_be_using_bluetooth=*/true);
}

TEST_F(HidDetectionManagerImplTest, StartDetection_BluetoothPointerFailure) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  SimulatePairingStarted(
      BluetoothHidMetadata{kTestHidName, BluetoothHidType::kPointer});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing failing.
  SimulatePairingSessionEnded();
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

TEST_F(HidDetectionManagerImplTest, StartDetection_BluetoothKeyboardSuccess) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kBluetoothKeyboard, /*count=*/0,
                          /*total_count=*/0);

  SimulatePairingStarted(
      BluetoothHidMetadata{kTestHidName, BluetoothHidType::kKeyboard});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching, /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  std::optional<BluetoothHidPairingState> pairing_state =
      BluetoothHidPairingState{kTestPinCode, /*num_keys_entered=*/6};
  SimulatePairingCodeRequired(pairing_state.value());
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching, /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false, pairing_state);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing succeeding.
  AddDevice(TestHidType::kKeyboard, InputDeviceType::TYPE_BLUETOOTH,
            /*id_out=*/nullptr, kTestHidName);
  AssertHidConnectedCount(HidType::kBluetoothKeyboard, /*count=*/1,
                          /*total_count=*/1);

  EXPECT_EQ(4u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching, /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false, pairing_state);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});

  SimulatePairingSessionEnded();
  EXPECT_EQ(5u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching, /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});

  StopHidDetection(/*should_be_using_bluetooth=*/true);
}

TEST_F(HidDetectionManagerImplTest, StartDetection_BluetoothKeyboardFailure) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  SimulatePairingStarted(
      BluetoothHidMetadata{kTestHidName, BluetoothHidType::kKeyboard});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching, /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  std::optional<BluetoothHidPairingState> pairing_state =
      BluetoothHidPairingState{kTestPinCode, /*num_keys_entered=*/6};
  SimulatePairingCodeRequired(pairing_state.value());
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching, /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false, pairing_state);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing failing.
  SimulatePairingSessionEnded();
  EXPECT_EQ(4u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_BluetoothKeyboardPointerComboSuccess) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  SimulatePairingStarted(BluetoothHidMetadata{
      kTestHidName, BluetoothHidType::kKeyboardPointerCombo});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing succeeding.
  AddDevice(std::vector{TestHidType::kKeyboard, TestHidType::kTouchpad},
            InputDeviceType::TYPE_BLUETOOTH,
            /*id_out=*/nullptr, kTestHidName);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});

  SimulatePairingSessionEnded();
  EXPECT_EQ(4u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});

  StopHidDetection(/*should_be_using_bluetooth=*/true);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_BluetoothKeyboardComboFailure) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  SimulatePairingStarted(BluetoothHidMetadata{
      kTestHidName, BluetoothHidType::kKeyboardPointerCombo});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing failing.
  SimulatePairingSessionEnded();
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_BluetoothKeyboardPointerComboPointerPreConnected) {
  std::string device_id1;
  AddDevice(TestHidType::kTouchpad, InputDeviceType::TYPE_USB, &device_id1);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});

  SimulatePairingStarted(BluetoothHidMetadata{
      kTestHidName, BluetoothHidType::kKeyboardPointerCombo});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});

  // Simulate the pairing succeeding.
  std::string device_id2;
  AddDevice(std::vector{TestHidType::kKeyboard, TestHidType::kTouchpad},
            InputDeviceType::TYPE_BLUETOOTH, &device_id2, kTestHidName);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});

  SimulatePairingSessionEnded();
  EXPECT_EQ(4u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});

  // Disconnect the Bluetooth device.
  RemoveDevice(device_id2);
  EXPECT_EQ(5u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

TEST_F(HidDetectionManagerImplTest, StartDetection_VirtialMouseConnected) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kBluetoothPointer, /*count=*/0,
                          /*total_count=*/0);

  std::string pointer_id1;
  AddDevice(TestHidType::kMouse, InputDeviceType::TYPE_BLUETOOTH, &pointer_id1,
            "VIRTUAL_SUSPEND_UHID");
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false,
      /*pairing_state=*/std::nullopt);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
  AssertHidConnectedCount(HidType::kBluetoothPointer, /*count=*/0,
                          /*total_count=*/0);

  StopHidDetection(/*should_be_using_bluetooth=*/false);
}

}  // namespace ash::hid_detection
