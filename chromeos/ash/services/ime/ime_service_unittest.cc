// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/ime_service.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/mock_input_channel.h"
#include "chromeos/ash/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/japanese_settings.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace ash {
namespace ime {

namespace {

const char kInvalidImeSpec[] = "ime_spec_never_support";
constexpr char kValidImeSpec[] = "valid_spec";
const std::vector<uint8_t> extra{0x66, 0x77, 0x88};

void ConnectCallback(bool* success, bool result) {
  *success = result;
}

class TestDecoderState;

// The fake decoder state has to be available globally because
// ImeSharedLibraryWrapper::EntryPoints is a list of stateless C functions, so
// the only way to have a stateful fake is to have a global reference to it.
TestDecoderState* g_test_decoder_state = nullptr;

mojo::ScopedMessagePipeHandle MessagePipeHandleFromInt(uint32_t handle) {
  return mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(handle));
}

class TestDecoderState : public mojom::ConnectionFactory {
 public:
  bool InitializeConnectionFactory(uint32_t receiver_pipe_handle) {
    connection_factory_.reset();
    connection_factory_.Bind(mojo::PendingReceiver<mojom::ConnectionFactory>(
        MessagePipeHandleFromInt(receiver_pipe_handle)));
    connection_factory_.set_disconnect_handler(
        base::BindOnce(&mojo::Receiver<mojom::ConnectionFactory>::reset,
                       base::Unretained(&connection_factory_)));
    return true;
  }

  bool IsConnected() { return connection_factory_.is_bound(); }

  // mojom::ConnectionFactory overrides
  void ConnectToInputMethod(
      const std::string& ime_spec,
      mojo::PendingAssociatedReceiver<ime::mojom::InputMethod> input_method,
      mojo::PendingAssociatedRemote<ime::mojom::InputMethodHost>
          input_method_host,
      mojom::InputMethodSettingsPtr settings,
      ConnectToInputMethodCallback callback) override {
    std::move(callback).Run(/*bound=*/false);
  }
  void ConnectToJapaneseDecoder(
      mojo::PendingAssociatedReceiver<ime::mojom::JapaneseDecoder>
          japanese_decoder,
      ConnectToJapaneseDecoderCallback callback) override {
    std::move(callback).Run(/*bound=*/false);
  }

 private:
  mojo::Receiver<ime::mojom::ConnectionFactory> connection_factory_{this};
};

class TestImeSharedLibraryWrapper : public ImeSharedLibraryWrapper {
 public:
  static TestImeSharedLibraryWrapper* GetInstance() {
    static base::NoDestructor<TestImeSharedLibraryWrapper> instance;
    return instance.get();
  }

  absl::optional<ImeSharedLibraryWrapper::EntryPoints>
  MaybeLoadThenReturnEntryPoints() override {
    return entry_points_;
  }

  void ResetState() {
    delete g_test_decoder_state;
    g_test_decoder_state = new TestDecoderState();

    entry_points_ = {
        .init_proto_mode = [](ImeCrosPlatform* platform) {},
        .close_proto_mode = []() {},
        .supports =
            [](const char* ime_spec) {
              return strcmp(kInvalidImeSpec, ime_spec) != 0;
            },
        .activate_ime = [](const char* ime_spec,
                           ImeClientDelegate* delegate) { return true; },
        .process = [](const uint8_t* data, size_t size) {},
        .init_mojo_mode = [](ImeCrosPlatform* platform) {},
        .close_mojo_mode = []() {},
        .initialize_connection_factory =
            [](uint32_t receiver_pipe_handle) {
              return g_test_decoder_state->InitializeConnectionFactory(
                  receiver_pipe_handle);
            },
        .is_input_method_connected =
            []() { return g_test_decoder_state->IsConnected(); },
    };
  }

 private:
  friend class base::NoDestructor<TestImeSharedLibraryWrapper>;

  explicit TestImeSharedLibraryWrapper() { ResetState(); }

  ~TestImeSharedLibraryWrapper() override = default;

