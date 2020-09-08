// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromeos {
namespace ime {

namespace {

const char kInvalidImeSpec[] = "ime_spec_never_support";
const std::vector<uint8_t> extra{0x66, 0x77, 0x88};

void ConnectCallback(bool* success, bool result) {
  *success = result;
}

void TestProcessKeypressForRulebasedCallback(
    mojom::KeypressResponseForRulebased* res_out,
    mojom::KeypressResponseForRulebasedPtr response) {
  res_out->result = response->result;
  res_out->operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  for (int i = 0; i < (int)response->operations.size(); i++) {
    res_out->operations.push_back(std::move(response->operations[i]));
  }
}
void TestGetRulebasedKeypressCountForTestingCallback(int32_t* res_out,
                                                     int32_t response) {
  *res_out = response;
}

class TestClientChannel : mojom::InputChannel {
 public:
  TestClientChannel() : receiver_(this) {}

  mojo::PendingRemote<mojom::InputChannel> CreatePendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::InputChannel implementation.
  MOCK_METHOD2(ProcessMessage,
               void(const std::vector<uint8_t>& message,
                    ProcessMessageCallback));
  MOCK_METHOD0(OnFocus, void());
  MOCK_METHOD2(ProcessKeypressForRulebased,
               void(const mojom::PhysicalKeyEventPtr event,
                    ProcessKeypressForRulebasedCallback));
  MOCK_METHOD0(ResetForRulebased, void());
  MOCK_METHOD1(GetRulebasedKeypressCountForTesting,
               void(GetRulebasedKeypressCountForTestingCallback));

 private:
  mojo::Receiver<mojom::InputChannel> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestClientChannel);
};

class ImeServiceTest : public testing::Test {
 public:
  ImeServiceTest() : service_(remote_service_.BindNewPipeAndPassReceiver()) {}
  ~ImeServiceTest() override = default;

  MOCK_METHOD1(SentTextCallback, void(const std::string&));
  MOCK_METHOD1(SentMessageCallback, void(const std::vector<uint8_t>&));

 protected:
  void SetUp() override {
    remote_service_->BindInputEngineManager(
        remote_manager_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<mojom::ImeService> remote_service_;
  mojo::Remote<mojom::InputEngineManager> remote_manager_;

 private:
  base::test::TaskEnvironment task_environment_;
  ImeService service_;

  DISALLOW_COPY_AND_ASSIGN(ImeServiceTest);
};

}  // namespace

// Tests that the service is instantiated and it will return false when
// activating an IME engine with an invalid IME spec.
TEST_F(ImeServiceTest, ConnectInvalidImeEngine) {
  bool success = true;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      kInvalidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_FALSE(success);
}

TEST_F(ImeServiceTest, MultipleClientsRulebased) {
  bool success = false;
  TestClientChannel test_channel_1;
  TestClientChannel test_channel_2;
  mojo::Remote<mojom::InputChannel> remote_engine_1;
  mojo::Remote<mojom::InputChannel> remote_engine_2;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", remote_engine_1.BindNewPipeAndPassReceiver(),
      test_channel_1.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", remote_engine_2.BindNewPipeAndPassReceiver(),
      test_channel_2.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  mojom::KeypressResponseForRulebased response;
  remote_engine_1->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "a",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  remote_engine_1.FlushForTesting();

  remote_engine_2->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "a",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  remote_engine_2.FlushForTesting();

  int32_t count;
  remote_engine_1->GetRulebasedKeypressCountForTesting(
      base::BindOnce(&TestGetRulebasedKeypressCountForTestingCallback, &count));
  remote_engine_1.FlushForTesting();
  EXPECT_EQ(1, count);

  remote_engine_2->GetRulebasedKeypressCountForTesting(
      base::BindOnce(&TestGetRulebasedKeypressCountForTestingCallback, &count));
  remote_engine_2.FlushForTesting();
  EXPECT_EQ(1, count);
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleModifierKeys) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> to_engine_remote;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", to_engine_remote.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  constexpr const char* kModifierKeys[] = {
      "Shift",    "ShiftLeft", "ShiftRight", "Alt",         "AltLeft",
      "AltRight", "AltGraph",  "CapsLock",   "ControlLeft", "ControlRight"};

  for (const auto* modifier_key : kModifierKeys) {
    mojom::KeypressResponseForRulebased response;
    to_engine_remote->ProcessKeypressForRulebased(
        mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown,
                                     modifier_key, modifier_key,
                                     mojom::ModifierState::New()),
        base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
    to_engine_remote.FlushForTesting();

    EXPECT_EQ(response.result, false);
    ASSERT_EQ(0U, response.operations.size());
  }
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleCtrlShortCut) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> to_engine_remote;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", to_engine_remote.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  mojom::KeypressResponseForRulebased response;
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "ControlLeft",
                                   "Control", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));

  auto modifier_state_with_control = mojom::ModifierState::New();
  modifier_state_with_control->control = true;
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "a",
                                   modifier_state_with_control->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, false);
  ASSERT_EQ(0U, response.operations.size());
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleAltShortCut) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> to_engine_remote;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", to_engine_remote.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  mojom::KeypressResponseForRulebased response;
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "AltLeft",
                                   "Alt", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));

  auto new_modifier_state = mojom::ModifierState::New();
  new_modifier_state->alt = true;
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "a",
                                   std::move(new_modifier_state)),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, false);
  ASSERT_EQ(0U, response.operations.size());
}

