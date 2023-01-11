// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language/language_packs/language_pack_manager.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::dlcservice::DlcState;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArg;

namespace ash::language_packs {

namespace {

constexpr char kFakeDlcId[] = "FakeDlc";
constexpr char kSupportedLocale[] = "es";

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

  MOCK_METHOD(void, Callback, (const PackResult&), ());
};

class MockObserver : public LanguagePackManager::Observer {
 public:
  MOCK_METHOD(void, OnPackStateChanged, (const PackResult& pack_result));
};

// Utility function that creates a DlcState with no error, populated with id
// and path.
DlcState CreateInstalledState() {
  DlcState output;
  output.set_state(dlcservice::DlcState_State_INSTALLED);
  output.set_id(kHandwritingFeatureId);
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

 protected:
  LanguagePackManager* manager_;
  PackResult pack_result_;
  FakeDlcserviceClient* dlcservice_client_;

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

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->InstallPack(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorNone);
  EXPECT_EQ(pack_result_.pack_state, PackResult::INSTALLED);
  EXPECT_EQ(pack_result_.path, "/path");
}

TEST_F(LanguagePackManagerTest, InstallFailureTest) {
  dlcservice_client_->set_install_error(dlcservice::kErrorInternal);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->InstallPack(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorInternal);
  EXPECT_NE(pack_result_.pack_state, PackResult::INSTALLED);
}

TEST_F(LanguagePackManagerTest, InstallWrongIdTest) {
  manager_->InstallPack(
      kFakeDlcId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorInvalidDlc);
  EXPECT_EQ(pack_result_.pack_state, PackResult::WRONG_ID);
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

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->GetPackState(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::GetPackStateTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorNone);
  EXPECT_EQ(pack_result_.pack_state, PackResult::INSTALLED);
  EXPECT_EQ(pack_result_.path, "/path");
}

TEST_F(LanguagePackManagerTest, GetPackStateFailureTest) {
  dlcservice_client_->set_get_dlc_state_error(dlcservice::kErrorInternal);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->GetPackState(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::GetPackStateTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorInternal);
  EXPECT_NE(pack_result_.pack_state, PackResult::INSTALLED);
}

TEST_F(LanguagePackManagerTest, GetPackStateWrongIdTest) {
  manager_->GetPackState(
      kFakeDlcId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::GetPackStateTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorInvalidDlc);
  EXPECT_EQ(pack_result_.pack_state, PackResult::WRONG_ID);
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

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->RemovePack(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::RemoveTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorNone);
  EXPECT_EQ(pack_result_.pack_state, PackResult::NOT_INSTALLED);
}

TEST_F(LanguagePackManagerTest, RemovePackFailureTest) {
  dlcservice_client_->set_uninstall_error(dlcservice::kErrorInternal);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->RemovePack(
      kHandwritingFeatureId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::RemoveTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorInternal);
}

TEST_F(LanguagePackManagerTest, RemovePackWrongIdTest) {
  manager_->RemovePack(
      kFakeDlcId, kSupportedLocale,
      base::BindOnce(&LanguagePackManagerTest::RemoveTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorInvalidDlc);
  EXPECT_EQ(pack_result_.pack_state, PackResult::WRONG_ID);
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
  EXPECT_CALL(observer, OnPackStateChanged(_)).Times(1);
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
  EXPECT_CALL(observer, OnPackStateChanged(_)).Times(1);
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
  const std::vector<std::string> handwriting(
      {"am", "ar", "be", "bg", "bn", "ca", "cs", "da", "de", "el", "es",
       "et", "fa", "fi", "fr", "ga", "gu", "hi", "hr", "hu", "hy", "id",
       "is", "it", "iw", "ja", "ka", "kk", "km", "kn", "ko", "lo", "lt",
       "lv", "ml", "mn", "mr", "ms", "mt", "my", "ne", "nl", "no", "or",
       "pa", "pl", "pt", "ro", "ru", "si", "sk", "sl", "sr", "sv", "ta",
       "te", "th", "ti", "tl", "tr", "uk", "ur", "vi", "zh"});
  for (const auto& locale : handwriting) {
    EXPECT_TRUE(manager_->IsPackAvailable(kHandwritingFeatureId, locale));
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

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->InstallBasePack(
      kHandwritingFeatureId,
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorNone);
  EXPECT_EQ(pack_result_.pack_state, PackResult::INSTALLED);
  EXPECT_EQ(pack_result_.path, "/path");
}

TEST_F(LanguagePackManagerTest, InstallBasePackFailureTestFailure) {
  dlcservice_client_->set_install_error(dlcservice::kErrorInternal);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->InstallBasePack(
      kHandwritingFeatureId,
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorInternal);
  EXPECT_NE(pack_result_.pack_state, PackResult::INSTALLED);
}

}  // namespace ash::language_packs