  absl::optional<ImeSharedLibraryWrapper::EntryPoints> entry_points_;
};

struct MockInputMethodHost : public mojom::InputMethodHost {
  void CommitText(const std::u16string& text,
                  mojom::CommitTextCursorBehavior cursor_behavior) override {
    last_commit = text;
  }
  void DEPRECATED_SetComposition(
      const std::u16string& text,
      std::vector<mojom::CompositionSpanPtr> spans) override {
    last_composition = text;
  }
  void SetComposition(const std::u16string& text,
                      std::vector<mojom::CompositionSpanPtr> spans,
                      uint32_t new_cursor_position) override {
    last_composition = text;
  }
  void SetCompositionRange(uint32_t start_index, uint32_t end_index) override {}
  void FinishComposition() override {}
  void DeleteSurroundingText(uint32_t num_before_cursor,
                             uint32_t num_after_cursor) override {}
  void HandleAutocorrect(mojom::AutocorrectSpanPtr autocorrect_span) override {}
  void RequestSuggestions(mojom::SuggestionsRequestPtr request,
                          RequestSuggestionsCallback callback) override {}
  void DisplaySuggestions(
      const std::vector<AssistiveSuggestion>& suggestions) override {}
  void UpdateCandidatesWindow(mojom::CandidatesWindowPtr window) override {}
  void RecordUkm(mojom::UkmEntryPtr entry) override {}
  void ReportKoreanAction(mojom::KoreanAction action) override {}
  void ReportKoreanSettings(mojom::KoreanSettingsPtr settings) override {}
  void ReportSuggestionOpportunity(AssistiveSuggestionMode mode) override {}
  void UpdateQuickSettings(
      mojom::InputMethodQuickSettingsPtr settings) override {}

  std::u16string last_commit;
  std::u16string last_composition;
};

class TestFieldTrialParamsRetriever : public FieldTrialParamsRetriever {
 public:
  explicit TestFieldTrialParamsRetriever() = default;
  ~TestFieldTrialParamsRetriever() override = default;
  TestFieldTrialParamsRetriever(const TestFieldTrialParamsRetriever&) = delete;
  TestFieldTrialParamsRetriever& operator=(
      const TestFieldTrialParamsRetriever&) = delete;

  std::string GetFieldTrialParamValueByFeature(
      const base::Feature& feature,
      const std::string& param_name) override {
    return base::StrCat({feature.name, "::", param_name});
  }
};

class ImeServiceTest : public testing::Test, public mojom::InputMethodHost {
 public:
  ImeServiceTest() = default;

  ImeServiceTest(const ImeServiceTest&) = delete;
  ImeServiceTest& operator=(const ImeServiceTest&) = delete;

  ~ImeServiceTest() override = default;

  void CommitText(const std::u16string& text,
                  mojom::CommitTextCursorBehavior cursor_behavior) override {}
  void DEPRECATED_SetComposition(
      const std::u16string& text,
      std::vector<mojom::CompositionSpanPtr> spans) override {}
  void SetComposition(const std::u16string& text,
                      std::vector<mojom::CompositionSpanPtr> spans,
                      uint32_t new_cursor_position) override {}
  void SetCompositionRange(uint32_t start_index, uint32_t end_index) override {}
  void FinishComposition() override {}
  void DeleteSurroundingText(uint32_t num_before_cursor,
                             uint32_t num_after_cursor) override {}
  void HandleAutocorrect(mojom::AutocorrectSpanPtr autocorrect_span) override {}
  void RequestSuggestions(mojom::SuggestionsRequestPtr request,
                          RequestSuggestionsCallback callback) override {}
  void DisplaySuggestions(
      const std::vector<AssistiveSuggestion>& suggestions) override {}
  void UpdateCandidatesWindow(mojom::CandidatesWindowPtr window) override {}
  void RecordUkm(mojom::UkmEntryPtr entry) override {}
  void ReportKoreanAction(mojom::KoreanAction action) override {}
  void ReportKoreanSettings(mojom::KoreanSettingsPtr settings) override {}
  void ReportSuggestionOpportunity(AssistiveSuggestionMode mode) override {}
  void UpdateQuickSettings(
      mojom::InputMethodQuickSettingsPtr settings) override {}

