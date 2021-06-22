// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/services/ime/ime_decoder.h"
#include "chromeos/services/ime/mock_input_channel.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method.mojom.h"
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
constexpr char kValidImeSpec[] = "valid_spec";
const std::vector<uint8_t> extra{0x66, 0x77, 0x88};

void ConnectCallback(bool* success, bool result) {
  *success = result;
}

ImeDecoder::EntryPoints CreateDecoderEntryPoints() {
  ImeDecoder::EntryPoints entry_points;
  entry_points.init_once = [](ImeCrosPlatform* platform) {};
  entry_points.supports = [](const char* ime_spec) {
    return strcmp(kInvalidImeSpec, ime_spec) != 0;
  };
  entry_points.activate_ime = [](const char* ime_spec,
                                 ImeClientDelegate* delegate) { return true; };
  entry_points.process = [](const uint8_t* data, size_t size) {};
  entry_points.close = []() {};
  return entry_points;
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
class ImeServiceTest : public testing::Test, public mojom::InputMethodHost {
 public:
  ImeServiceTest() : service_(remote_service_.BindNewPipeAndPassReceiver()) {}
  ~ImeServiceTest() override = default;

  void CommitText(const std::string& text,
                  mojom::CommitTextCursorBehavior cursor_behavior) override {}
  void SetComposition(const std::string& text) override {}
  void SetCompositionRange(uint32_t start_byte_index,
                           uint32_t end_byte_index) override {}
  void FinishComposition() override {}
  void DeleteSurroundingText(uint32_t num_bytes_before_cursor,
                             uint32_t num_bytes_after_cursor) override {}
  void HandleAutocorrect(mojom::AutocorrectSpanPtr autocorrect_span) override {}
  void RequestSuggestions(mojom::SuggestionsRequestPtr request,
                          RequestSuggestionsCallback callback) override {}
  void DisplaySuggestions(const std::vector<::chromeos::ime::TextSuggestion>&
                              suggestions) override {}
  void RecordUkm(mojom::UkmEntryPtr entry) override {}

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kImeMojoDecoder,
                              features::kSystemLatinPhysicalTyping},
        /*disabled_features=*/{});

    FakeDecoderEntryPointsForTesting(CreateDecoderEntryPoints());
    remote_service_->BindInputEngineManager(
        remote_manager_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<mojom::ImeService> remote_service_;
  mojo::Remote<mojom::InputEngineManager> remote_manager_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  ImeService service_;

  DISALLOW_COPY_AND_ASSIGN(ImeServiceTest);
};

}  // namespace

// Tests that the service is instantiated and it will return false when
// activating an IME engine with an invalid IME spec.
TEST_F(ImeServiceTest, ConnectInvalidImeEngineDoesNotConnectRemote) {
  bool success = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      kInvalidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  EXPECT_FALSE(success);
  EXPECT_FALSE(remote_engine.is_connected());
}

TEST_F(ImeServiceTest, ConnectToValidEngineConnectsRemote) {
  bool success = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), {},
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  EXPECT_TRUE(success);
  EXPECT_TRUE(remote_engine.is_connected());
}

TEST_F(ImeServiceTest, ConnectToImeEngineWillOverrideExistingImeEngine) {
  bool success1, success2 = true;
  MockInputChannel test_channel1, test_channel2;
  mojo::Remote<mojom::InputChannel> remote_engine1, remote_engine2;

  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine1.BindNewPipeAndPassReceiver(),
      test_channel1.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine2.BindNewPipeAndPassReceiver(),
      test_channel2.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_.FlushForTesting();

  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);
  EXPECT_FALSE(remote_engine1.is_connected());
  EXPECT_TRUE(remote_engine2.is_connected());
}

TEST_F(ImeServiceTest,
       ConnectToImeEngineCannotConnectIfInputMethodIsConnected) {
  bool success1, success2 = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToInputMethod(
      kValidImeSpec, input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_.FlushForTesting();

  // The second connection should have failed.
  EXPECT_TRUE(success1);
  EXPECT_FALSE(success2);
  EXPECT_TRUE(input_method.is_connected());
  EXPECT_FALSE(remote_engine.is_connected());
}

TEST_F(ImeServiceTest,
       ConnectToImeEngineCanConnectIfInputMethodIsDisconnected) {
  bool success1, success2 = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToInputMethod(
      kValidImeSpec, input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success1));
  input_method.reset();
  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_.FlushForTesting();

  // The second connection should have succeed since the first connection was
  // disconnected.
  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);
  EXPECT_FALSE(input_method.is_bound());
  EXPECT_TRUE(remote_engine.is_connected());
}

