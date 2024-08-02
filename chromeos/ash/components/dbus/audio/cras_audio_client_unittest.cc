// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/audio/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ash {

namespace {

// Audio nodes for GetNodes unit test.
const uint64_t kInternalSpeakerId = 10001;
const uint64_t kInternalMicId = 20001;
const uint64_t kNbsMicId = 30001;

const uint32_t kInputMaxSupportedChannels = 1;
const uint32_t kOutputMaxSupportedChannels = 2;

const uint32_t kInputAudioEffect = cras::EFFECT_TYPE_NOISE_CANCELLATION;
const uint32_t kNbsMicAudioEffect = cras::EFFECT_TYPE_HFP_MIC_SR;
const uint32_t kOutputAudioEffect = 0;

const int32_t kInputNumberOfVolumeSteps = 0;
const int32_t kOutputNumberOfVolumeSteps = 25;

const AudioNode kInternalSpeaker(false,
                                 kInternalSpeakerId,
                                 false /* has_v2_stable_device_id */,
                                 kInternalSpeakerId /* stable_device_id_v1 */,
                                 0 /* stable_device_id_v2 */,
                                 "Fake Speaker",
                                 "INTERNAL_SPEAKER",
                                 "Speaker",
                                 false,
                                 0,
                                 kOutputMaxSupportedChannels,
                                 kOutputAudioEffect,
                                 kOutputNumberOfVolumeSteps);

const AudioNode kInternalMic(true,
                             kInternalMicId,
                             false /* has_v2_stable_device_id */,
                             kInternalMicId /* stable_device_id_v1*/,
                             0 /* stable_device_id_v2 */,
                             "Fake Mic",
                             "INTERNAL_MIC",
                             "Internal Mic",
                             false,
                             0,
                             kInputMaxSupportedChannels,
                             kInputAudioEffect,
                             kInputNumberOfVolumeSteps);

const AudioNode kInternalSpeakerV2(
    false,
    kInternalSpeakerId,
    true /* has_v2_stable_device_id */,
    kInternalSpeakerId /* stable_device_id_v1 */,
    // stable_device_id_v2: XOR to make sure the
    // ID is different from |stable_device_id_v1|.
    kInternalSpeakerId ^ 0xFF,
    "Fake Speaker",
    "INTERNAL_SPEAKER",
    "Speaker",
    false,
    0,
    kOutputMaxSupportedChannels,
    kOutputAudioEffect,
    kOutputNumberOfVolumeSteps);

const AudioNode kInternalMicV2(true,
                               kInternalMicId,
                               true /* has_v2_stable_device_id */,
                               kInternalMicId /* stable_device_id_v1 */,
                               // XOR to make sure the ID is different from
                               // |stable_device_id_v1|.
                               kInternalMicId ^ 0xFF /* stable_device_id_v2 */,
                               "Fake Mic",
                               "INTERNAL_MIC",
                               "Internal Mic",
                               false,
                               0,
                               kInputMaxSupportedChannels,
                               kInputAudioEffect,
                               kInputNumberOfVolumeSteps);

const AudioNode kNbsMic(true,
                        kNbsMicId,
                        false /* has_v2_stable_device_id */,
                        kNbsMicId /* stable_device_id_v1*/,
                        0 /* stable_device_id_v2 */,
                        "Fake Nbs Mic",
                        "BLUETOOTH_NB_MIC",
                        "Bluetooth Nb Mic",
                        false,
                        0,
                        kInputMaxSupportedChannels,
                        kNbsMicAudioEffect,
                        kInputNumberOfVolumeSteps);

// A mock CrasAudioClient Observer.
class MockObserver : public CrasAudioClient::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;
  MOCK_METHOD1(OutputMuteChanged, void(bool mute_on));
  MOCK_METHOD1(InputMuteChanged, void(bool mute_on));
  MOCK_METHOD0(NodesChanged, void());
  MOCK_METHOD1(ActiveOutputNodeChanged, void(uint64_t node_id));
  MOCK_METHOD1(ActiveInputNodeChanged, void(uint64_t node_id));
  MOCK_METHOD2(OutputNodeVolumeChanged, void(uint64_t node_id, int volume));
  MOCK_METHOD2(InputNodeGainChanged, void(uint64_t node_id, int gain));
  MOCK_METHOD2(HotwordTriggered, void(uint64_t tv_sec, uint64_t tv_nsec));
  MOCK_METHOD0(NumberOfActiveStreamsChanged, void());
  MOCK_METHOD1(
      NumberOfInputStreamsWithPermissionChanged,
      void(const base::flat_map<std::string, uint32_t>& num_input_streams));
  MOCK_METHOD2(BluetoothBatteryChanged,
               void(const std::string& address, uint32_t level));
  MOCK_METHOD1(SurveyTriggered,
               void(const base::flat_map<std::string, std::string>&
                        survey_specific_data));
  MOCK_METHOD0(SpeakOnMuteDetected, void());
  MOCK_METHOD1(EwmaPowerReported, void(double power));
  MOCK_METHOD0(NumberOfNonChromeOutputStreamsChanged, void());
  MOCK_METHOD1(NumStreamIgnoreUiGains, void(int32_t num));
  MOCK_METHOD0(NumberOfArcStreamsChanged, void());
  MOCK_METHOD1(SidetoneSupportedChanged, void(bool supported));
};

// Expect the reader to be empty.
void ExpectNoArgument(dbus::MessageReader* reader) {
  EXPECT_FALSE(reader->HasMoreData());
}

// Expect the reader to have an int32_t and an array of doubles.
void ExpectInt32AndArrayOfDoublesArguments(
    int32_t expected_value,
    const std::vector<double>& expected_doubles,
    dbus::MessageReader* reader) {
  int32_t value;
  ASSERT_TRUE(reader->PopInt32(&value));
  EXPECT_EQ(expected_value, value);
  const double* doubles = nullptr;
  size_t size = 0;
  ASSERT_TRUE(reader->PopArrayOfDoubles(&doubles, &size));
  EXPECT_EQ(expected_doubles.size(), size);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(expected_doubles[i], doubles[i]);
  }
  EXPECT_FALSE(reader->HasMoreData());
}

// Expect the reader to have an uint64_t and an int32_t.
void ExpectUint64AndInt32Arguments(uint64_t expected_uint64,
                                   int32_t expected_int32,
                                   dbus::MessageReader* reader) {
  uint64_t value1;
  ASSERT_TRUE(reader->PopUint64(&value1));
  EXPECT_EQ(expected_uint64, value1);
  int32_t value2;
  ASSERT_TRUE(reader->PopInt32(&value2));
  EXPECT_EQ(expected_int32, value2);
  EXPECT_FALSE(reader->HasMoreData());
}

