// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service_controller.h"

#include <memory>
#include <string>
#include <vector>

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

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using CreateTranslatorError =
    OnDeviceTranslationController::CreateTranslatorError;

class MockServiceLauncher : public OnDeviceTranslationServiceLauncher {
 public:
  ~MockServiceLauncher() override = default;
  MOCK_METHOD(mojo::PendingRemote<mojom::OnDeviceTranslationService>,
              Launch,
              (std::string_view service_display_name_suffix,
               OnDeviceTranslationInstaller* installer,
               const std::vector<LanguagePackPath>& language_packs),
              (override));
  MOCK_METHOD(void,
              UpdateLanguagePacks,
              (mojom::OnDeviceTranslationService & service,
               const std::vector<LanguagePackPath>& language_packs),
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

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorFailsWhenLibraryInstallFails) {
  FakeOnDeviceTranslationService fake_service;
  EXPECT_CALL(*mock_launcher_, Launch).Times(0);
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      failed_future1;
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      failed_future2;
  service_controller_->CreateTranslator("en", "es",
                                        failed_future1.GetCallback());
  service_controller_->CreateTranslator("es", "en",
                                        failed_future2.GetCallback());
  EXPECT_FALSE(failed_future1.IsReady());

  fake_installer_->FailLibraryInstallNow();
  ASSERT_TRUE(failed_future1.IsReady());
  EXPECT_EQ(failed_future1.Get().error(),
            CreateTranslatorError::kLibraryInstallFailed);
  ASSERT_TRUE(failed_future2.IsReady());
  EXPECT_EQ(failed_future2.Get().error(),
            CreateTranslatorError::kLibraryInstallFailed);

  // A later CreateTranslator call retries the install and can succeed.
  EXPECT_CALL(*mock_launcher_, Launch)
      .WillOnce(testing::Return(
          testing::ByMove(fake_service.BindNewPipeAndPassRemote())));
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      retry_future;
  service_controller_->CreateTranslator("en", "es", retry_future.GetCallback());
  EXPECT_TRUE(retry_future.Get().has_value());
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorFailsWhenLanguagePackDownloadFails) {
  fake_installer_->InitNow(base::DoNothing());

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      en_es_future;
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      fr_ja_future;
  service_controller_->CreateTranslator("en", "es", en_es_future.GetCallback());
  service_controller_->CreateTranslator("fr", "ja", fr_ja_future.GetCallback());

  fake_installer_->FailLanguagePackInstallNow(LanguagePackKey::kEn_Es);
  ASSERT_TRUE(en_es_future.IsReady());
  EXPECT_EQ(en_es_future.Get().error(),
            CreateTranslatorError::kLanguagePackDownloadFailed);
  // The other request doesn't need the language pack that failed.
  EXPECT_FALSE(fr_ja_future.IsReady());
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorRetriesFailedLanguagePack) {
  FakeOnDeviceTranslationService fake_service;
  fake_installer_->InitNow(base::DoNothing());

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      failed_future;
  service_controller_->CreateTranslator("en", "es",
                                        failed_future.GetCallback());
  fake_installer_->FailLanguagePackInstallNow(
      LanguagePackKey::kEn_Es, InstallError::kNotEnoughDiskSpace);
  ASSERT_TRUE(failed_future.IsReady());
  EXPECT_EQ(failed_future.Get().error(),
            CreateTranslatorError::kNotEnoughDiskSpace);
  // The language pack stays registered.
  EXPECT_THAT(fake_installer_->RegisteredLanguagePacks(),
              UnorderedElementsAre(LanguagePackKey::kEn_Es));

  // The next request asks for the registered language pack again.
  EXPECT_CALL(*mock_launcher_, Launch)
      .WillOnce(testing::Return(
          testing::ByMove(fake_service.BindNewPipeAndPassRemote())));
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      retry_future;
  service_controller_->CreateTranslator("en", "es", retry_future.GetCallback());
  EXPECT_TRUE(retry_future.Get().has_value());
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       LibraryInstallFailureIgnoredWhenLibraryIsInstalled) {
  fake_installer_->InitNow(base::DoNothing());

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      future;
  // The language pack is still downloading.
  service_controller_->CreateTranslator("en", "es", future.GetCallback());
  fake_installer_->FailLibraryInstallNow();
  EXPECT_FALSE(future.IsReady());
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       NewLanguagePackIsAddedToRunningService) {
  FakeOnDeviceTranslationService fake_service;
  fake_installer_->InitNow(base::DoNothing());
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);
  EXPECT_CALL(*mock_launcher_,
              Launch(_, _, ElementsAre(Pair(LanguagePackKey::kEn_Es, _))))
      .WillOnce(testing::Return(
          testing::ByMove(fake_service.BindNewPipeAndPassRemote())));
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      en_es_future;
  service_controller_->CreateTranslator("en", "es", en_es_future.GetCallback());
  ASSERT_TRUE(en_es_future.Get().has_value());
  ASSERT_TRUE(service_controller_->IsServiceRunning());