TEST_F(ImeServiceTest, ConnectToInputMethodCanOverrideAnyConnection) {
  bool success1, success2, success3 = true;
  MockInputChannel test_channel;
  mojo::Receiver<mojom::InputMethodHost> host1(this), host2(this);
  mojo::Remote<mojom::InputMethod> input_method1, input_method2;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_->ConnectToInputMethod(
      kValidImeSpec, input_method1.BindNewPipeAndPassReceiver(),
      host1.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_->ConnectToInputMethod(
      kValidImeSpec, input_method2.BindNewPipeAndPassReceiver(),
      host2.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success3));
  remote_manager_.FlushForTesting();

  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);
  EXPECT_TRUE(success3);
  EXPECT_FALSE(remote_engine.is_connected());
  EXPECT_FALSE(input_method1.is_connected());
  EXPECT_TRUE(input_method2.is_connected());
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleModifierKeys) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  constexpr const char* kModifierKeys[] = {
      "Shift",    "ShiftLeft", "ShiftRight", "Alt",         "AltLeft",
      "AltRight", "AltGraph",  "CapsLock",   "ControlLeft", "ControlRight"};

  for (const auto* modifier_key : kModifierKeys) {
    mojom::KeypressResponseForRulebased response;
    input_method->ProcessKeypressForRulebased(
        mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown,
                                     modifier_key, modifier_key,
                                     mojom::ModifierState::New()),
        base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
    input_method.FlushForTesting();

    EXPECT_EQ(response.result, false);
    ASSERT_EQ(0U, response.operations.size());
  }
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleCtrlShortCut) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  mojom::KeypressResponseForRulebased response;
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "ControlLeft",
                                   "Control", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));

  auto modifier_state_with_control = mojom::ModifierState::New();
  modifier_state_with_control->control = true;
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "a",
                                   modifier_state_with_control->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, false);
  ASSERT_EQ(0U, response.operations.size());
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleAltShortCut) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  mojom::KeypressResponseForRulebased response;
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "AltLeft",
                                   "Alt", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));

  auto new_modifier_state = mojom::ModifierState::New();
  new_modifier_state->alt = true;
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "a",
                                   std::move(new_modifier_state)),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, false);
  ASSERT_EQ(0U, response.operations.size());
}

TEST_F(ImeServiceTest, RuleBasedHandlesAltRight) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  mojom::KeypressResponseForRulebased response;
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "AltRight",
                                   "Alt", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));

  auto modifier_state_with_alt = mojom::ModifierState::New();
  modifier_state_with_alt->alt = true;
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "a",
                                   modifier_state_with_alt->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, true);
  ASSERT_EQ(1U, response.operations.size());
}

// Tests that the rule-based Arabic keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedArabic) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test Shift+KeyA.
  mojom::KeypressResponseForRulebased response;
  auto modifier_state_with_shift = mojom::ModifierState::New();
  modifier_state_with_shift->shift = true;
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "A",
                                   modifier_state_with_shift->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, true);
  std::vector<mojom::OperationForRulebasedPtr> expected_operations;
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::COMMIT_TEXT, "\u0650")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Test KeyB
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyB", "b",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();
  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::COMMIT_TEXT, "\u0644\u0627")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Test unhandled key.
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Enter",
                                   "Enter", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();
  EXPECT_EQ(response.result, false);

  // Test keyup.
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyUp, "Enter",
                                   "Enter", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();
  EXPECT_EQ(response.result, false);

  // TODO(keithlee) Test reset function
  input_method->OnCompositionCanceledBySystem();

  // Test invalid request.
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "", "",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();
  EXPECT_EQ(response.result, false);
}

// Tests that the rule-based DevaPhone keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedDevaPhone) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:deva_phone", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  mojom::KeypressResponseForRulebased response;
  std::vector<mojom::OperationForRulebasedPtr> expected_operations;

  // Test KeyN.
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyN", "n",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::SET_COMPOSITION, "\u0928")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Backspace.
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Backspace",
                                   "Backspace", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::SET_COMPOSITION, "")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // KeyN + KeyC.
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyN", "n",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyC", "c",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::SET_COMPOSITION,
      "\u091e\u094d\u091a")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Space.
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Space", " ",
                                   mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

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
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:deva_phone", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  auto modifier_state_with_shift = mojom::ModifierState::New();
  modifier_state_with_shift->shift = true;

  mojom::KeypressResponseForRulebased response;

  // Test Shift+Quote ('"').
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Quote", "\"",
                                   modifier_state_with_shift->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, true);
  ASSERT_EQ(1U, response.operations.size());
  EXPECT_EQ(mojom::OperationMethodForRulebased::COMMIT_TEXT,
            response.operations[0]->method);
  EXPECT_EQ("\"", response.operations[0]->arguments);

  // Backslash.
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Backslash",
                                   "\\", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, true);
  ASSERT_EQ(1U, response.operations.size());
  EXPECT_EQ(mojom::OperationMethodForRulebased::COMMIT_TEXT,
            response.operations[0]->method);
  EXPECT_EQ("\\", response.operations[0]->arguments);

  // Shift+Comma ('<')
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "Comma", "<",
                                   modifier_state_with_shift->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, true);
  ASSERT_EQ(1U, response.operations.size());
  EXPECT_EQ(mojom::OperationMethodForRulebased::COMMIT_TEXT,
            response.operations[0]->method);
  EXPECT_EQ("<", response.operations[0]->arguments);
}

// Tests that AltGr works with rule-based. See crbug.com/1035145.
TEST_F(ImeServiceTest, KhmerKeyboardAltGr) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:km", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test AltRight+KeyA.
  // We do not support AltGr for rule-based. We treat AltRight as AltGr.
  mojom::KeypressResponseForRulebased response;
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "AltRight",
                                   "Alt", mojom::ModifierState::New()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));

  auto modifier_state_with_alt = mojom::ModifierState::New();
  modifier_state_with_alt->alt = true;
  input_method->ProcessKeypressForRulebased(
      mojom::PhysicalKeyEvent::New(mojom::KeyEventType::kKeyDown, "KeyA", "a",
                                   modifier_state_with_alt->Clone()),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  input_method.FlushForTesting();

  EXPECT_EQ(response.result, true);
  ASSERT_EQ(1U, response.operations.size());
  EXPECT_EQ(mojom::OperationMethodForRulebased::COMMIT_TEXT,
            response.operations[0]->method);
  EXPECT_EQ("+", response.operations[0]->arguments);
}

}  // namespace ime
}  // namespace chromeos