 protected:
  void SetUp() override {
    service_ = std::make_unique<ImeService>(
        remote_service_.BindNewPipeAndPassReceiver(),
        TestImeSharedLibraryWrapper::GetInstance(),
        std::make_unique<TestFieldTrialParamsRetriever>());
    remote_service_->BindInputEngineManager(
        remote_manager_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    service_.reset();
    TestImeSharedLibraryWrapper::GetInstance()->ResetState();
  }

  mojo::Remote<mojom::ImeService> remote_service_;
  mojo::Remote<mojom::InputEngineManager> remote_manager_;

 protected:
  std::unique_ptr<ImeService> service_;

 private:
  base::test::TaskEnvironment task_environment_;
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
       ConnectToImeEngineCannotConnectIfConnectionFactoryIsConnected) {
  bool success1, success2 = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::ConnectionFactory> connection_factory;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->InitializeConnectionFactory(
      connection_factory.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kImeServiceLib,
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_.FlushForTesting();

  // The second connection should have failed.
  EXPECT_TRUE(success1);
  EXPECT_FALSE(success2);
  EXPECT_TRUE(connection_factory.is_connected());
  EXPECT_FALSE(remote_engine.is_connected());
}

TEST_F(ImeServiceTest,
       ConnectToImeEngineCanConnectIfConnectionFactoryIsDisconnected) {
  bool success1, success2 = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::ConnectionFactory> connection_factory;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->InitializeConnectionFactory(
      connection_factory.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kImeServiceLib,
      base::BindOnce(&ConnectCallback, &success1));
  connection_factory.reset();
  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_.FlushForTesting();

  // The second connection should have succeed since the first connection was
  // disconnected.
  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);
  EXPECT_FALSE(connection_factory.is_bound());
  EXPECT_TRUE(remote_engine.is_connected());
}

TEST_F(ImeServiceTest, InitializeConnectionFactoryCanOverrideAnyConnection) {
  bool success1, success2, success3 = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::ConnectionFactory> connection_factory1,
      connection_factory2;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_->InitializeConnectionFactory(
      connection_factory1.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kImeServiceLib,
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_->InitializeConnectionFactory(
      connection_factory2.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kImeServiceLib,
      base::BindOnce(&ConnectCallback, &success3));
  remote_manager_.FlushForTesting();

  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);
  EXPECT_TRUE(success3);
  EXPECT_FALSE(remote_engine.is_connected());
  EXPECT_FALSE(connection_factory1.is_connected());
  EXPECT_TRUE(connection_factory2.is_connected());
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleModifierKeys) {
  bool success1 = false;
  bool success2 = false;
  mojo::Remote<mojom::ConnectionFactory> connection_factory;
  mojo::PendingAssociatedRemote<mojom::InputMethodHost> host_remote;
  mojo::AssociatedRemote<mojom::InputMethod> input_method;
  mojo::AssociatedReceiver<mojom::InputMethodHost> host(this);

  remote_manager_->InitializeConnectionFactory(
      connection_factory.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kRulebasedEngine,
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success1);

  host.Bind(host_remote.InitWithNewEndpointAndPassReceiver());
  connection_factory->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewEndpointAndPassReceiver(),
      std::move(host_remote), nullptr,
      base::BindOnce(&ConnectCallback, &success2));
  connection_factory.FlushForTesting();
  EXPECT_TRUE(success2);