// Expect the reader to have an uint64_t.
void ExpectUint64Argument(uint64_t expected_value,
                          dbus::MessageReader* reader) {
  uint64_t value;
  ASSERT_TRUE(reader->PopUint64(&value));
  EXPECT_EQ(expected_value, value);
  EXPECT_FALSE(reader->HasMoreData());
}

// Expect the reader to have a bool.
void ExpectBoolArgument(bool expected_bool, dbus::MessageReader* reader) {
  bool value;
  ASSERT_TRUE(reader->PopBool(&value));
  EXPECT_EQ(expected_bool, value);
  EXPECT_FALSE(reader->HasMoreData());
}

// Expect the reader to have an uint64_t and a bool.
void ExpectUint64AndBoolArguments(uint64_t expected_uint64,
                                  bool expected_bool,
                                  dbus::MessageReader* reader) {
  uint64_t value1;
  ASSERT_TRUE(reader->PopUint64(&value1));
  EXPECT_EQ(expected_uint64, value1);
  bool value2;
  ASSERT_TRUE(reader->PopBool(&value2));
  EXPECT_EQ(expected_bool, value2);
  EXPECT_FALSE(reader->HasMoreData());
}

// Expect the reader to have a string.
void ExpectStringArgument(std::string expected_string,
                          dbus::MessageReader* reader) {
  std::string value;
  ASSERT_TRUE(reader->PopString(&value));
  EXPECT_EQ(expected_string, value);
  EXPECT_FALSE(reader->HasMoreData());
}

// Expect the reader to have a int64_t.
void ExpectInt64Argument(int64_t expected_int64, dbus::MessageReader* reader) {
  int64_t value;
  ASSERT_TRUE(reader->PopInt64(&value));
  EXPECT_EQ(expected_int64, value);
  EXPECT_FALSE(reader->HasMoreData());
}

// Expect the reader to have an array of dictionary of string and variant
// string.
void ExpectArrayOfDictOfStringAndVariantStringArguments(
    const std::map<std::string, std::string>& expected_dict,
    dbus::MessageReader* reader) {
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader->PopArray(&array_reader));

  dbus::MessageReader dict_entry_reader(nullptr);
  for (auto& entry : expected_dict) {
    EXPECT_TRUE(array_reader.HasMoreData());
    ASSERT_TRUE(array_reader.PopDictEntry(&dict_entry_reader));

    std::string key, value;
    ASSERT_TRUE(dict_entry_reader.PopString(&key));
    EXPECT_EQ(entry.first, key);
    ASSERT_TRUE(dict_entry_reader.PopVariantOfString(&value));
    EXPECT_EQ(entry.second, value);
  }
  EXPECT_FALSE(reader->HasMoreData());
}

// Expect the reader to have an array of dictionary of string and variant
// int64.
void ExpectArrayOfDictOfStringAndVariantInt64Arguments(
    const std::map<std::string, int64_t>& expected_dict,
    dbus::MessageReader* reader) {
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader->PopArray(&array_reader));

  dbus::MessageReader dict_entry_reader(nullptr);
  for (auto& entry : expected_dict) {
    EXPECT_TRUE(array_reader.HasMoreData());
    ASSERT_TRUE(array_reader.PopDictEntry(&dict_entry_reader));

    std::string key;
    int64_t value;
    ASSERT_TRUE(dict_entry_reader.PopString(&key));
    EXPECT_EQ(entry.first, key);
    ASSERT_TRUE(dict_entry_reader.PopVariantOfInt64(&value));
    EXPECT_EQ(entry.second, value);
  }
  EXPECT_FALSE(reader->HasMoreData());
}

void WriteNodesToResponse(const AudioNodeList& node_list,
                          dbus::MessageWriter* writer) {
  dbus::MessageWriter sub_writer(nullptr);
  dbus::MessageWriter entry_writer(nullptr);
  for (size_t i = 0; i < node_list.size(); ++i) {
    writer->OpenArray("{sv}", &sub_writer);
    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kIsInputProperty);
    entry_writer.AppendVariantOfBool(node_list[i].is_input);
    sub_writer.CloseContainer(&entry_writer);

    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kIdProperty);
    entry_writer.AppendVariantOfUint64(node_list[i].id);
    sub_writer.CloseContainer(&entry_writer);

    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kDeviceNameProperty);
    entry_writer.AppendVariantOfString(node_list[i].device_name);
    sub_writer.CloseContainer(&entry_writer);

    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kTypeProperty);
    entry_writer.AppendVariantOfString(node_list[i].type);
    sub_writer.CloseContainer(&entry_writer);

    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kNameProperty);
    entry_writer.AppendVariantOfString(node_list[i].name);
    sub_writer.CloseContainer(&entry_writer);

    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kActiveProperty);
    entry_writer.AppendVariantOfBool(node_list[i].active);
    sub_writer.CloseContainer(&entry_writer);

    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kPluggedTimeProperty);
    entry_writer.AppendVariantOfUint64(node_list[i].plugged_time);
    sub_writer.CloseContainer(&entry_writer);

    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kStableDeviceIdProperty);
    entry_writer.AppendVariantOfUint64(node_list[i].stable_device_id_v1);
    sub_writer.CloseContainer(&entry_writer);

    if (node_list[i].has_v2_stable_device_id) {
      sub_writer.OpenDictEntry(&entry_writer);
      entry_writer.AppendString(cras::kStableDeviceIdNewProperty);
      entry_writer.AppendVariantOfUint64(node_list[i].stable_device_id_v2);
      sub_writer.CloseContainer(&entry_writer);
    }

    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kAudioEffectProperty);
    entry_writer.AppendVariantOfUint32(node_list[i].audio_effect);
    sub_writer.CloseContainer(&entry_writer);

    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kNumberOfVolumeStepsProperty);
    entry_writer.AppendVariantOfInt32(node_list[i].number_of_volume_steps);
    sub_writer.CloseContainer(&entry_writer);

    writer->CloseContainer(&sub_writer);
  }
}

