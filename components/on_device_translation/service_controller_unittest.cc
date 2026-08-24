// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service_controller.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/public/supported_languages.h"
#include "components/on_device_translation/service/service_launcher.h"
#include "components/on_device_translation/test/fake_installer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {
namespace {

using ::testing::UnorderedElementsAre;
using CreateTranslatorError =
    OnDeviceTranslationController::CreateTranslatorError;

class MockServiceLauncher : public OnDeviceTranslationServiceLauncher {
 public:
  ~MockServiceLauncher() override = default;
  MOCK_METHOD(mojo::PendingRemote<mojom::OnDeviceTranslationService>,
              Launch,
              (std::string_view service_display_name_suffix,
               OnDeviceTranslationInstaller* installer),
              (override));
};

class FakeOnDeviceTranslationService
    : public mojom::OnDeviceTranslationService {
 public:
  FakeOnDeviceTranslationService() = default;
  ~FakeOnDeviceTranslationService() override = default;

  void SetServiceConfig(
      mojom::OnDeviceTranslationServiceConfigPtr config) override {}

  void CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<mojom::OnDeviceTranslator> receiver,
      CreateTranslatorCallback callback) override {
    std::move(callback).Run(create_translator_result_);
  }

  void CanTranslate(const std::string& source_lang,
                    const std::string& target_lang,
                    CanTranslateCallback callback) override {
    std::move(callback).Run(true);
  }

  void set_create_translator_result(mojom::CreateTranslatorResult result) {
    create_translator_result_ = result;
  }

  mojo::PendingRemote<mojom::OnDeviceTranslationService>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::OnDeviceTranslationService> receiver_{this};
  mojom::CreateTranslatorResult create_translator_result_ =
      mojom::CreateTranslatorResult::kSuccess;
};

class OnDeviceTranslationServiceControllerTest : public testing::Test {
 public:
  OnDeviceTranslationServiceControllerTest() = default;
  ~OnDeviceTranslationServiceControllerTest() override = default;

  void SetUp() override {
    fake_installer_ =
        std::make_unique<FakeOnDeviceTranslationInstaller>(base::FilePath());
    auto launcher = std::make_unique<MockServiceLauncher>();
    mock_launcher_ = launcher.get();
    service_controller_ =
        std::make_unique<OnDeviceTranslationServiceController>(
            std::move(launcher), "test_suffix", fake_installer_.get());
  }

  void TearDown() override {
    mock_launcher_ = nullptr;
    service_controller_.reset();
    fake_installer_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeOnDeviceTranslationInstaller> fake_installer_;
  raw_ptr<MockServiceLauncher> mock_launcher_;
  std::unique_ptr<OnDeviceTranslationServiceController> service_controller_;
};

TEST_F(OnDeviceTranslationServiceControllerTest,
       CanTranslateWithRegionalLanguageTags) {
  base::test::TestFuture<OnDeviceTranslationController::CanTranslateResult>
      future;
  service_controller_->CanTranslate("en-US", "es-419", future.GetCallback());
  EXPECT_EQ(future.Get(), OnDeviceTranslationController::CanTranslateResult::
                              kAfterDownloadLibraryAndLanguagePackNotReady);
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorWithRegionalLanguageTags) {
  // Before calling CreateTranslator, no language packs should be registered.
  EXPECT_TRUE(fake_installer_->RegisteredLanguagePacks().empty());

  // Call CreateTranslator with regional tags like "en-US" and "es-419".
  service_controller_->CreateTranslator("en-US", "es-419", base::DoNothing());

  // Verify that the language pack for en <-> es was registered for
  // installation.
  EXPECT_THAT(fake_installer_->RegisteredLanguagePacks(),
              UnorderedElementsAre(LanguagePackKey::kEn_Es));
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorWithNonEnglishRegionalTags) {
  EXPECT_TRUE(fake_installer_->RegisteredLanguagePacks().empty());

  // Translating between French (fr-FR) and Japanese (ja-JP) requires en-fr and
  // en-ja packs.
  service_controller_->CreateTranslator("fr-FR", "ja-JP", base::DoNothing());

  EXPECT_THAT(
      fake_installer_->RegisteredLanguagePacks(),
      UnorderedElementsAre(LanguagePackKey::kEn_Fr, LanguagePackKey::kEn_Ja));
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorFailureRecordsLanguagePairMetric) {
  base::HistogramTester histogram_tester;
  FakeOnDeviceTranslationService fake_service;
  fake_service.set_create_translator_result(
      mojom::CreateTranslatorResult::kErrorFailedToCreateTranslator);

  EXPECT_CALL(*mock_launcher_, Launch)
      .WillOnce(testing::Return(
          testing::ByMove(fake_service.BindNewPipeAndPassRemote())));

  fake_installer_->InitNow(base::DoNothing());
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      future;
  service_controller_->CreateTranslator("en", "es", future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            CreateTranslatorError::kFailedToCreateTranslator);

  histogram_tester.ExpectUniqueSample(
      "Translate.OnDeviceTranslation.CreateFailed.LanguagePair",
      static_cast<int>(SupportedLanguage::kEn) * 1000 +
          static_cast<int>(SupportedLanguage::kEs),
      1);
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorSuccessDoesNotRecordLanguagePairFailureMetric) {
  base::HistogramTester histogram_tester;
  FakeOnDeviceTranslationService fake_service;
  fake_service.set_create_translator_result(
      mojom::CreateTranslatorResult::kSuccess);

  EXPECT_CALL(*mock_launcher_, Launch)
      .WillOnce(testing::Return(
          testing::ByMove(fake_service.BindNewPipeAndPassRemote())));

  fake_installer_->InitNow(base::DoNothing());
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      future;
  service_controller_->CreateTranslator("en", "es", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  histogram_tester.ExpectTotalCount(
      "Translate.OnDeviceTranslation.CreateFailed.LanguagePair", 0);
}

}  // namespace
}  // namespace on_device_translation