  constexpr std::pair<mojom::NamedDomKey, mojom::DomCode> kModifierKeys[] = {
      {mojom::NamedDomKey::kShift, mojom::DomCode::kShiftLeft},
      {mojom::NamedDomKey::kShift, mojom::DomCode::kShiftRight},
      {mojom::NamedDomKey::kAlt, mojom::DomCode::kAltLeft},
      {mojom::NamedDomKey::kAlt, mojom::DomCode::kAltRight},
      {mojom::NamedDomKey::kAltGraph, mojom::DomCode::kAltRight},
      {mojom::NamedDomKey::kCapsLock, mojom::DomCode::kCapsLock},
      {mojom::NamedDomKey::kControl, mojom::DomCode::kControlLeft},
      {mojom::NamedDomKey::kControl, mojom::DomCode::kControlRight}};
  for (const auto& modifier_key : kModifierKeys) {
    input_method->ProcessKeyEvent(
        mojom::PhysicalKeyEvent::New(
            mojom::KeyEventType::kKeyDown,
            mojom::DomKey::NewNamedKey(modifier_key.first), modifier_key.second,
            mojom::ModifierState::New()),
        base::BindLambdaForTesting([](mojom::KeyEventResult result) {
          EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
        }));
    input_method.FlushForTesting();
  }
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleCtrlShortCut) {
  bool success1 = false;
  bool success2 = false;
  mojo::Remote<mojom::ConnectionFactory> connection_factory;
  mojo::PendingAssociatedRemote<mojom::InputMethodHost> host_remote;
  mojo::AssociatedRemote<mojom::InputMethod> input_method;
  mojo::AssociatedReceiver<mojom::InputMethodHost> host(this);

  remote_manager_->InitializeConnectionFactory(
      connection_factory.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kRulebasedEngine,
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success1);

  host.Bind(host_remote.InitWithNewEndpointAndPassReceiver());
  connection_factory->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewEndpointAndPassReceiver(),
      std::move(host_remote), nullptr,
      base::BindOnce(&ConnectCallback, &success2));
  connection_factory.FlushForTesting();
  EXPECT_TRUE(success2);

  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kControl),
          mojom::DomCode::kControlLeft,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));

  auto modifier_state_with_control = mojom::ModifierState::New();
  modifier_state_with_control->control = true;
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('a'),
          mojom::DomCode::kKeyA, modifier_state_with_control->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));
  input_method.FlushForTesting();
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleAltShortCut) {
  bool success1 = false;
  bool success2 = false;
  mojo::Remote<mojom::ConnectionFactory> connection_factory;
  mojo::PendingAssociatedRemote<mojom::InputMethodHost> host_remote;
  mojo::AssociatedRemote<mojom::InputMethod> input_method;
  mojo::AssociatedReceiver<mojom::InputMethodHost> host(this);

  remote_manager_->InitializeConnectionFactory(
      connection_factory.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kRulebasedEngine,
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success1);

  host.Bind(host_remote.InitWithNewEndpointAndPassReceiver());
  connection_factory->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewEndpointAndPassReceiver(),
      std::move(host_remote), nullptr,
      base::BindOnce(&ConnectCallback, &success2));
  connection_factory.FlushForTesting();
  EXPECT_TRUE(success2);

  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kAlt),
          mojom::DomCode::kAltLeft,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));

  auto new_modifier_state = mojom::ModifierState::New();
  new_modifier_state->alt = true;
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('a'),
          mojom::DomCode::kKeyA, std::move(new_modifier_state)),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));
  input_method.FlushForTesting();
}

TEST_F(ImeServiceTest, RuleBasedHandlesAltRight) {
  bool success1 = false;
  bool success2 = false;
  mojo::Remote<mojom::ConnectionFactory> connection_factory;
  mojo::PendingAssociatedRemote<mojom::InputMethodHost> host_remote;
  MockInputMethodHost mock_host;
  mojo::AssociatedRemote<mojom::InputMethod> input_method;
  mojo::AssociatedReceiver<mojom::InputMethodHost> host(&mock_host);

  remote_manager_->InitializeConnectionFactory(
      connection_factory.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kRulebasedEngine,
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success1);

  host.Bind(host_remote.InitWithNewEndpointAndPassReceiver());
  connection_factory->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewEndpointAndPassReceiver(),
      std::move(host_remote), nullptr,
      base::BindOnce(&ConnectCallback, &success2));
  connection_factory.FlushForTesting();
  EXPECT_TRUE(success2);

  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kAlt),
          mojom::DomCode::kAltRight,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));

  auto modifier_state_with_alt = mojom::ModifierState::New();
  modifier_state_with_alt->alt = true;
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('a'),
          mojom::DomCode::kKeyA, modifier_state_with_alt->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_FALSE(mock_host.last_commit.empty());
      }));
  input_method.FlushForTesting();
}

