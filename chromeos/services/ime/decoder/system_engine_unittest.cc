// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/decoder/system_engine.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/ime/decoder/proto_conversion.h"
#include "chromeos/services/ime/mock_input_channel.h"
#include "chromeos/services/ime/public/proto/messages.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace ime {

// EqualsProto must match against two arguments: a buffer containing the actual
// serialized proto and a number indicating the size of the buffer.
MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized;
  message.SerializeToString(&expected_serialized);

  const char* bytes = reinterpret_cast<const char*>(std::get<0>(arg));
  int size = std::get<1>(arg);
  std::string actual_serialized(bytes, size);
  return expected_serialized == actual_serialized;
}

constexpr char kImeSpec[] = "xkb:us::eng";

class MockImeEngineMainEntry : public ImeEngineMainEntry {
 public:
  bool IsImeSupported(const char*) final { return true; }
  bool ActivateIme(const char*, ImeClientDelegate* delegate) final {
    delegate_ = delegate;
    return true;
  }
  MOCK_METHOD(void, Process, (const uint8_t* data, size_t size));
  void Destroy() final {}

  ImeClientDelegate* delegate() const { return delegate_; }

 private:
  ImeClientDelegate* delegate_;
};

// Sets up the test environment for Mojo and inject a mock ImeEngineMainEntry.
class SystemEngineTest : public testing::Test {
 protected:
  void SetUp() final {
    FakeEngineMainEntryForTesting(&mock_main_entry_);
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kSystemLatinPhysicalTyping}, {});
  }

  void TearDown() final { FakeEngineMainEntryForTesting(nullptr); }

  MockImeEngineMainEntry mock_main_entry_;

 private:
  // Mojo calls need a SequencedTaskRunner.
  base::test::SingleThreadTaskEnvironment task_environment;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SystemEngineTest, BindRequestBindsInterfaces) {
  SystemEngine engine(/*platform=*/nullptr);

  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  EXPECT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));

  EXPECT_TRUE(client.is_bound());
  EXPECT_TRUE(mock_channel.IsBound());
}

TEST_F(SystemEngineTest, OnInputMethodChangedSendsMessageToSharedLib) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));
  ime::Wrapper expected_proto;
  *expected_proto.mutable_public_message() =
      OnInputMethodChangedToProto(/*seq_id=*/0, "xkb:us::eng");

  EXPECT_CALL(mock_main_entry_, Process).With(EqualsProto(expected_proto));

  client->OnInputMethodChanged("xkb:us::eng");
  client.FlushForTesting();
}

TEST_F(SystemEngineTest, OnFocusSendsMessageToSharedLib) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));

  auto info = mojom::InputFieldInfo::New(mojom::InputFieldType::kNumber,
                                         mojom::AutocorrectMode::kEnabled,
                                         mojom::PersonalizationMode::kEnabled);

  ime::Wrapper expected_proto;
  *expected_proto.mutable_public_message() =
      OnFocusToProto(/*seq_id=*/0, info.Clone());

  EXPECT_CALL(mock_main_entry_, Process).With(EqualsProto(expected_proto));

  client->OnFocus(info.Clone());
  client.FlushForTesting();
}

TEST_F(SystemEngineTest, OnBlurSendsMessageToSharedLib) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));
  ime::Wrapper expected_proto;
  *expected_proto.mutable_public_message() = OnBlurToProto(/*seq_id=*/0);

  EXPECT_CALL(mock_main_entry_, Process).With(EqualsProto(expected_proto));

  client->OnBlur();
  client.FlushForTesting();
}

TEST_F(SystemEngineTest, OnKeyEventRepliesWithCallback) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));
  auto key_event = mojom::PhysicalKeyEvent::New(
      mojom::KeyEventType::kKeyDown, "KeyA", "A", mojom::ModifierState::New());
  ime::Wrapper expected_proto;
  *expected_proto.mutable_public_message() =
      OnKeyEventToProto(/*seq_id=*/0, key_event.Clone());

  // Set up the mock shared library to reply to the key event.
  bool consumed_by_test = false;
  EXPECT_CALL(mock_main_entry_, Process)
      .With(EqualsProto(expected_proto))
      .WillOnce([this]() {
        ime::Wrapper wrapper;
        wrapper.mutable_public_message()
            ->mutable_on_key_event_reply()
            ->set_consumed(true);
        std::vector<uint8_t> output(wrapper.ByteSizeLong());
        wrapper.SerializeToArray(output.data(), output.size());
        mock_main_entry_.delegate()->Process(output.data(), output.size());
      });

  client->OnKeyEvent(
      std::move(key_event),
      base::BindLambdaForTesting(
          [&consumed_by_test](bool consumed) { consumed_by_test = consumed; }));
  client.FlushForTesting();

  EXPECT_TRUE(consumed_by_test);
}