// Expect the AudioNodeList result.
void ExpectAudioNodeListResult(bool* called,
                               const AudioNodeList& expected_node_list,
                               std::optional<AudioNodeList> result) {
  *called = true;
  ASSERT_TRUE(result.has_value());
  const AudioNodeList& node_list = result.value();
  ASSERT_EQ(expected_node_list.size(), node_list.size());
  for (size_t i = 0; i < node_list.size(); ++i) {
    EXPECT_EQ(expected_node_list[i].is_input, node_list[i].is_input);
    EXPECT_EQ(expected_node_list[i].id, node_list[i].id);
    EXPECT_EQ(expected_node_list[i].stable_device_id_v1,
              node_list[i].stable_device_id_v1);
    EXPECT_EQ(expected_node_list[i].stable_device_id_v2,
              node_list[i].stable_device_id_v2);
    EXPECT_EQ(expected_node_list[i].device_name, node_list[i].device_name);
    EXPECT_EQ(expected_node_list[i].type, node_list[i].type);
    EXPECT_EQ(expected_node_list[i].name, node_list[i].name);
    EXPECT_EQ(expected_node_list[i].active, node_list[i].active);
    EXPECT_EQ(expected_node_list[i].plugged_time, node_list[i].plugged_time);
    EXPECT_EQ(expected_node_list[i].StableDeviceIdVersion(),
              node_list[i].StableDeviceIdVersion());
    EXPECT_EQ(expected_node_list[i].audio_effect, node_list[i].audio_effect);
    EXPECT_EQ(expected_node_list[i].number_of_volume_steps,
              node_list[i].number_of_volume_steps);
  }
}

}  // namespace

class CrasAudioClientTest : public testing::Test {
 public:
  CrasAudioClientTest()
      : interface_name_(cras::kCrasControlInterface), response_(nullptr) {}

  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock cras proxy.
    mock_cras_proxy_ =
        new dbus::MockObjectProxy(mock_bus_.get(), cras::kCrasServiceName,
                                  dbus::ObjectPath(cras::kCrasServicePath));

    // Set an expectation so mock_cras_proxy's CallMethod() will use
    // OnCallMethod() to return responses.
    EXPECT_CALL(*mock_cras_proxy_.get(), DoCallMethod(_, _, _))
        .WillRepeatedly(Invoke(this, &CrasAudioClientTest::OnCallMethod));

    // Set an expectation so mock_cras_proxy's CallMethodWithErrorCallback()
    // will use OnCallMethodWithErrorCallback() to return responses.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoCallMethodWithErrorCallback(_, _, _, _))
        .WillRepeatedly(
            Invoke(this, &CrasAudioClientTest::OnCallMethodWithErrorCallback));

    // Set an expectation so mock_cras_proxy's monitoring OutputMuteChanged
    // ConnectToSignal will use OnConnectToOutputMuteChanged() to run the
    // callback.
    EXPECT_CALL(
        *mock_cras_proxy_.get(),
        DoConnectToSignal(interface_name_, cras::kOutputMuteChanged, _, _))
        .WillRepeatedly(
            Invoke(this, &CrasAudioClientTest::OnConnectToOutputMuteChanged));

    // Set an expectation so mock_cras_proxy's monitoring InputMuteChanged
    // ConnectToSignal will use OnConnectToInputMuteChanged() to run the
    // callback.
    EXPECT_CALL(
        *mock_cras_proxy_.get(),
        DoConnectToSignal(interface_name_, cras::kInputMuteChanged, _, _))
        .WillRepeatedly(
            Invoke(this, &CrasAudioClientTest::OnConnectToInputMuteChanged));

    // Set an expectation so mock_cras_proxy's monitoring NodesChanged
    // ConnectToSignal will use OnConnectToNodesChanged() to run the callback.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoConnectToSignal(interface_name_, cras::kNodesChanged, _, _))
        .WillRepeatedly(
            Invoke(this, &CrasAudioClientTest::OnConnectToNodesChanged));

