// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/language_pack_manager.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "components/session_manager/core/session_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::dlcservice::DlcState;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArg;

namespace ash::language_packs {

namespace {

constexpr char kFakeDlcId[] = "FakeDlc";
constexpr char kSupportedLocale[] = "es";
constexpr char kHistogramGetPackStateFeatureId[] =
    "ChromeOS.LanguagePacks.GetPackState.FeatureId";
constexpr char kHistogramInstallPackSuccess[] =
    "ChromeOS.LanguagePacks.InstallPack.Success";
constexpr char kHistogramInstallBasePackFeatureId[] =
    "ChromeOS.LanguagePacks.InstallBasePack.FeatureId";
constexpr char kHistogramOobeValidLocale[] =
    "ChromeOS.LanguagePacks.Oobe.ValidLocale";
constexpr char kHistogramUninstallCompleteSuccess[] =
    "ChromeOS.LanguagePacks.UninstallComplete.Success";

// We need a mock callback so that we can check that it gets called.
class CallbackForTesting {
 public:
  OnInstallCompleteCallback GetInstallCallback() {
    return base::BindOnce(&CallbackForTesting::Callback,
                          base::Unretained(this));
  }

  GetPackStateCallback GetPackStateCallback() {
    return base::BindOnce(&CallbackForTesting::Callback,
                          base::Unretained(this));
  }

  OnUninstallCompleteCallback GetRemoveCallback() {
    return base::BindOnce(&CallbackForTesting::Callback,
                          base::Unretained(this));
  }

  OnUpdatePacksForOobeCallback GetOobeCallback() {
    return base::BindOnce(&CallbackForTesting::Callback,
                          base::Unretained(this));
  }

  MOCK_METHOD(void, Callback, (const PackResult&), ());
};

class MockObserver : public LanguagePackManager::Observer {
 public:
  MOCK_METHOD(void, OnPackStateChanged, (const PackResult& pack_result));
};

// Utility function that creates a DlcState with no error, populated with id
// corresponding to German handwriting recognition and path.
DlcState CreateInstalledState() {
  DlcState output;
  output.set_state(dlcservice::DlcState_State_INSTALLED);
  output.set_id("handwriting-de");
  output.set_root_path("/path");
  return output;
}

}  // namespace

class LanguagePackManagerTest : public testing::Test {
 public:
  void SetUp() override {
    // The Fake DLC Service needs to be initialized before we instantiate
    // LanguagePackManager.
    DlcserviceClient::InitializeFake();
    dlcservice_client_ =
        static_cast<FakeDlcserviceClient*>(DlcserviceClient::Get());

    session_manager_ = std::make_unique<session_manager::SessionManager>();

    manager_ = LanguagePackManager::GetInstance();
    manager_->Initialize();
    ResetPackResult();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    manager_->ResetForTesting();
    DlcserviceClient::Shutdown();
  }

  void InstallTestCallback(const PackResult& pack_result) {
    pack_result_ = pack_result;
  }

  void GetPackStateTestCallback(const PackResult& pack_result) {
    pack_result_ = pack_result;
  }

  void RemoveTestCallback(const PackResult& pack_result) {
    pack_result_ = pack_result;
  }

  void OobeTestCallback(const PackResult& pack_result) {
    pack_result_ = pack_result;
  }

 protected:
  raw_ptr<LanguagePackManager, ExperimentalAsh> manager_;
  PackResult pack_result_;
  raw_ptr<FakeDlcserviceClient, DanglingUntriaged | ExperimentalAsh>
      dlcservice_client_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  void ResetPackResult() {
    PackResult temp = PackResult();
    pack_result_ = temp;
  }
};

TEST_F(LanguagePackManagerTest, InstallSuccessTest) {
  dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  dlcservice_client_->set_install_root_path("/path");

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kHistogramInstallPackSuccess, FeatureSuccessEnum::kHandwritingSuccess, 0);
  histogram_tester.ExpectBucketCount(
      kHistogramInstallPackSuccess, FeatureSuccessEnum::kHandwritingFailure, 0);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->InstallPack(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kNone);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kInstalled);
  EXPECT_EQ(pack_result_.path, "/path");
  EXPECT_EQ(pack_result_.feature_id, kHandwritingFeatureId);
  EXPECT_EQ(pack_result_.language_code, kSupportedLocale);

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(
      kHistogramInstallPackSuccess, FeatureSuccessEnum::kHandwritingSuccess, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramInstallPackSuccess, FeatureSuccessEnum::kHandwritingFailure, 0);
}