// Tests that the rule-based Arabic keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedArabic) {
  bool success1 = false;
  bool success2 = false;
  mojo::Remote<mojom::ConnectionFactory> connection_factory;
  mojo::PendingAssociatedRemote<mojom::InputMethodHost> host_remote;
  MockInputMethodHost mock_host;
  mojo::AssociatedRemote<mojom::InputMethod> input_method;
  mojo::AssociatedReceiver<mojom::InputMethodHost> host(&mock_host);

  remote_manager_->InitializeConnectionFactory(
      connection_factory.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kRulebasedEngine,
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success1);

  host.Bind(host_remote.InitWithNewEndpointAndPassReceiver());
  connection_factory->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewEndpointAndPassReceiver(),
      std::move(host_remote), nullptr,
      base::BindOnce(&ConnectCallback, &success2));
  connection_factory.FlushForTesting();
  EXPECT_TRUE(success2);

  // Test Shift+KeyA.
  auto modifier_state_with_shift = mojom::ModifierState::New();
  modifier_state_with_shift->shift = true;
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('A'),
          mojom::DomCode::kKeyA, modifier_state_with_shift->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"\u0650");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();

  // Test KeyB
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('b'),
          mojom::DomCode::kKeyB, mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"\u0644\u0627");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();

  // Test unhandled key.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kEnter),
          mojom::DomCode::kEnter,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));
  input_method.FlushForTesting();

  // Test keyup.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyUp,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kEnter),
          mojom::DomCode::kEnter,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));
  input_method.FlushForTesting();

  // TODO(keithlee) Test reset function
  input_method->OnCompositionCanceledBySystem();
}

// Tests that the rule-based DevaPhone keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedDevaPhone) {
  bool success1 = false;
  bool success2 = false;
  mojo::Remote<mojom::ConnectionFactory> connection_factory;
  mojo::PendingAssociatedRemote<mojom::InputMethodHost> host_remote;
  MockInputMethodHost mock_host;
  mojo::AssociatedRemote<mojom::InputMethod> input_method;
  mojo::AssociatedReceiver<mojom::InputMethodHost> host(&mock_host);

  remote_manager_->InitializeConnectionFactory(
      connection_factory.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kRulebasedEngine,
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success1);

  host.Bind(host_remote.InitWithNewEndpointAndPassReceiver());
  connection_factory->ConnectToInputMethod(
      "m17n:deva_phone", input_method.BindNewEndpointAndPassReceiver(),
      std::move(host_remote), nullptr,
      base::BindOnce(&ConnectCallback, &success2));
  connection_factory.FlushForTesting();
  EXPECT_TRUE(success2);

  // Test KeyN.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('n'),
          mojom::DomCode::kKeyN, mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_TRUE(mock_host.last_commit.empty());
        EXPECT_EQ(mock_host.last_composition, u"\u0928");
      }));
  input_method.FlushForTesting();

  // Backspace.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kBackspace),
          mojom::DomCode::kBackspace,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_TRUE(mock_host.last_commit.empty());
        EXPECT_EQ(mock_host.last_composition, u"");
      }));
  input_method.FlushForTesting();

  // KeyN + KeyC.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('n'),
          mojom::DomCode::kKeyN, mojom::ModifierState::New()),
      base::DoNothing());
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('c'),
          mojom::DomCode::kKeyC, mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_composition, u"\u091e\u094d\u091a");
      }));
  input_method.FlushForTesting();

  // Space.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint(' '),
          mojom::DomCode::kSpace, mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_composition, u"\u091e\u094d\u091a");
      }));
  input_method.FlushForTesting();
}