    // Set an expectation so mock_cras_proxy's monitoring
    // ActiveOutputNodeChanged ConnectToSignal will use
    // OnConnectToActiveOutputNodeChanged() to run the callback.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoConnectToSignal(interface_name_,
                                  cras::kActiveOutputNodeChanged, _, _))
        .WillRepeatedly(Invoke(
            this, &CrasAudioClientTest::OnConnectToActiveOutputNodeChanged));

    // Set an expectation so mock_cras_proxy's monitoring
    // ActiveInputNodeChanged ConnectToSignal will use
    // OnConnectToActiveInputNodeChanged() to run the callback.
    EXPECT_CALL(
        *mock_cras_proxy_.get(),
        DoConnectToSignal(interface_name_, cras::kActiveInputNodeChanged, _, _))
        .WillRepeatedly(Invoke(
            this, &CrasAudioClientTest::OnConnectToActiveInputNodeChanged));

    // Set an expectation so mock_cras_proxy's monitoring
    // OutputNodeVolumeChanged ConnectToSignal will use
    // OnConnectToOutputNodeVolumeChanged() to run the callback.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoConnectToSignal(interface_name_,
                                  cras::kOutputNodeVolumeChanged, _, _))
        .WillRepeatedly(Invoke(
            this, &CrasAudioClientTest::OnConnectToOutputNodeVolumeChanged));

    // Set an expectation so mock_cras_proxy's monitoring
    // InputNodeGainChanged ConnectToSignal will use
    // OnConnectToInputNodeGainChanged() to run the callback.
    EXPECT_CALL(
        *mock_cras_proxy_.get(),
        DoConnectToSignal(interface_name_, cras::kInputNodeGainChanged, _, _))
        .WillRepeatedly(Invoke(
            this, &CrasAudioClientTest::OnConnectToInputNodeGainChanged));

    // Set an expectation so mock_cras_proxy's monitoring
    // HotwordTriggered ConnectToSignal will use OnHotwordTriggered() to
    // run the callback.
    EXPECT_CALL(
        *mock_cras_proxy_.get(),
        DoConnectToSignal(interface_name_, cras::kHotwordTriggered, _, _))
        .WillRepeatedly(Invoke(this, &CrasAudioClientTest::OnHotwordTriggered));

    // Set an expectation so mock_cras_proxy's monitoring
    // NumberOfActiveStreamsChanged ConnectToSignal will use
    // OnNumberOfActiveStreamsChanged() to run the callback.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoConnectToSignal(interface_name_,
                                  cras::kNumberOfActiveStreamsChanged, _, _))
        .WillRepeatedly(
            Invoke(this, &CrasAudioClientTest::OnNumberOfActiveStreamsChanged));

    // Set and expectation so mock_cras_proxy's monitoring
    // NumberOfInputStreamsWithPermissionChanged ConnectToSignal will use
    // OnNumberOfInputStreamsWithPermissionChanged to run the callback.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoConnectToSignal(
                    interface_name_,
                    cras::kNumberOfInputStreamsWithPermissionChanged, _, _))
        .WillRepeatedly(Invoke(
            this,
            &CrasAudioClientTest::OnNumberOfInputStreamsWithPermissionChanged));

    // Set an expectation so mock_cras_proxy's monitoring
    // BluetoothBatteryChanged ConnectToSignal will use
    // OnBluetoothBatteryChanged() to run the callback.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoConnectToSignal(interface_name_,
                                  cras::kBluetoothBatteryChanged, _, _))
        .WillRepeatedly(
            Invoke(this, &CrasAudioClientTest::OnBluetoothBatteryChanged));

    // Set an expectation so mock_cras_proxy's monitoring
    // SurveyTrigger ConnectToSignal will use
    // OnSurveyTriggered() to run the callback.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoConnectToSignal(interface_name_, cras::kSurveyTrigger, _, _))
        .WillRepeatedly(Invoke(this, &CrasAudioClientTest::OnSurveyTriggered));

    // Set an expectation so mock_cras_proxy's monitoring
    // SurveyTrigger ConnectToSignal will use
    // OnSpeakOnMuteDetected() to run the callback.
    EXPECT_CALL(
        *mock_cras_proxy_.get(),
        DoConnectToSignal(interface_name_,
                          cras::kNumberOfNonChromeOutputStreamsChanged, _, _))
        .WillRepeatedly(Invoke(
            this,
            &CrasAudioClientTest::OnNumberOfNonChromeOutputStreamsChanged));

    // Set an expectation so mock_cras_proxy's monitoring
    // SurveyTrigger ConnectToSignal will use
    // OnSpeakOnMuteDetected() to run the callback.
    EXPECT_CALL(
        *mock_cras_proxy_.get(),
        DoConnectToSignal(interface_name_, cras::kSpeakOnMuteDetected, _, _))
        .WillRepeatedly(
            Invoke(this, &CrasAudioClientTest::OnSpeakOnMuteDetected));

    // Set an expectation so mock_cras_proxy's monitoring
    // EwmaPowerReported ConnectToSignal will use
    // OnEwmaPowerReported() to run the callback.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoConnectToSignal(interface_name_, "EwmaPowerReported", _, _))
        .WillRepeatedly(
            Invoke(this, &CrasAudioClientTest::OnEwmaPowerReported));

    // Set an expectation so mock_cras_proxy's monitoring
    // SurveyTrigger ConnectToSignal will use
    // OnNumStreamIgnoreUiGains() to run the callback.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoConnectToSignal(interface_name_,
                                  cras::kNumStreamIgnoreUiGainsChanged, _, _))
        .WillRepeatedly(Invoke(
            this, &CrasAudioClientTest::OnNumStreamIgnoreUiGainsChanged));

    // Set an expectation so mock_cras_proxy's monitoring
    // NumberOfArcStreamsChanged ConnectToSignal will use
    // OnNumberOfArcStreamsChanged() to run the callback.
    EXPECT_CALL(*mock_cras_proxy_.get(),
                DoConnectToSignal(interface_name_,
                                  cras::kNumberOfArcStreamsChanged, _, _))
        .WillRepeatedly(
            Invoke(this, &CrasAudioClientTest::OnNumberOfArcStreamsChanged));

    // Set an expectation so mock_cras_proxy's monitoring
    // SidetoneSupportedChanged ConnectToSignal will use
    // OnSidetoneSupportedChanged() to run the callback.
    EXPECT_CALL(
        *mock_cras_proxy_.get(),
        DoConnectToSignal(interface_name_, "SidetoneSupportedChanged", _, _))
        .WillRepeatedly(
            Invoke(this, &CrasAudioClientTest::OnSidetoneSupportedChanged));

    // Set an expectation so mock_bus's GetObjectProxy() for the given
    // service name and the object path will return mock_cras_proxy_.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(cras::kCrasServiceName,
                               dbus::ObjectPath(cras::kCrasServicePath)))
        .WillOnce(Return(mock_cras_proxy_.get()));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create a client with the mock bus.
    CrasAudioClient::Initialize(mock_bus_.get());
    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    mock_bus_->ShutdownAndBlock();
    CrasAudioClient::Shutdown();
  }

 protected:
  // A callback to intercept and check the method call arguments.
  using ArgumentCheckCallback =
      base::RepeatingCallback<void(dbus::MessageReader* reader)>;

  // Sets expectations for called method name and arguments, and sets response.
  void PrepareForMethodCall(const std::string& method_name,
                            const ArgumentCheckCallback& argument_checker,
                            dbus::Response* response) {
    expected_method_name_ = method_name;
    argument_checker_ = argument_checker;
    response_ = response;
  }

  // Send output mute changed signal to the tested client.
  void SendOutputMuteChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(output_mute_changed_handler_.is_null());
    output_mute_changed_handler_.Run(signal);
  }

  // Send input mute changed signal to the tested client.
  void SendInputMuteChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(input_mute_changed_handler_.is_null());
    input_mute_changed_handler_.Run(signal);
  }

  // Send nodes changed signal to the tested client.
  void SendNodesChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(nodes_changed_handler_.is_null());
    nodes_changed_handler_.Run(signal);
  }

  // Send active output node changed signal to the tested client.
  void SendActiveOutputNodeChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(active_output_node_changed_handler_.is_null());
    active_output_node_changed_handler_.Run(signal);
  }

  // Send active input node changed signal to the tested client.
  void SendActiveInputNodeChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(active_input_node_changed_handler_.is_null());
    active_input_node_changed_handler_.Run(signal);
  }

  // Send output node volume changed signal to the tested client.
  void SendOutputNodeVolumeChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(output_node_volume_changed_handler_.is_null());
    output_node_volume_changed_handler_.Run(signal);
  }

  // Send output node volume changed signal to the tested client.
  void SendInputNodeGainChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(input_node_gain_changed_handler_.is_null());
    input_node_gain_changed_handler_.Run(signal);
  }

  // Send hotword triggered signal to the tested client.
  void SendHotwordTriggeredSignal(dbus::Signal* signal) {
    ASSERT_FALSE(hotword_triggered_handler_.is_null());
    hotword_triggered_handler_.Run(signal);
  }

  // Send number-of-active-streams-changed signal to the tested client.
  void SendNumberOfActiveStreamsChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(number_of_active_streams_changed_handler_.is_null());
    number_of_active_streams_changed_handler_.Run(signal);
  }

  // Send number-of-input-streams-with-permission-changed signal to the tested
  // client.
  void SendNumberOfInputStreamsWithPermissionChangedSignal(
      dbus::Signal* signal) {
    ASSERT_FALSE(
        number_of_input_streams_with_permission_changed_handler_.is_null());
    number_of_input_streams_with_permission_changed_handler_.Run(signal);
  }

  // Send Bluetooth battery changed signal to the tested client.
  void SendBluetoothBatteryChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(bluetooth_battery_changed_handler_.is_null());
    bluetooth_battery_changed_handler_.Run(signal);
  }

  // Send survey-trigger signal to the tested client.
  void SendSurveyTriggerSignal(dbus::Signal* signal) {
    ASSERT_FALSE(survey_trigger_handler_.is_null());
    survey_trigger_handler_.Run(signal);
  }

  // Send speak-on-mute detected signal to the tested client.
  void SendSpeakOnMuteDetectedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(speak_on_mute_detected_handler_.is_null());
    speak_on_mute_detected_handler_.Run(signal);
  }

  // Send EwmaPowerReported signal to the tested client.
  void SendEwmaPowerReportedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(ewma_power_reported_handler_.is_null());
    ewma_power_reported_handler_.Run(signal);
  }

  void SendNumberOfNonChromeOutputStreamsChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(
        number_of_non_chrome_output_streams_changed_handler_.is_null());
    number_of_non_chrome_output_streams_changed_handler_.Run(signal);
  }

  // Send num-stream-ignore-ui-gains changed signal to the tested client.
  void SendNumStreamIgnoreUiGainsSignal(dbus::Signal* signal) {
    ASSERT_FALSE(num_stream_ignore_ui_gains_handler_.is_null());
    num_stream_ignore_ui_gains_handler_.Run(signal);
  }

  // Send number of arc streams changed signal to the tested client.
  void SendNumberOfArcStreamsChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(number_of_arc_streams_changed_handler_.is_null());
    number_of_arc_streams_changed_handler_.Run(signal);
  }

  // Send sidetone supported changed signal to the tested client.
  void SendSidetoneSupportedChangedSignal(dbus::Signal* signal) {
    ASSERT_FALSE(sidetone_supported_changed_handler_.is_null());
    sidetone_supported_changed_handler_.Run(signal);
  }

  CrasAudioClient* client() { return CrasAudioClient::Get(); }

  // The interface name.
  const std::string interface_name_;
  // A message loop to emulate asynchronous behavior.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // The mock bus.
  scoped_refptr<dbus::MockBus> mock_bus_;
  // The mock object proxy.
  scoped_refptr<dbus::MockObjectProxy> mock_cras_proxy_;
  // The OutputMuteChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback output_mute_changed_handler_;
  // The InputMuteChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback input_mute_changed_handler_;
  // The NodesChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback nodes_changed_handler_;
  // The ActiveOutputNodeChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback active_output_node_changed_handler_;
  // The ActiveInputNodeChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback active_input_node_changed_handler_;
  // The OutputNodeVolumeChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback output_node_volume_changed_handler_;
  // The InputNodeGainChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback input_node_gain_changed_handler_;

  // The HotwordTriggered signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback hotword_triggered_handler_;
  // The NumberOfActiveStreamsChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback number_of_active_streams_changed_handler_;
  // The NumberOfInputStreamsWithPermissionChanged signal handler given by the
  // tested client.
  dbus::ObjectProxy::SignalCallback
      number_of_input_streams_with_permission_changed_handler_;
  // The BluetoothBatteryChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback bluetooth_battery_changed_handler_;
  // The SurveyTrigger signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback survey_trigger_handler_;
  // The SpeakOnMuteDetected signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback speak_on_mute_detected_handler_;
  // The EwmaPowerReported signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback ewma_power_reported_handler_;
  // The NumberOfNonChromeOutputStreamsChanged signal handler given by the
  // tested client.
  dbus::ObjectProxy::SignalCallback
      number_of_non_chrome_output_streams_changed_handler_;
  // The NumStreamIgnoreUiGains signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback num_stream_ignore_ui_gains_handler_;
  // The NumberOfArcStreamsChanged signal handler given by the
  // tested client.
  dbus::ObjectProxy::SignalCallback number_of_arc_streams_changed_handler_;
  // The SidetoneSupportedChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback sidetone_supported_changed_handler_;
  // The name of the method which is expected to be called.
  std::string expected_method_name_;
  // The response which the mock cras proxy returns.
  raw_ptr<dbus::Response, DanglingUntriaged> response_;
  // A callback to intercept and check the method call arguments.
  ArgumentCheckCallback argument_checker_;

 private:
  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnConnectToOutputMuteChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    output_mute_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnConnectToInputMuteChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    input_mute_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnConnectToNodesChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    nodes_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnConnectToActiveOutputNodeChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    active_output_node_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnConnectToActiveInputNodeChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    active_input_node_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnConnectToOutputNodeVolumeChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    output_node_volume_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnConnectToInputNodeGainChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    input_node_gain_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnHotwordTriggered(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    hotword_triggered_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnNumberOfActiveStreamsChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    number_of_active_streams_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  void OnNumberOfInputStreamsWithPermissionChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    number_of_input_streams_with_permission_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnBluetoothBatteryChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    bluetooth_battery_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnSurveyTriggered(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    survey_trigger_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  void OnNumberOfNonChromeOutputStreamsChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    number_of_non_chrome_output_streams_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }
  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnSpeakOnMuteDetected(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    speak_on_mute_detected_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnEwmaPowerReported(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    ewma_power_reported_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnNumStreamIgnoreUiGainsChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    num_stream_ignore_ui_gains_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnNumberOfArcStreamsChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    number_of_arc_streams_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the requested interface name and signal name.
  // Used to implement the mock cras proxy.
  void OnSidetoneSupportedChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    sidetone_supported_changed_handler_ = signal_callback;
    constexpr bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }

  // Checks the content of the method call and returns the response.
  // Used to implement the mock cras proxy.
  void OnCallMethod(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback* response) {
    EXPECT_EQ(interface_name_, method_call->GetInterface());
    EXPECT_EQ(expected_method_name_, method_call->GetMember());
    dbus::MessageReader reader(method_call);
    argument_checker_.Run(&reader);
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*response), response_));
  }

  // Checks the content of the method call and returns the response.
  // Used to implement the mock cras proxy.
  void OnCallMethodWithErrorCallback(
      dbus::MethodCall* method_call,
      int timeout_ms,
      dbus::ObjectProxy::ResponseCallback* response_callback,
      dbus::ObjectProxy::ErrorCallback* error_callback) {
    OnCallMethod(method_call, timeout_ms, response_callback);
  }
};

