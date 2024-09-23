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
  void Unused(
      mojo::PendingAssociatedReceiver<ime::mojom::JpUnused> japanese_decoder,
      UnusedCallback callback) override {
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

  std::optional<ImeSharedLibraryWrapper::EntryPoints>
  MaybeLoadThenReturnEntryPoints() override {
    return entry_points_;
  }

  void ResetState() {
    delete g_test_decoder_state;
    g_test_decoder_state = new TestDecoderState();

    entry_points_ = {
        .init_proto_mode = [](ImeCrosPlatform* platform) {},
        .close_proto_mode = []() {},
        .proto_mode_supports =
            [](const char* ime_spec) {
              return strcmp(kInvalidImeSpec, ime_spec) != 0;
            },
        .proto_mode_activate_ime =
            [](const char* ime_spec, ImeClientDelegate* delegate) {
              return true;
            },
        .proto_mode_process = [](const uint8_t* data, size_t size) {},
        .init_mojo_mode = [](ImeCrosPlatform* platform) {},
        .close_mojo_mode = []() {},
        .mojo_mode_initialize_connection_factory =
            [](uint32_t receiver_pipe_handle) {
              return g_test_decoder_state->InitializeConnectionFactory(
                  receiver_pipe_handle);
            },
        .mojo_mode_is_input_method_connected =
            []() { return g_test_decoder_state->IsConnected(); },
    };
  }

 private:
  friend class base::NoDestructor<TestImeSharedLibraryWrapper>;

  explicit TestImeSharedLibraryWrapper() { ResetState(); }

  ~TestImeSharedLibraryWrapper() override = default;

  std::optional<ImeSharedLibraryWrapper::EntryPoints> entry_points_;
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
  void ReplaceSurroundingText(uint32_t num_before_cursor,
                              uint32_t num_after_cursor,
                              const std::u16string& text) override {}
  void HandleAutocorrect(mojom::AutocorrectSpanPtr autocorrect_span) override {}
  void RequestSuggestions(mojom::SuggestionsRequestPtr request,
                          RequestSuggestionsCallback callback) override {}
  void DisplaySuggestions(
      const std::vector<AssistiveSuggestion>& suggestions,
      const std::optional<SuggestionsTextContext>& context) override {}
  void UpdateCandidatesWindow(mojom::CandidatesWindowPtr window) override {}
  void RecordUkm(mojom::UkmEntryPtr entry) override {}
  void DEPRECATED_ReportKoreanAction(mojom::KoreanAction action) override {}
  void DEPRECATED_ReportKoreanSettings(
      mojom::KoreanSettingsPtr settings) override {}
  void DEPRECATED_ReportSuggestionOpportunity(
      AssistiveSuggestionMode mode) override {}
  void ReportHistogramSample(base::Histogram* histogram,
                             uint16_t value) override {}
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
  void ReplaceSurroundingText(uint32_t num_before_cursor,
                              uint32_t num_after_cursor,
                              const std::u16string& text) override {}
  void HandleAutocorrect(mojom::AutocorrectSpanPtr autocorrect_span) override {}
  void RequestSuggestions(mojom::SuggestionsRequestPtr request,
                          RequestSuggestionsCallback callback) override {}
  void DisplaySuggestions(
      const std::vector<AssistiveSuggestion>& suggestions,
      const std::optional<SuggestionsTextContext>& context) override {}
  void UpdateCandidatesWindow(mojom::CandidatesWindowPtr window) override {}
  void RecordUkm(mojom::UkmEntryPtr entry) override {}
  void DEPRECATED_ReportKoreanAction(mojom::KoreanAction action) override {}
  void DEPRECATED_ReportKoreanSettings(
      mojom::KoreanSettingsPtr settings) override {}
  void DEPRECATED_ReportSuggestionOpportunity(
      AssistiveSuggestionMode mode) override {}
  void ReportHistogramSample(base::Histogram* histogram,
                             uint16_t value) override {}
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
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_->InitializeConnectionFactory(
      connection_factory2.BindNewPipeAndPassReceiver(),
      base::BindOnce(&ConnectCallback, &success3));
  remote_manager_.FlushForTesting();

  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);
  EXPECT_TRUE(success3);
  EXPECT_FALSE(remote_engine.is_connected());
  EXPECT_FALSE(connection_factory1.is_connected());
  EXPECT_TRUE(connection_factory2.is_connected());
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