TEST_F(SystemEngineTest, OnSurroundingTextChangedSendsMessageToSharedLib) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));
  const auto selection = mojom::SelectionRange::New(/*anchor=*/3, /*focus=*/2);
  ime::Wrapper expected_proto;
  *expected_proto.mutable_public_message() = OnSurroundingTextChangedToProto(
      /*seq_id=*/0, "hello", /*offset=*/1, selection->Clone());

  EXPECT_CALL(mock_main_entry_, Process).With(EqualsProto(expected_proto));

  client->OnSurroundingTextChanged("hello", /*offset=*/1, selection->Clone());
  client.FlushForTesting();
}

TEST_F(SystemEngineTest, OnCompositionCanceledSendsMessageToSharedLib) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));
  ime::Wrapper expected_proto;
  *expected_proto.mutable_public_message() =
      OnCompositionCanceledToProto(/*seq_id=*/0);

  EXPECT_CALL(mock_main_entry_, Process).With(EqualsProto(expected_proto));

  client->OnCompositionCanceled();
  client.FlushForTesting();
}

TEST_F(SystemEngineTest, CommitTextSendsMessageToReceiver) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));
  ime::Wrapper proto;
  proto.mutable_public_message()->mutable_commit_text()->set_text("hello");

  EXPECT_CALL(mock_channel, CommitText("hello"));

  const std::string serialized = proto.SerializeAsString();
  mock_main_entry_.delegate()->Process(
      reinterpret_cast<const uint8_t*>(serialized.data()), serialized.size());
  mock_channel.FlushForTesting();
}

TEST_F(SystemEngineTest, SetCompositionSendsMessageToReceiver) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));
  ime::Wrapper proto;
  proto.mutable_public_message()->mutable_set_composition()->set_text("hello");

  EXPECT_CALL(mock_channel, SetComposition("hello"));

  const std::string serialized = proto.SerializeAsString();
  mock_main_entry_.delegate()->Process(
      reinterpret_cast<const uint8_t*>(serialized.data()), serialized.size());
  mock_channel.FlushForTesting();
}

TEST_F(SystemEngineTest, SetCompositionRangeSendsMessageToReceiver) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));
  ime::Wrapper proto;
  proto.mutable_public_message()
      ->mutable_set_composition_range()
      ->set_start_byte_index(2);
  proto.mutable_public_message()
      ->mutable_set_composition_range()
      ->set_end_byte_index(6);

  EXPECT_CALL(mock_channel, SetCompositionRange(2, 6));

  const std::string serialized = proto.SerializeAsString();
  mock_main_entry_.delegate()->Process(
      reinterpret_cast<const uint8_t*>(serialized.data()), serialized.size());
  mock_channel.FlushForTesting();
}

TEST_F(SystemEngineTest, FinishCompositionSendsMessageToReceiver) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));
  ime::Wrapper proto;
  *proto.mutable_public_message()->mutable_finish_composition() =
      ime::FinishComposition();

  EXPECT_CALL(mock_channel, FinishComposition());

  const std::string serialized = proto.SerializeAsString();
  mock_main_entry_.delegate()->Process(
      reinterpret_cast<const uint8_t*>(serialized.data()), serialized.size());
  mock_channel.FlushForTesting();
}

TEST_F(SystemEngineTest, DeleteSurroundingTextSendsMessageToReceiver) {
  SystemEngine engine(/*platform=*/nullptr);
  MockInputChannel mock_channel;
  mojo::Remote<mojom::InputChannel> client;
  ASSERT_TRUE(engine.BindRequest(kImeSpec, client.BindNewPipeAndPassReceiver(),
                                 mock_channel.CreatePendingRemote(), {}));
  ime::Wrapper proto;
  proto.mutable_public_message()
      ->mutable_delete_surrounding_text()
      ->set_num_bytes_before_cursor(2);
  proto.mutable_public_message()
      ->mutable_delete_surrounding_text()
      ->set_num_bytes_after_cursor(6);

  EXPECT_CALL(mock_channel, DeleteSurroundingText(2, 6));

  const std::string serialized = proto.SerializeAsString();
  mock_main_entry_.delegate()->Process(
      reinterpret_cast<const uint8_t*>(serialized.data()), serialized.size());
  mock_channel.FlushForTesting();
}

}  // namespace ime
}  // namespace chromeos
