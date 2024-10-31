// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/on_device_translation_service.h"

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "components/services/on_device_translation/test/test_util.h"
#include "components/services/on_device_translation/translate_kit_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {
namespace {

class OnDeviceTranslationServiceTest : public testing::Test {
 public:
  OnDeviceTranslationServiceTest()
      : service_impl_(OnDeviceTranslationService::CreateForTesting(
            service_remote_.BindNewPipeAndPassReceiver(),
            TranslateKitClient::CreateForTest(GetMockLibraryPath()))) {}
  ~OnDeviceTranslationServiceTest() override = default;

  OnDeviceTranslationServiceTest(const OnDeviceTranslationServiceTest&) =
      delete;
  OnDeviceTranslationServiceTest& operator=(
      const OnDeviceTranslationServiceTest&) = delete;

 protected:
  // Sends a config to the service and returns a fake file operation proxy that
  // needs to be kept alive for the duration of the test.
  std::unique_ptr<FakeFileOperationProxy, base::OnTaskRunnerDeleter> SendConfig(
      const std::vector<std::pair<std::string, std::string>>& packages,
      const std::vector<TestFile>& files) {
    auto config = mojom::OnDeviceTranslationServiceConfig::New();

    for (const auto& pair : packages) {
      config->packages.push_back(mojom::OnDeviceTranslationLanguagePackage::New(
          pair.first, pair.second));
    }
    auto file_operation_proxy = FakeFileOperationProxy::Create(
        config->file_operation_proxy.InitWithNewPipeAndPassReceiver(), files);
    service_remote_->SetServiceConfig(std::move(config));
    return file_operation_proxy;
  }

  // Returns true if the service can translate the given language pair.
  bool CanTranslate(const std::string& source_lang,
                    const std::string& target_lang) {
    bool result_out = false;
    base::RunLoop run_loop;
    service_remote_->CanTranslate(source_lang, target_lang,
                                  base::BindLambdaForTesting([&](bool result) {
                                    result_out = result;
                                    run_loop.Quit();
                                  }));
    run_loop.Run();
    return result_out;
  }

  // Returns a remote to a translator for the given language pair or an invalid
  // remote if the language pair is not supported.
  mojo::Remote<mojom::Translator> CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang) {
    mojo::Remote<mojom::Translator> translator_remote;
    base::RunLoop run_loop;
    service_remote_->CreateTranslator(
        source_lang, target_lang,
        base::BindLambdaForTesting(
            [&](mojo::PendingRemote<mojom::Translator> result) {
              if (result) {
                translator_remote =
                    mojo::Remote<mojom::Translator>(std::move(result));
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return translator_remote;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::OnDeviceTranslationService> service_remote_;
  std::unique_ptr<OnDeviceTranslationService> service_impl_;
};

// Tests that the CanTranslate method returns true for a language pair that is
// supported by the service.
TEST_F(OnDeviceTranslationServiceTest, CanTranslateSuccess) {
  auto file_operation_proxy =
      SendConfig({{"en", "ja"}}, {
                                     {"0/dict.dat", "En to Ja - "},
                                 });
  EXPECT_TRUE(CanTranslate("en", "ja"));
}

// Tests that the CanTranslate method returns false for a language pair that is
// not supported by the service.
TEST_F(OnDeviceTranslationServiceTest, CanTranslateFaulure) {
  auto file_operation_proxy =
      SendConfig({{"en", "ja"}}, {
                                     {"0/dict.dat", "En to Ja - "},
                                 });
  EXPECT_FALSE(CanTranslate("en", "es"));
}

// Tests that the CreateTranslator method returns a valid remote to a translator
// for a language pair that is supported by the service. And the translator
// should be able to translate a text.
TEST_F(OnDeviceTranslationServiceTest, CreateTranslatorSuccess) {
  auto file_operation_proxy =
      SendConfig({{"en", "ja"}}, {
                                     {"0/dict.dat", "En to Ja - "},
                                 });
  mojo::Remote<mojom::Translator> translator_remote =
      CreateTranslator("en", "ja");
  ASSERT_TRUE(translator_remote);

  base::RunLoop run_loop;
  translator_remote->Translate(
      "test",
      base::BindLambdaForTesting([&](const std::optional<std::string>& output) {
        EXPECT_EQ(output, "En to Ja - test");
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Tests that the CreateTranslator method returns a null remote when the
// language pair is not supported by the service.
TEST_F(OnDeviceTranslationServiceTest, CreateTranslatorFaulure) {
  auto file_operation_proxy =
      SendConfig({{"en", "ja"}}, {
                                     {"0/dict.dat", "En to Ja - "},
                                 });
  EXPECT_FALSE(CreateTranslator("en", "es"));
}

// Tests that the Translate method returns an empty string when the translator
// fails to translate the text.
TEST_F(OnDeviceTranslationServiceTest, TranslateFailure) {
  auto file_operation_proxy =
      SendConfig({{"en", "ja"}}, {
                                     {"0/dict.dat", "En to Ja - "},
                                 });
  mojo::Remote<mojom::Translator> translator_remote =
      CreateTranslator("en", "ja");
  ASSERT_TRUE(translator_remote);

  base::RunLoop run_loop;
  translator_remote->Translate(
      "SIMULATE_ERROR",
      base::BindLambdaForTesting([&](const std::optional<std::string>& output) {
        EXPECT_FALSE(output.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace on_device_translation
