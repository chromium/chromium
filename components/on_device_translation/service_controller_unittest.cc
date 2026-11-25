// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/public/supported_languages.h"
#include "components/on_device_translation/service/service_launcher.h"
#include "components/on_device_translation/service_controller_manager.h"
#include "components/on_device_translation/test/fake_installer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"
#include "url/origin.h"

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

// Tests that a successful translator creation records the engine latency,
// split by whether the service was already running.
TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorRecordsEngineLatencyByServiceState) {
  base::HistogramTester histogram_tester;
  FakeOnDeviceTranslationService fake_service;
  EXPECT_CALL(*mock_launcher_, Launch)
      .WillOnce(testing::Return(
          testing::ByMove(fake_service.BindNewPipeAndPassRemote())));
  fake_installer_->InitNow(base::DoNothing());
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      first_future;
  service_controller_->CreateTranslator("en", "es", first_future.GetCallback());
  ASSERT_TRUE(first_future.Get().has_value());
  histogram_tester.ExpectTotalCount(
      "Translate.OnDeviceTranslation.CreateTranslator.EngineLatency."
      "ServiceStarted",
      1);
  histogram_tester.ExpectTotalCount(
      "Translate.OnDeviceTranslation.CreateTranslator.EngineLatency."
      "ServiceRunning",
      0);

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      second_future;
  service_controller_->CreateTranslator("es", "en",
                                        second_future.GetCallback());
  ASSERT_TRUE(second_future.Get().has_value());
  histogram_tester.ExpectTotalCount(
      "Translate.OnDeviceTranslation.CreateTranslator.EngineLatency."
      "ServiceStarted",
      1);
  histogram_tester.ExpectTotalCount(
      "Translate.OnDeviceTranslation.CreateTranslator.EngineLatency."
      "ServiceRunning",
      1);
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
       CreateTranslatorFailsWhenOffline) {
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      library_future;
  service_controller_->CreateTranslator("en", "es",
                                        library_future.GetCallback());
  fake_installer_->FailLibraryInstallNow(InstallError::kOffline);
  EXPECT_EQ(library_future.Get().error(), CreateTranslatorError::kOffline);

  fake_installer_->InitNow(base::DoNothing());
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      pack_future;
  service_controller_->CreateTranslator("en", "es", pack_future.GetCallback());
  fake_installer_->FailLanguagePackInstallNow(LanguagePackKey::kEn_Es,
                                              InstallError::kOffline);
  EXPECT_EQ(pack_future.Get().error(), CreateTranslatorError::kOffline);

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      data_saver_future;
  service_controller_->CreateTranslator("en", "es",
                                        data_saver_future.GetCallback());
  fake_installer_->FailLanguagePackInstallNow(LanguagePackKey::kEn_Es,
                                              InstallError::kDataSaverEnabled);
  EXPECT_EQ(data_saver_future.Get().error(),
            CreateTranslatorError::kDataSaverEnabled);

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      metered_future;
  service_controller_->CreateTranslator("en", "es",
                                        metered_future.GetCallback());
  fake_installer_->FailLanguagePackInstallNow(
      LanguagePackKey::kEn_Es, InstallError::kMeteredConnectionBlocked);
  EXPECT_EQ(metered_future.Get().error(),
            CreateTranslatorError::kMeteredConnectionBlocked);
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorRejectsPairExceedingMaxInstalledPacks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kTranslationAPI,
      {{"TranslationAPIMaxInstalledLanguagePacks", "3"}});

  fake_installer_->InitNow(base::DoNothing());
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kDe_En);
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Fr);

  // Requesting es -> ja needs 2 new packs (en-es and en-ja), which would bring
  // total packs to 4 > 3. Reject up front without registering either pack.
  base::HistogramTester histogram_tester;
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      pivot_future;
  service_controller_->CreateTranslator("es", "ja", pivot_future.GetCallback());
  ASSERT_TRUE(pivot_future.IsReady());
  EXPECT_EQ(pivot_future.Get().error(),
            CreateTranslatorError::kExceedsLanguagePackCountLimitation);
  EXPECT_THAT(
      fake_installer_->RegisteredLanguagePacks(),
      UnorderedElementsAre(LanguagePackKey::kDe_En, LanguagePackKey::kEn_Fr));
  histogram_tester.ExpectUniqueSample(
      "Translate.OnDeviceTranslation.DownloadExceedLimit.LanguagePair",
      static_cast<int>(SupportedLanguage::kEs) * 1000 +
          static_cast<int>(SupportedLanguage::kJa),
      1);
  histogram_tester.ExpectTotalCount(
      "Translate.OnDeviceTranslation.Download.LanguagePair", 0);

  // Registering a 3rd pack (en-es) brings registered packs to 3 (not yet
  // installed). A request for a 4th pack (en-ja) must still be rejected.
  service_controller_->CreateTranslator("en", "es", base::DoNothing());
  EXPECT_THAT(
      fake_installer_->RegisteredLanguagePacks(),
      UnorderedElementsAre(LanguagePackKey::kDe_En, LanguagePackKey::kEn_Fr,
                           LanguagePackKey::kEn_Es));

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      fourth_pack_future;
  service_controller_->CreateTranslator("en", "ja",
                                        fourth_pack_future.GetCallback());
  ASSERT_TRUE(fourth_pack_future.IsReady());
  EXPECT_EQ(fourth_pack_future.Get().error(),
            CreateTranslatorError::kExceedsLanguagePackCountLimitation);

  // Retrying the already-registered 3rd pack (en-es) stays at 3 and is allowed.
  fake_installer_->FailLanguagePackInstallNow(LanguagePackKey::kEn_Es);
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      retry_future;
  service_controller_->CreateTranslator("en", "es", retry_future.GetCallback());
  EXPECT_FALSE(retry_future.IsReady());
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorAllowsInstalledPacksAtOrAboveLimit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kTranslationAPI,
      {{"TranslationAPIMaxInstalledLanguagePacks", "2"}});

  FakeOnDeviceTranslationService fake_service;
  EXPECT_CALL(*mock_launcher_, Launch)
      .WillOnce(testing::Return(
          testing::ByMove(fake_service.BindNewPipeAndPassRemote())));

  fake_installer_->InitNow(base::DoNothing());
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kDe_En);
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Fr);

  // Even when 3 packs are already installed and the cap is lowered to 2, using
  // an already-installed pair (de -> fr) requires no new download and succeeds.
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      future;
  service_controller_->CreateTranslator("de", "fr", future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorAllowsRetryOfRegisteredPackAboveLimit) {
  fake_installer_->InitNow(base::DoNothing());
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kDe_En);
  fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);

  // Register `en-ja` before the cap is lowered, then fail its download so it
  // stays registered without being installed.
  service_controller_->CreateTranslator("en", "ja", base::DoNothing());
  fake_installer_->FailLanguagePackInstallNow(LanguagePackKey::kEn_Ja);
  ASSERT_EQ(fake_installer_->RegisteredLanguagePacks().size(), 3u);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kTranslationAPI,
      {{"TranslationAPIMaxInstalledLanguagePacks", "2"}});

  // Retrying the already-registered `en-ja` pack does not raise the count and
  // is allowed even above the lowered cap.
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      retry_future;
  service_controller_->CreateTranslator("en", "ja", retry_future.GetCallback());
  EXPECT_FALSE(retry_future.IsReady());

  // Requesting an unregistered pack (`en-fr`) is rejected.
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      new_pack_future;
  service_controller_->CreateTranslator("en", "fr",
                                        new_pack_future.GetCallback());
  ASSERT_TRUE(new_pack_future.IsReady());
  EXPECT_EQ(new_pack_future.Get().error(),
            CreateTranslatorError::kExceedsLanguagePackCountLimitation);
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       UnavailableWhenLibraryIsNotSupported) {
  fake_installer_->set_supported(false);
  EXPECT_CALL(*mock_launcher_, Launch).Times(0);

  base::test::TestFuture<OnDeviceTranslationController::CanTranslateResult>
      can_translate_future;
  service_controller_->CanTranslate("en", "es",
                                    can_translate_future.GetCallback());
  EXPECT_EQ(
      can_translate_future.Get(),
      OnDeviceTranslationController::CanTranslateResult::kNoUnsupportedDevice);

  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      create_future;
  service_controller_->CreateTranslator("en", "es",
                                        create_future.GetCallback());
  ASSERT_TRUE(create_future.IsReady());
  EXPECT_EQ(create_future.Get().error(),
            CreateTranslatorError::kUnsupportedDevice);

  // No language pack download starts.
  EXPECT_TRUE(fake_installer_->RegisteredLanguagePacks().empty());
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

class ServiceControllerManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_installer_ =
        std::make_unique<FakeOnDeviceTranslationInstaller>(base::FilePath());
    fake_installer_->InitNow(base::DoNothing());
    fake_installer_->InstallLanguagePackNow(LanguagePackKey::kEn_Es);
    manager_ = std::make_unique<ServiceControllerManager>(
        /*local_state=*/nullptr,
        base::BindRepeating(&ServiceControllerManagerTest::CreateLauncher,
                            base::Unretained(this)));
    manager_->SetInstallerForTesting(fake_installer_.get());
  }

  void TearDown() override {
    manager_.reset();
    fake_installer_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeOnDeviceTranslationInstaller> fake_installer_;
  std::unique_ptr<ServiceControllerManager> manager_;

 private:
  std::unique_ptr<OnDeviceTranslationServiceLauncher> CreateLauncher() {
    return std::make_unique<MockServiceLauncher>();
  }
};

// Tests that destroying a controller settles the requests that wait in it.
TEST_F(ServiceControllerManagerTest,
       DestroyingControllerRejectsPendingRequest) {
  const auto origin = url::Origin::Create(GURL("https://a.test"));
  fake_installer_->set_install_language_packs_automatically(false);
  base::test::TestFuture<base::expected<
      mojo::PendingRemote<mojom::OnDeviceTranslator>, CreateTranslatorError>>
      pending_future;
  manager_->CreateTranslator(origin, "en", "ja", pending_future.GetCallback());
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(pending_future.IsReady());

  manager_.reset();
  ASSERT_TRUE(pending_future.IsReady());
  EXPECT_EQ(pending_future.Get().error(),
            CreateTranslatorError::kExceedsServiceCountLimitation);
}

}  // namespace
}  // namespace on_device_translation