// Tests escapable characters. See https://crbug.com/1014384.
TEST_F(ImeServiceTest, RuleBasedDoesNotEscapeCharacters) {
  bool success1 = false;
  bool success2 = false;
  mojo::Remote<mojom::ConnectionFactory> connection_factory;
  mojo::PendingAssociatedRemote<mojom::InputMethodHost> host_remote;
  MockInputMethodHost mock_host;
  mojo::AssociatedRemote<mojom::InputMethod> input_method;
  mojo::AssociatedReceiver<mojom::InputMethodHost> host(&mock_host);

  remote_manager_->InitializeConnectionFactory(
      connection_factory.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kRulebasedEngine,
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success1);

  host.Bind(host_remote.InitWithNewEndpointAndPassReceiver());
  connection_factory->ConnectToInputMethod(
      "m17n:deva_phone", input_method.BindNewEndpointAndPassReceiver(),
      std::move(host_remote), nullptr,
      base::BindOnce(&ConnectCallback, &success2));
  connection_factory.FlushForTesting();
  EXPECT_TRUE(success2);

  auto modifier_state_with_shift = mojom::ModifierState::New();
  modifier_state_with_shift->shift = true;

  // Test Shift+Quote ('"').
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('"'),
          mojom::DomCode::kQuote, modifier_state_with_shift->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"\"");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();

  // Backslash.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('\\'),
          mojom::DomCode::kBackslash, mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"\\");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();

  // Shift+Comma ('<')
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('<'),
          mojom::DomCode::kComma, modifier_state_with_shift->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"<");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();
}

// Tests that AltGr works with rule-based. See crbug.com/1035145.
TEST_F(ImeServiceTest, KhmerKeyboardAltGr) {
  bool success1 = false;
  bool success2 = false;
  mojo::Remote<mojom::ConnectionFactory> connection_factory;
  mojo::PendingAssociatedRemote<mojom::InputMethodHost> host_remote;
  MockInputMethodHost mock_host;
  mojo::AssociatedRemote<mojom::InputMethod> input_method;
  mojo::AssociatedReceiver<mojom::InputMethodHost> host(&mock_host);

  remote_manager_->InitializeConnectionFactory(
      connection_factory.BindNewPipeAndPassReceiver(),
      mojom::ConnectionTarget::kRulebasedEngine,
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success1);

  host.Bind(host_remote.InitWithNewEndpointAndPassReceiver());
  connection_factory->ConnectToInputMethod(
      "m17n:km", input_method.BindNewEndpointAndPassReceiver(),
      std::move(host_remote), nullptr,
      base::BindOnce(&ConnectCallback, &success2));
  connection_factory.FlushForTesting();
  EXPECT_TRUE(success2);

  // Test AltRight+KeyA.
  // We do not support AltGr for rule-based. We treat AltRight as AltGr.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kAlt),
          mojom::DomCode::kAltRight,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));

  auto modifier_state_with_alt = mojom::ModifierState::New();
  modifier_state_with_alt->alt = true;
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('a'),
          mojom::DomCode::kKeyA, modifier_state_with_alt->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"+");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();
}

TEST_F(ImeServiceTest, GetFieldTrialParamValueByFeatureNonConsidered) {
  const char* value = service_->GetFieldTrialParamValueByFeature(
      "non-considered-feature", "param-name");

  EXPECT_STREQ(value, "");
  delete[] value;
}

TEST_F(ImeServiceTest, GetFieldTrialParamValueByFeatureConsidered) {
  const char* value = service_->GetFieldTrialParamValueByFeature(
      "AutocorrectParamsTuning", "param-name");

  EXPECT_STREQ(value, "AutocorrectParamsTuning::param-name");
  delete[] value;
}

}  // namespace ime
}  // namespace ash