TEST_F(CrasAudioClientTest, OutputMuteChanged) {
  const bool kSystemMuteOn = false;
  const bool kUserMuteOn = true;
  // Create a signal.
  dbus::Signal signal(cras::kCrasControlInterface, cras::kOutputMuteChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendBool(kSystemMuteOn);
  writer.AppendBool(kUserMuteOn);

  // Set expectations.
  MockObserver observer;
  EXPECT_CALL(observer, OutputMuteChanged(kUserMuteOn)).Times(1);

  // Add the observer.
  client()->AddObserver(&observer);

  // Run the signal callback.
  SendOutputMuteChangedSignal(&signal);

  // Remove the observer.
  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, OutputMuteChanged(_)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendOutputMuteChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, InputMuteChanged) {
  const bool kInputMuteOn = true;
  // Create a signal.
  dbus::Signal signal(cras::kCrasControlInterface, cras::kInputMuteChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendBool(kInputMuteOn);

  // Set expectations.
  MockObserver observer;
  EXPECT_CALL(observer, InputMuteChanged(kInputMuteOn)).Times(1);

  // Add the observer.
  client()->AddObserver(&observer);

  // Run the signal callback.
  SendInputMuteChangedSignal(&signal);

  // Remove the observer.
  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, InputMuteChanged(_)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendInputMuteChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, HotwordTriggered) {
  dbus::Signal signal(cras::kCrasControlInterface, cras::kHotwordTriggered);
  dbus::MessageWriter writer(&signal);
  writer.AppendInt64(0);
  writer.AppendInt64(0);

  MockObserver observer;

  // Set expectations.
  EXPECT_CALL(observer, HotwordTriggered(_, _)).Times(1);

  // Add the observer.
  client()->AddObserver(&observer);

  // Run the signal callback.
  SendHotwordTriggeredSignal(&signal);

  // Remove the observer.
  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, HotwordTriggered(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendHotwordTriggeredSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, NumberOfActiveStreamsChanged) {
  dbus::Signal signal(cras::kCrasControlInterface,
                      cras::kNumberOfActiveStreamsChanged);
  MockObserver observer;
  EXPECT_CALL(observer, NumberOfActiveStreamsChanged()).Times(1);

  client()->AddObserver(&observer);

  SendNumberOfActiveStreamsChangedSignal(&signal);

  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, NumberOfActiveStreamsChanged()).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendNumberOfActiveStreamsChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, NumberOfInputStreamsWithPermissionChanged) {
  dbus::Signal signal(cras::kCrasControlInterface,
                      cras::kNumberOfInputStreamsWithPermissionChanged);
  dbus::MessageWriter writer(&signal);
  base::flat_map<std::string, uint32_t> num_input_streams = {
      {"CRAS_CLIENT_TYPE_CHROME", 1}, {"CRAS_CLIENT_TYPE_CROSVM", 0}};
  for (auto& it : num_input_streams) {
    dbus::MessageWriter sub_writer(nullptr);
    dbus::MessageWriter entry_writer(nullptr);
    writer.OpenArray("{sv}", &sub_writer);
    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kClientType);
    entry_writer.AppendVariantOfString(it.first);
    sub_writer.CloseContainer(&entry_writer);
    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(cras::kNumStreamsWithPermission);
    entry_writer.AppendVariantOfUint32(it.second);
    sub_writer.CloseContainer(&entry_writer);
    writer.CloseContainer(&sub_writer);
  }

  MockObserver observer;
  EXPECT_CALL(observer,
              NumberOfInputStreamsWithPermissionChanged(num_input_streams))
      .Times(1);

  client()->AddObserver(&observer);

  SendNumberOfInputStreamsWithPermissionChangedSignal(&signal);

  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer,
              NumberOfInputStreamsWithPermissionChanged(num_input_streams))
      .Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendNumberOfInputStreamsWithPermissionChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, BluetoothBatteryChanged) {
  const std::string address = "11:22:33:44:55:66";
  const uint32_t level = 82;
  dbus::Signal signal(cras::kCrasControlInterface,
                      cras::kBluetoothBatteryChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(address);
  writer.AppendUint32(level);
  MockObserver observer;
  EXPECT_CALL(observer, BluetoothBatteryChanged(address, level)).Times(1);

  client()->AddObserver(&observer);

  SendBluetoothBatteryChangedSignal(&signal);

  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, BluetoothBatteryChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendBluetoothBatteryChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SurveyTrigger) {
  dbus::Signal signal(cras::kCrasControlInterface, cras::kSurveyTrigger);
  dbus::MessageWriter writer(&signal);
  dbus::MessageWriter sub_writer(nullptr);
  base::flat_map<std::string, std::string> survey_specific_data = {
      {"StreamType", "CRAS_STREAM_TYPE_DEFAULT"},
      {"ClientType", "CRAS_CLIENT_TYPE_CHROME"},
      {"NodeType", "USB_INTERNAL_SPEAKER"}};

  writer.OpenArray("{sv}", &sub_writer);
  for (auto& it : survey_specific_data) {
    dbus::MessageWriter entry_writer(nullptr);
    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(it.first);
    entry_writer.AppendVariantOfString(it.second);
    sub_writer.CloseContainer(&entry_writer);
  }
  writer.CloseContainer(&sub_writer);

  MockObserver observer;
  EXPECT_CALL(observer, SurveyTriggered(survey_specific_data)).Times(1);

  client()->AddObserver(&observer);

  SendSurveyTriggerSignal(&signal);

  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, SurveyTriggered(survey_specific_data)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendSurveyTriggerSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, NumberOfNonChromeOutputStreamsChanged) {
  dbus::Signal signal(cras::kCrasControlInterface,
                      cras::kNumberOfNonChromeOutputStreamsChanged);
  MockObserver observer;
  EXPECT_CALL(observer, NumberOfNonChromeOutputStreamsChanged()).Times(1);
  client()->AddObserver(&observer);
  SendNumberOfNonChromeOutputStreamsChangedSignal(&signal);
  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, NumberOfNonChromeOutputStreamsChanged()).Times(0);
  // Run the signal callback again and make sure the observer isn't called.
  SendSpeakOnMuteDetectedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SpeakOnMuteDetected) {
  dbus::Signal signal(cras::kCrasControlInterface, cras::kSpeakOnMuteDetected);
  MockObserver observer;
  EXPECT_CALL(observer, SpeakOnMuteDetected()).Times(1);

  client()->AddObserver(&observer);

  SendSpeakOnMuteDetectedSignal(&signal);

  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, SpeakOnMuteDetected()).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendSpeakOnMuteDetectedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, EwmaPowerReported) {
  const double kPower = 0.5;

  dbus::Signal signal(cras::kCrasControlInterface, "EwmaPowerReported");
  dbus::MessageWriter writer(&signal);
  writer.AppendDouble(kPower);

  MockObserver observer;
  EXPECT_CALL(observer, EwmaPowerReported(kPower)).Times(1);
  client()->AddObserver(&observer);
  SendEwmaPowerReportedSignal(&signal);
  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, EwmaPowerReported(kPower)).Times(0);
  // Run the signal callback again and make sure the observer isn't called.
  SendEwmaPowerReportedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, NumStreamIgnoreUiGainsChanged) {
  const int32_t kNumStream = 1;

  dbus::Signal signal(cras::kCrasControlInterface,
                      cras::kNumStreamIgnoreUiGainsChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendInt32(kNumStream);

  MockObserver observer;
  EXPECT_CALL(observer, NumStreamIgnoreUiGains(kNumStream)).Times(1);

  client()->AddObserver(&observer);

  SendNumStreamIgnoreUiGainsSignal(&signal);

  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, NumStreamIgnoreUiGains(kNumStream)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendNumStreamIgnoreUiGainsSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, NumberOfArcStreamsChanged) {
  dbus::Signal signal(cras::kCrasControlInterface,
                      cras::kNumberOfArcStreamsChanged);
  MockObserver observer;
  EXPECT_CALL(observer, NumberOfArcStreamsChanged()).Times(1);
  client()->AddObserver(&observer);
  SendNumberOfArcStreamsChangedSignal(&signal);
  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, NumberOfArcStreamsChanged()).Times(0);
  // Run the signal callback again and make sure the observer isn't called.
  SendNumberOfArcStreamsChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SidetoneSupportedChanged) {
  const bool kSupported = true;

  dbus::Signal signal(cras::kCrasControlInterface, "SidetoneSupportedChanged");
  dbus::MessageWriter writer(&signal);
  writer.AppendBool(kSupported);

  MockObserver observer;
  EXPECT_CALL(observer, SidetoneSupportedChanged(kSupported)).Times(1);
  client()->AddObserver(&observer);
  SendSidetoneSupportedChangedSignal(&signal);
  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, SidetoneSupportedChanged(kSupported)).Times(0);
  // Run the signal callback again and make sure the observer isn't called.
  SendSidetoneSupportedChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, NodesChanged) {
  // Create a signal.
  dbus::Signal signal(cras::kCrasControlInterface, cras::kNodesChanged);
  // Set expectations.
  MockObserver observer;
  EXPECT_CALL(observer, NodesChanged()).Times(1);

  // Add the observer.
  client()->AddObserver(&observer);

  // Run the signal callback.
  SendNodesChangedSignal(&signal);

  // Remove the observer.
  client()->RemoveObserver(&observer);

  EXPECT_CALL(observer, NodesChanged()).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendNodesChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, ActiveOutputNodeChanged) {
  const uint64_t kNodeId = 10002;
  // Create a signal.
  dbus::Signal signal(cras::kCrasControlInterface,
                      cras::kActiveOutputNodeChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendUint64(kNodeId);

  // Set expectations.
  MockObserver observer;
  EXPECT_CALL(observer, ActiveOutputNodeChanged(kNodeId)).Times(1);

  // Add the observer.
  client()->AddObserver(&observer);

  // Run the signal callback.
  SendActiveOutputNodeChangedSignal(&signal);

  // Remove the observer.
  client()->RemoveObserver(&observer);
  EXPECT_CALL(observer, ActiveOutputNodeChanged(_)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendActiveOutputNodeChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, ActiveInputNodeChanged) {
  const uint64_t kNodeId = 20002;
  // Create a signal.
  dbus::Signal signal(cras::kCrasControlInterface,
                      cras::kActiveInputNodeChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendUint64(kNodeId);

  // Set expectations.
  MockObserver observer;
  EXPECT_CALL(observer, ActiveInputNodeChanged(kNodeId)).Times(1);

  // Add the observer.
  client()->AddObserver(&observer);

  // Run the signal callback.
  SendActiveInputNodeChangedSignal(&signal);

  // Remove the observer.
  client()->RemoveObserver(&observer);
  EXPECT_CALL(observer, ActiveInputNodeChanged(_)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendActiveInputNodeChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, OutputNodeVolumeChanged) {
  const uint64_t kNodeId = 20003;
  const int32_t volume = 82;
  // Create a signal
  dbus::Signal signal(cras::kCrasControlInterface,
                      cras::kOutputNodeVolumeChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendUint64(kNodeId);
  writer.AppendInt32(volume);

  // Set expectations
  MockObserver observer;
  EXPECT_CALL(observer, OutputNodeVolumeChanged(kNodeId, volume)).Times(1);

  // Add the observer.
  client()->AddObserver(&observer);

  // Run the signal callback.
  SendOutputNodeVolumeChangedSignal(&signal);

  // Remove the observer.
  client()->RemoveObserver(&observer);
  EXPECT_CALL(observer, OutputNodeVolumeChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendOutputNodeVolumeChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, InputNodeGainChanged) {
  const uint64_t kNodeId = 20003;
  const int32_t gain = 82;
  // Create a signal
  dbus::Signal signal(cras::kCrasControlInterface, cras::kInputNodeGainChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendUint64(kNodeId);
  writer.AppendInt32(gain);

  // Set expectations
  MockObserver observer;
  EXPECT_CALL(observer, InputNodeGainChanged(kNodeId, gain)).Times(1);

  // Add the observer.
  client()->AddObserver(&observer);

  // Run the signal callback.
  SendInputNodeGainChangedSignal(&signal);

  // Remove the observer.
  client()->RemoveObserver(&observer);
  EXPECT_CALL(observer, InputNodeGainChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendInputNodeGainChangedSignal(&signal);

  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, GetNodes) {
  // Create the expected value.
  AudioNodeList expected_node_list{kInternalSpeaker, kInternalMic};

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  WriteNodesToResponse(expected_node_list, &writer);

  // Set expectations.
  PrepareForMethodCall(cras::kGetNodes, base::BindRepeating(&ExpectNoArgument),
                       response.get());
  // Call method.
  bool called = false;
  client()->GetNodes(
      base::BindOnce(&ExpectAudioNodeListResult, &called, expected_node_list));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(CrasAudioClientTest, GetNodesV2) {
  // Create the expected value.
  AudioNodeList expected_node_list{kInternalSpeakerV2, kInternalMicV2};

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  WriteNodesToResponse(expected_node_list, &writer);

  // Set expectations.
  PrepareForMethodCall(cras::kGetNodes, base::BindRepeating(&ExpectNoArgument),
                       response.get());

  // Call method.
  bool called = false;
  client()->GetNodes(
      base::BindOnce(&ExpectAudioNodeListResult, &called, expected_node_list));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(CrasAudioClientTest, SetOutputNodeVolume) {
  const uint64_t kNodeId = 10003;
  const int32_t kVolume = 80;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      cras::kSetOutputNodeVolume,
      base::BindRepeating(&ExpectUint64AndInt32Arguments, kNodeId, kVolume),
      response.get());
  // Call method.
  client()->SetOutputNodeVolume(kNodeId, kVolume);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetOutputUserMute) {
  const bool kUserMuteOn = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kSetOutputUserMute,
                       base::BindRepeating(&ExpectBoolArgument, kUserMuteOn),
                       response.get());
  // Call method.
  client()->SetOutputUserMute(kUserMuteOn);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetInputNodeGain) {
  const uint64_t kNodeId = 20003;
  const int32_t kInputGain = 2;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      cras::kSetInputNodeGain,
      base::BindRepeating(&ExpectUint64AndInt32Arguments, kNodeId, kInputGain),
      response.get());
  // Call method.
  client()->SetInputNodeGain(kNodeId, kInputGain);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetInputMute) {
  const bool kInputMuteOn = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kSetInputMute,
                       base::BindRepeating(&ExpectBoolArgument, kInputMuteOn),
                       response.get());
  // Call method.
  client()->SetInputMute(kInputMuteOn);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetNoiseCancellationEnabled) {
  const bool kNoiseCancellationOn = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      cras::kSetNoiseCancellationEnabled,
      base::BindRepeating(&ExpectBoolArgument, kNoiseCancellationOn),
      response.get());
  // Call method.
  client()->SetNoiseCancellationEnabled(kNoiseCancellationOn);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetStyleTransferEnabled) {
  const bool kStyleTransferOn = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      cras::kSetStyleTransferEnabled,
      base::BindRepeating(&ExpectBoolArgument, kStyleTransferOn),
      response.get());
  // Call method.
  client()->SetStyleTransferEnabled(kStyleTransferOn);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetHfpMicSrEnabled) {
  const bool kHfpMicSrOn = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kSetHfpMicSrEnabled,
                       base::BindRepeating(&ExpectBoolArgument, kHfpMicSrOn),
                       response.get());
  // Call method.
  client()->SetHfpMicSrEnabled(kHfpMicSrOn);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetActiveOutputNode) {
  const uint64_t kNodeId = 10004;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kSetActiveOutputNode,
                       base::BindRepeating(&ExpectUint64Argument, kNodeId),
                       response.get());
  // Call method.
  client()->SetActiveOutputNode(kNodeId);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetActiveInputNode) {
  const uint64_t kNodeId = 20004;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kSetActiveInputNode,
                       base::BindRepeating(&ExpectUint64Argument, kNodeId),
                       response.get());
  // Call method.
  client()->SetActiveInputNode(kNodeId);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, AddActiveInputNode) {
  const uint64_t kNodeId = 20005;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kAddActiveInputNode,
                       base::BindRepeating(&ExpectUint64Argument, kNodeId),
                       response.get());
  // Call method.
  client()->AddActiveInputNode(kNodeId);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, RemoveActiveInputNode) {
  const uint64_t kNodeId = 20006;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kRemoveActiveInputNode,
                       base::BindRepeating(&ExpectUint64Argument, kNodeId),
                       response.get());
  // Call method.
  client()->RemoveActiveInputNode(kNodeId);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, AddActiveOutputNode) {
  const uint64_t kNodeId = 10005;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kAddActiveOutputNode,
                       base::BindRepeating(&ExpectUint64Argument, kNodeId),
                       response.get());
  // Call method.
  client()->AddActiveOutputNode(kNodeId);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, RemoveActiveOutputNode) {
  const uint64_t kNodeId = 10006;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kRemoveActiveOutputNode,
                       base::BindRepeating(&ExpectUint64Argument, kNodeId),
                       response.get());
  // Call method.
  client()->RemoveActiveOutputNode(kNodeId);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SwapLeftRight) {
  const uint64_t kNodeId = 10007;
  const bool kSwap = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      cras::kSwapLeftRight,
      base::BindRepeating(&ExpectUint64AndBoolArguments, kNodeId, kSwap),
      response.get());
  // Call method.
  client()->SwapLeftRight(kNodeId, kSwap);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetGlobalOutputChannelRemix) {
  const int32_t kChannels = 2;
  const std::vector<double> kMixer = {0, 0.1, 0.5, 1};
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      cras::kSetGlobalOutputChannelRemix,
      base::BindRepeating(&ExpectInt32AndArrayOfDoublesArguments, kChannels,
                          kMixer),
      response.get());

  // Call method.
  client()->SetGlobalOutputChannelRemix(kChannels, kMixer);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetPlayerPlaybackStatus) {
  const std::string kStatus = "paused";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kSetPlayerPlaybackStatus,
                       base::BindRepeating(&ExpectStringArgument, kStatus),
                       response.get());

  // Call method.
  client()->SetPlayerPlaybackStatus(kStatus);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetPlayerIdentity) {
  const std::string kIdentity = "Chrome Player";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kSetPlayerIdentity,
                       base::BindRepeating(&ExpectStringArgument, kIdentity),
                       response.get());

  // Call method.
  client()->SetPlayerIdentity(kIdentity);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetPlayerPosition) {
  const int64_t kPosition = 2020224;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kSetPlayerPosition,
                       base::BindRepeating(&ExpectInt64Argument, kPosition),
                       response.get());

  // Call method.
  client()->SetPlayerPosition(kPosition);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetPlayerDuration) {
  const int64_t kDuration = 20200302;
  const std::map<std::string, int64_t> kMetadata = {{"length", kDuration}};
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      cras::kSetPlayerMetadata,
      base::BindRepeating(&ExpectArrayOfDictOfStringAndVariantInt64Arguments,
                          kMetadata),
      response.get());

  // Call method.
  client()->SetPlayerDuration(kDuration);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetPlayerMetadata) {
  const std::string kTitle = "Chrome Metadata Title";
  const std::string kAlbum = "Chrome Metadata Album";
  const std::string kArtist = "Chrome Metadata Artist";
  const std::map<std::string, std::string> kMetadata = {
      {"title", kTitle}, {"album", kAlbum}, {"artist", kArtist}};
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      cras::kSetPlayerMetadata,
      base::BindRepeating(&ExpectArrayOfDictOfStringAndVariantStringArguments,
                          kMetadata),
      response.get());

  // Call method.
  client()->SetPlayerMetadata(kMetadata);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, ResendBluetoothBattery) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(cras::kResendBluetoothBattery,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  // Call method.
  client()->ResendBluetoothBattery();
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetSpeakOnMuteDetection) {
  const bool kSpeakOnMuteDetectionOn = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      cras::kSetSpeakOnMuteDetection,
      base::BindRepeating(&ExpectBoolArgument, kSpeakOnMuteDetectionOn),
      response.get());
  // Call method.
  client()->SetSpeakOnMuteDetection(kSpeakOnMuteDetectionOn);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrasAudioClientTest, SetForceRespectUiGainsEnabled) {
  const bool kForceRespectUiGainsOn = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      cras::kSetForceRespectUiGains,
      base::BindRepeating(&ExpectBoolArgument, kForceRespectUiGainsOn),
      response.get());
  // Call method.
  client()->SetForceRespectUiGains(kForceRespectUiGainsOn);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
