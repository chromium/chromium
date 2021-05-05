// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/language/language_packs/language_pack_manager.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::chromeos::language_packs::LanguagePackManager;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArg;

namespace chromeos {
namespace language_packs {

namespace {

constexpr char kFakeDlcId[] = "FakeDlc";

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

}  // namespace

class LanguagePackManagerTest : public testing::Test {
 public:
  void SetUp() override {
    manager_ = LanguagePackManager::GetInstance();

    DlcserviceClient::InitializeFake();
    dlcservice_client_ =
        static_cast<FakeDlcserviceClient*>(DlcserviceClient::Get());

    ResetPackResult();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { DlcserviceClient::Shutdown(); }

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

TEST_F(LanguagePackManagerTest, IsPackAvailableTrueTest) {
  const bool available = manager_->IsPackAvailable(kHandwritingFeatureId, "en");
  EXPECT_TRUE(available);
}

TEST_F(LanguagePackManagerTest, IsPackAvailableFalseTest) {
  // Correct ID, wrong language.
  bool available = manager_->IsPackAvailable(kHandwritingFeatureId, "fr");
  EXPECT_FALSE(available);

  // ID doesn't exists.
  available = manager_->IsPackAvailable("foo", "fr");
  EXPECT_FALSE(available);
}

TEST_F(LanguagePackManagerTest, InstallSuccessTest) {
  dlcservice_client_->set_install_error(dlcservice::kErrorNone);
  dlcservice_client_->set_install_root_path("/path");

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->InstallPack(
      kHandwritingFeatureId, "en",
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
      kHandwritingFeatureId, "en",
      base::BindOnce(&LanguagePackManagerTest::InstallTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorInternal);
  EXPECT_NE(pack_result_.pack_state, PackResult::INSTALLED);
}

TEST_F(LanguagePackManagerTest, InstallWrongIdTest) {
  manager_->InstallPack(
      kFakeDlcId, "en",
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

  manager_->InstallPack(kFakeDlcId, "en", callback.GetInstallCallback());
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
      kHandwritingFeatureId, "en",
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
      kHandwritingFeatureId, "en",
      base::BindOnce(&LanguagePackManagerTest::GetPackStateTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorInternal);
  EXPECT_NE(pack_result_.pack_state, PackResult::INSTALLED);
}

TEST_F(LanguagePackManagerTest, GetPackStateWrongIdTest) {
  manager_->GetPackState(
      kFakeDlcId, "en",
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

  manager_->GetPackState(kFakeDlcId, "en", callback.GetPackStateCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanguagePackManagerTest, RemovePackSuccessTest) {
  dlcservice_client_->set_uninstall_error(dlcservice::kErrorNone);

  // We need to use an existing Pack ID, so that we do get a result back.
  manager_->RemovePack(
      kHandwritingFeatureId, "en",
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
      kHandwritingFeatureId, "en",
      base::BindOnce(&LanguagePackManagerTest::RemoveTestCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pack_result_.operation_error, dlcservice::kErrorInternal);
}

TEST_F(LanguagePackManagerTest, RemovePackWrongIdTest) {
  manager_->RemovePack(
      kFakeDlcId, "en",
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

  manager_->RemovePack(kFakeDlcId, "en", callback.GetRemoveCallback());
  base::RunLoop().RunUntilIdle();
}

}  // namespace language_packs
}  // namespace chromeos