TEST_F(ImeServiceTest, RuleBasedHandlesAltRight) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> to_engine_remote;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", to_engine_remote.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  mojom::KeypressResponseForRulebased response;
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "AltRight",
                                   "Alt", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));

  auto modifier_state_with_alt = mojom::ModifierState::New();
  modifier_state_with_alt->alt = true;
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "a",
                                   modifier_state_with_alt->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, true);
  ASSERT_EQ(1U, response.operations.size());
}

// Tests that the rule-based Arabic keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedArabic) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> to_engine_remote;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", to_engine_remote.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test Shift+KeyA.
  mojom::KeypressResponseForRulebased response;
  auto modifier_state_with_shift = mojom::ModifierState::New();
  modifier_state_with_shift->shift = true;
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "A",
                                   modifier_state_with_shift->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, true);
  std::vector<mojom::OperationForRulebasedPtr> expected_operations;
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::COMMIT_TEXT, "\u0650")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Test KeyB
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyB", "b",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();
  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::COMMIT_TEXT, "\u0644\u0627")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Test unhandled key.
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Enter",
                                   "Enter", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();
  EXPECT_EQ(response.result, false);

  // Test keyup.
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyUp, "Enter",
                                   "Enter", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();
  EXPECT_EQ(response.result, false);

  // TODO(keithlee) Test reset function
  to_engine_remote->ResetForRulebased();

  // Test invalid request.
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "", "",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();
  EXPECT_EQ(response.result, false);
}

// Tests that the rule-based DevaPhone keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedDevaPhone) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> to_engine_remote;

  remote_manager_->ConnectToImeEngine(
      "m17n:deva_phone", to_engine_remote.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  mojom::KeypressResponseForRulebased response;
  std::vector<mojom::OperationForRulebasedPtr> expected_operations;

  // Test KeyN.
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyN", "n",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::SET_COMPOSITION, "\u0928")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Backspace.
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Backspace",
                                   "Backspace", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::SET_COMPOSITION, "")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // KeyN + KeyC.
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyN", "n",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyC", "c",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::SET_COMPOSITION,
      "\u091e\u094d\u091a")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Space.
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Space", " ",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::COMMIT_TEXT, "\u091e\u094d\u091a ")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);
}

// Tests escapable characters. See https://crbug.com/1014384.
TEST_F(ImeServiceTest, RuleBasedDoesNotEscapeCharacters) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> to_engine_remote;

  remote_manager_->ConnectToImeEngine(
      "m17n:deva_phone", to_engine_remote.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  auto modifier_state_with_shift = mojom::ModifierState::New();
  modifier_state_with_shift->shift = true;

  mojom::KeypressResponseForRulebased response;

  // Test Shift+Quote ('"').
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Quote", "\"",
                                   modifier_state_with_shift->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, true);
  ASSERT_EQ(1U, response.operations.size());
  EXPECT_EQ(mojom::OperationMethodForRulebased::COMMIT_TEXT,
            response.operations[0]->method);
  EXPECT_EQ("\"", response.operations[0]->arguments);

  // Backslash.
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Backslash",
                                   "\\", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, true);
  ASSERT_EQ(1U, response.operations.size());
  EXPECT_EQ(mojom::OperationMethodForRulebased::COMMIT_TEXT,
            response.operations[0]->method);
  EXPECT_EQ("\\", response.operations[0]->arguments);

  // Shift+Comma ('<')
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Comma", "<",
                                   modifier_state_with_shift->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, true);
  ASSERT_EQ(1U, response.operations.size());
  EXPECT_EQ(mojom::OperationMethodForRulebased::COMMIT_TEXT,
            response.operations[0]->method);
  EXPECT_EQ("<", response.operations[0]->arguments);
}

// Tests that AltGr works with rule-based. See crbug.com/1035145.
TEST_F(ImeServiceTest, KhmerKeyboardAltGr) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> to_engine_remote;

  remote_manager_->ConnectToImeEngine(
      "m17n:km", to_engine_remote.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test AltRight+KeyA.
  // We do not support AltGr for rule-based. We treat AltRight as AltGr.
  mojom::KeypressResponseForRulebased response;
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "AltRight",
                                   "Alt", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));

  auto modifier_state_with_alt = mojom::ModifierState::New();
  modifier_state_with_alt->alt = true;
  to_engine_remote->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "a",
                                   modifier_state_with_alt->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_remote.FlushForTesting();

  EXPECT_EQ(response.result, true);
  ASSERT_EQ(1U, response.operations.size());
  EXPECT_EQ(mojom::OperationMethodForRulebased::COMMIT_TEXT,
            response.operations[0]->method);
  EXPECT_EQ("+", response.operations[0]->arguments);
}

}  // namespace ime
}  // namespace chromeos