TEST_F(LanguagePackManagerTest, InstallFailureTest) {
  dlcservice_client_->set_install_error(dlcservice::kErrorInternal);

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kHistogramInstallPackSuccess, FeatureSuccessEnum::kHandwritingSuccess, 0);
  histogram_tester.ExpectBucketCount(
      kHistogramInstallPackSuccess, FeatureSuccessEnum::kHandwritingFailure, 0);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->InstallPack(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kOther);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kUnknown);

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(
      kHistogramInstallPackSuccess, FeatureSuccessEnum::kHandwritingSuccess, 0);
  histogram_tester.ExpectBucketCount(
      kHistogramInstallPackSuccess, FeatureSuccessEnum::kHandwritingFailure, 1);
}

TEST_F(LanguagePackManagerTest, InstallWrongIdTest) {
  // Note: no UMA metrics are reconded in this case, because there is no call to
  // DLC Service, hence no success nor failure.

  manager_->InstallPack(
      kFakeDlcId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kWrongId);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kUnknown);
}

// Check that the callback is actually called.
TEST_F(LanguagePackManagerTest, InstallCallbackTest) {
  dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  dlcservice_client_->set_install_root_path("/path");

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback, Callback(_));

  manager_->InstallPack(kFakeDlcId, kSupportedLocale,
                        callback.GetInstallCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackManagerTest, GetPackStateSuccessTest) {
  dlcservice_client_->set_get_dlc_state_error(dlcservice::kErrorNone);
  dlcservice::DlcState dlc_state;
  dlc_state.set_state(dlcservice::DlcState_State_INSTALLED);
  dlc_state.set_is_verified(true);
  dlc_state.set_root_path("/path");
  dlcservice_client_->set_dlc_state(dlc_state);

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramGetPackStateFeatureId,
                                     FeatureIdsEnum::kHandwriting, 0);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->GetPackState(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::GetPackStateTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kNone);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kInstalled);
  EXPECT_EQ(pack_result_.path, "/path");
  EXPECT_EQ(pack_result_.feature_id, kHandwritingFeatureId);
  EXPECT_EQ(pack_result_.language_code, kSupportedLocale);

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(kHistogramGetPackStateFeatureId,
                                     FeatureIdsEnum::kHandwriting, 1);
}

TEST_F(LanguagePackManagerTest, GetPackStateFailureTest) {
  dlcservice_client_->set_get_dlc_state_error(dlcservice::kErrorInternal);

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramGetPackStateFeatureId,
                                     FeatureIdsEnum::kHandwriting, 0);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->GetPackState(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::GetPackStateTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kOther);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kUnknown);

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(kHistogramGetPackStateFeatureId,
                                     FeatureIdsEnum::kHandwriting, 1);
}

TEST_F(LanguagePackManagerTest, GetPackStateWrongIdTest) {
  // Note: no UMA metrics are reconded in this case, because there is no call to
  // DLC Service, hence no success nor failure.

  manager_->GetPackState(
      kFakeDlcId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::GetPackStateTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kWrongId);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kUnknown);
}

// Check that the callback is actually called.
TEST_F(LanguagePackManagerTest, GetPackStateCallbackTest) {
  dlcservice_client_->set_get_dlc_state_error(dlcservice::kErrorNone);

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback, Callback(_));

  manager_->GetPackState(kFakeDlcId, kSupportedLocale,
                         callback.GetPackStateCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackManagerTest, RemovePackSuccessTest) {
  dlcservice_client_->set_uninstall_error(dlcservice::kErrorNone);

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramUninstallCompleteSuccess,
                                     1 /* True */, 0);
  histogram_tester.ExpectBucketCount(kHistogramUninstallCompleteSuccess,
                                     0 /* False */, 0);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->RemovePack(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::RemoveTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kNone);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kNotInstalled);
  EXPECT_EQ(pack_result_.feature_id, kHandwritingFeatureId);
  EXPECT_EQ(pack_result_.language_code, kSupportedLocale);

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(kHistogramUninstallCompleteSuccess,
                                     1 /* True */, 1);
  histogram_tester.ExpectBucketCount(kHistogramUninstallCompleteSuccess,
                                     0 /* False */, 0);
}

TEST_F(LanguagePackManagerTest, RemovePackFailureTest) {
  dlcservice_client_->set_uninstall_error(dlcservice::kErrorInternal);

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramUninstallCompleteSuccess,
                                     1 /* True */, 0);
  histogram_tester.ExpectBucketCount(kHistogramUninstallCompleteSuccess,
                                     0 /* False */, 0);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->RemovePack(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::RemoveTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kOther);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kUnknown);

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(kHistogramUninstallCompleteSuccess,
                                     1 /* True */, 0);
  histogram_tester.ExpectBucketCount(kHistogramUninstallCompleteSuccess,
                                     0 /* False */, 1);
}