  // A new language pack goes to the running service, after the packs that the
  // service already has.
  EXPECT_CALL(
      *mock_launcher_,
      UpdateLanguagePacks(_, ElementsAre(Pair(LanguagePackKey::kEn_Es, _),
                                         Pair(LanguagePackKey::kEn_Ja, _))));
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Ja);
  EXPECT_TRUE(service_controller_->IsServiceRunning());

  // An update of a language pack that the service has keeps the service.
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);
  EXPECT_TRUE(service_controller_->IsServiceRunning());

  // The next translator comes from the same service.
  EXPECT_CALL(*mock_launcher_, Launch).Times(0);
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      en_ja_future;
  service_controller_->CreateTranslator("en", "ja", en_ja_future.GetCallback());
  EXPECT_TRUE(en_ja_future.Get().has_value());
}

// Tests that the running service keeps the paths that it got for its packs
// when a newer version of a pack is installed. The service may have read some
// files of the old version already.
TEST_F(OnDeviceTranslationServiceControllerTest,
       RunningServiceKeepsPathsOfItsLanguagePacks) {
  FakeOnDeviceTranslationService fake_service;
  const base::FilePath old_en_es_path(FILE_PATH_LITERAL("/packs/en_es/1"));
  const base::FilePath new_en_es_path(FILE_PATH_LITERAL("/packs/en_es/2"));
  const base::FilePath en_ja_path(FILE_PATH_LITERAL("/packs/en_ja/1"));
  fake_installer_->InitNow(base::DoNothing());
  fake_installer_->SetLanguagePackPath(LanguagePackKey::kEn_Es, old_en_es_path);
  fake_installer_->SetLanguagePackPath(LanguagePackKey::kEn_Ja, en_ja_path);
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);
  EXPECT_CALL(
      *mock_launcher_,
      Launch(_, _, ElementsAre(Pair(LanguagePackKey::kEn_Es, old_en_es_path))))
      .WillOnce(testing::Return(
          testing::ByMove(fake_service.BindNewPipeAndPassRemote())));
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      future;
  service_controller_->CreateTranslator("en", "es", future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());

  // A newer version of en-es lands, then en-ja.
  fake_installer_->SetLanguagePackPath(LanguagePackKey::kEn_Es, new_en_es_path);
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);
  EXPECT_CALL(*mock_launcher_,
              UpdateLanguagePacks(
                  _, ElementsAre(Pair(LanguagePackKey::kEn_Es, old_en_es_path),
                                 Pair(LanguagePackKey::kEn_Ja, en_ja_path))));
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Ja);
  EXPECT_TRUE(service_controller_->IsServiceRunning());
}

// Tests that a new pack restarts the service, instead of crashing, when its
// path isn't known yet because the library is not ready.
TEST_F(OnDeviceTranslationServiceControllerTest,
       NewLanguagePackWithoutLibraryStopsRunningService) {
  FakeOnDeviceTranslationService fake_service;
  fake_installer_->InitNow(base::DoNothing());
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);
  EXPECT_CALL(*mock_launcher_, Launch)
      .WillOnce(testing::Return(
          testing::ByMove(fake_service.BindNewPipeAndPassRemote())));
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      future;
  service_controller_->CreateTranslator("en", "es", future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());

  fake_installer_->ResetInit();
  EXPECT_CALL(*mock_launcher_, UpdateLanguagePacks).Times(0);
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Ja);
  EXPECT_FALSE(service_controller_->IsServiceRunning());
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       UninstalledLanguagePackStopsRunningService) {
  FakeOnDeviceTranslationService fake_service;
  fake_installer_->InitNow(base::DoNothing());
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);
  EXPECT_CALL(*mock_launcher_, Launch)
      .WillOnce(testing::Return(
          testing::ByMove(fake_service.BindNewPipeAndPassRemote())));
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      future;
  service_controller_->CreateTranslator("en", "es", future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());

  EXPECT_CALL(*mock_launcher_, UpdateLanguagePacks).Times(0);
  fake_installer_->UnInstallLanguagePack(LanguagePackKey::kEn_Es);
  EXPECT_FALSE(service_controller_->IsServiceRunning());
}

}  // namespace
}  // namespace on_device_translation