TEST_F(LanguagePackManagerTest, RemovePackWrongIdTest) {
  // Note: no UMA metrics are reconded in this case, because there is no call to
  // DLC Service, hence no success nor failure.

  manager_->RemovePack(
      kFakeDlcId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::RemoveTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kWrongId);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kUnknown);
}

// Check that the callback is actually called.
TEST_F(LanguagePackManagerTest, RemovePackCallbackTest) {
  dlcservice_client_->set_uninstall_error(dlcservice::kErrorNone);

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback, Callback(_));

  manager_->RemovePack(kFakeDlcId, kSupportedLocale,
                       callback.GetRemoveCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackManagerTest, InstallObserverTest) {
  dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  dlcservice_client_->set_install_root_path("/path");
  const DlcState dlc_state = CreateInstalledState();
  MockObserver observer;

  EXPECT_CALL(observer, OnPackStateChanged(_)).Times(0);
  dlcservice_client_->NotifyObserversForTest(dlc_state);

  // Add an Observer and expect it to be notified.
  manager_->AddObserver(&observer);
  EXPECT_CALL(observer, OnPackStateChanged(_))
      .With(
          FieldsAre(AllOf(Field(&PackResult::feature_id, kHandwritingFeatureId),
                          Field(&PackResult::language_code, "de"))))
      .Times(1);
  dlcservice_client_->NotifyObserversForTest(dlc_state);

  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackManagerTest, RemoveObserverTest) {
  dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  dlcservice_client_->set_install_root_path("/path");
  const DlcState dlc_state = CreateInstalledState();
  MockObserver observer;

  // Add an Observer and expect it to be notified.
  manager_->AddObserver(&observer);
  EXPECT_CALL(observer, OnPackStateChanged(_))
      .With(
          FieldsAre(AllOf(Field(&PackResult::feature_id, kHandwritingFeatureId),
                          Field(&PackResult::language_code, "de"))))
      .Times(1);
  dlcservice_client_->NotifyObserversForTest(dlc_state);

  // Remove the Observer and there should be no more notifications.
  manager_->RemoveObserver(&observer);
  EXPECT_CALL(observer, OnPackStateChanged(_)).Times(0);
  dlcservice_client_->NotifyObserversForTest(dlc_state);

  base::RunLoop().RunUntilIdle();
}

// Check that all supported locales are available.
TEST_F(LanguagePackManagerTest, CheckAllLocalesAvailable) {
  // Handwriting Recognition.
  const std::vector<std::string> handwriting({
      "am", "ar", "be", "bg",  "bn", "ca", "cs", "da", "de", "el",    "es",
      "et", "fa", "fi", "fil", "fr", "ga", "gu", "hi", "hr", "hu",    "hy",
      "id", "is", "it", "iw",  "ja", "ka", "kk", "km", "kn", "ko",    "lo",
      "lt", "lv", "ml", "mn",  "mr", "ms", "mt", "my", "ne", "nl",    "no",
      "or", "pa", "pl", "pt",  "ro", "ru", "si", "sk", "sl", "sr",    "sv",
      "ta", "te", "th", "ti",  "tr", "uk", "ur", "vi", "zh", "zh-HK",
  });
  for (const auto& locale : handwriting) {
    EXPECT_TRUE(manager_->IsPackAvailable(kHandwritingFeatureId, locale));
  }

  // TTS.
  const std::vector<std::string> tts({
      "bn-bd", "cs-cz", "da-dk", "de-de", "el-gr",  "en-au", "en-gb",
      "en-us", "es-es", "es-us", "fi-fi", "fil-ph", "fr-fr", "hi-in",
      "hu-hu", "id-id", "it-it", "ja-jp", "km-kh",  "ko-kr", "nb-no",
      "ne-np", "nl-nl", "pl-pl", "pt-br", "si-lk",  "sk-sk", "sv-se",
      "th-th", "tr-tr", "uk-ua", "vi-vn", "yue-hk",
  });
  for (const auto& locale : tts) {
    EXPECT_TRUE(manager_->IsPackAvailable(kTtsFeatureId, locale));
  }
}

TEST_F(LanguagePackManagerTest, IsPackAvailableFalseTest) {
  // Correct ID, wrong language (Welsh).
  bool available = manager_->IsPackAvailable(kHandwritingFeatureId, "cy");
  EXPECT_FALSE(available);

  // ID doesn't exists.
  available = manager_->IsPackAvailable("foo", "fr");
  EXPECT_FALSE(available);
}

TEST_F(LanguagePackManagerTest, InstallBasePackSuccess) {
  dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  dlcservice_client_->set_install_root_path("/path");

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramInstallBasePackFeatureId,
                                     FeatureIdsEnum::kHandwriting, 0);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->InstallBasePack(
      kHandwritingFeatureId,
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kNone);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kInstalled);
  EXPECT_EQ(pack_result_.path, "/path");
  EXPECT_EQ(pack_result_.feature_id, kHandwritingFeatureId);

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(kHistogramInstallBasePackFeatureId,
                                     FeatureIdsEnum::kHandwriting, 1);
}

TEST_F(LanguagePackManagerTest, InstallBasePackFailureTestFailure) {
  dlcservice_client_->set_install_error(dlcservice::kErrorInternal);

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramInstallBasePackFeatureId,
                                     FeatureIdsEnum::kHandwriting, 0);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->InstallBasePack(
      kHandwritingFeatureId,
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kOther);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kUnknown);

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(kHistogramInstallBasePackFeatureId,
                                     FeatureIdsEnum::kHandwriting, 1);
}

// If we are not in OOBE nothing should happen.
TEST_F(LanguagePackManagerTest, UpdatePacksForOobeNotOobeTest) {
  // Set session as user logged in.
  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  dlcservice_client_->set_install_root_path("/path");

  testing::StrictMock<CallbackForTesting> callback;
  EXPECT_CALL(callback, Callback(_)).Times(0);

  manager_->UpdatePacksForOobe(kSupportedLocale, callback.GetInstallCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackManagerTest, UpdatePacksForOobeSuccessTest) {
  session_manager_->SetSessionState(session_manager::SessionState::OOBE);

  dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  dlcservice_client_->set_install_root_path("/path");

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 1 /* True */,
                                     0);
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 0 /* False */,
                                     0);

  manager_->UpdatePacksForOobe(
      "en-au", base::BindOnce(&LanguagePackManagerTest::OobeTestCallback,
                              base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kNone);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kInstalled);
  EXPECT_EQ(pack_result_.path, "/path");
  EXPECT_EQ(pack_result_.feature_id, kTtsFeatureId);
  EXPECT_EQ(pack_result_.language_code, "en-au");

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 1 /* True */,
                                     1);
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 0 /* False */,
                                     0);
}

TEST_F(LanguagePackManagerTest, UpdatePacksForOobeSuccess2Test) {
  session_manager_->SetSessionState(session_manager::SessionState::OOBE);

  dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  dlcservice_client_->set_install_root_path("/path");

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 1 /* True */,
                                     0);
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 0 /* False */,
                                     0);

  manager_->UpdatePacksForOobe(
      "it-it", base::BindOnce(&LanguagePackManagerTest::OobeTestCallback,
                              base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kNone);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kInstalled);
  EXPECT_EQ(pack_result_.path, "/path");
  EXPECT_EQ(pack_result_.feature_id, kTtsFeatureId);
  EXPECT_EQ(pack_result_.language_code, "it");

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 1 /* True */,
                                     1);
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 0 /* False */,
                                     0);
}

TEST_F(LanguagePackManagerTest, UpdatePacksForOobeWrongLocaleTest) {
  session_manager_->SetSessionState(session_manager::SessionState::OOBE);

  dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  dlcservice_client_->set_install_root_path("/path");

  // Test UMA metrics: pre-condition.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 1 /* True */,
                                     0);
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 0 /* False */,
                                     0);

  manager_->UpdatePacksForOobe(
      "xxx", base::BindOnce(&LanguagePackManagerTest::OobeTestCallback,
                            base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kWrongId);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kUnknown);

  // Test UMA metrics: post-condition.
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 1 /* True */,
                                     0);
  histogram_tester.ExpectBucketCount(kHistogramOobeValidLocale, 0 /* False */,
                                     1);
}

TEST_F(LanguagePackManagerTest, UpdatePacksForOobeFailureTest) {
  session_manager_->SetSessionState(session_manager::SessionState::OOBE);

  dlcservice_client_->set_install_error(dlcservice::kErrorInternal);

  manager_->UpdatePacksForOobe(
      "es-es", base::BindOnce(&LanguagePackManagerTest::OobeTestCallback,
                              base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, PackResult::ErrorCode::kOther);
  EXPECT_EQ(pack_result_.pack_state, PackResult::StatusCode::kUnknown);
}

}  // namespace ash::language_packs
