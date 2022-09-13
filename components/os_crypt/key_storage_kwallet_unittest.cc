// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_kwallet.h"

#include "base/memory/raw_ptr.h"
#include "base/nix/xdg_util.h"
#include "build/branding_buildflags.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::StrictMock;

constexpr KWalletDBus::Error SUCCESS = KWalletDBus::Error::SUCCESS;
constexpr KWalletDBus::Error CANNOT_READ = KWalletDBus::Error::CANNOT_READ;
constexpr KWalletDBus::Error CANNOT_CONTACT =
    KWalletDBus::Error::CANNOT_CONTACT;

// These names are not allowed to change in prod, unless we intentionally
// migrate.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kExpectedFolderName[] = "Chrome Keys";
const char kExpectedEntryName[] = "Chrome Safe Storage";
#else
const char kExpectedFolderName[] = "Chromium Keys";
const char kExpectedEntryName[] = "Chromium Safe Storage";
#endif

// Environment-specific behavior is handled and tested with KWalletDBus, not
// here, but we still need a value to instantiate.
const base::nix::DesktopEnvironment kDesktopEnv =
    base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_KDE5;

class MockKWalletDBus : public KWalletDBus {
 public:
  MockKWalletDBus() : KWalletDBus(kDesktopEnv) {
    // On destruction, GetSessionBus() is called to get the bus and shut it
    // down. Here we create a mock to be returned by GetSessionBus().
    mock_session_bus_ = new dbus::MockBus(dbus::Bus::Options());
  }

  dbus::MockBus* GetSessionBus() override { return mock_session_bus_.get(); }

  MOCK_METHOD0(StartKWalletd, bool());

  MOCK_METHOD1(IsEnabled, KWalletDBus::Error(bool*));

  MOCK_METHOD1(NetworkWallet, KWalletDBus::Error(std::string*));

  MOCK_METHOD3(Open,
               KWalletDBus::Error(const std::string&,
                                  const std::string&,
                                  int*));

  MOCK_METHOD4(
      HasFolder,
      KWalletDBus::Error(int, const std::string&, const std::string&, bool*));

  MOCK_METHOD4(
      CreateFolder,
      KWalletDBus::Error(int, const std::string&, const std::string&, bool*));

  MOCK_METHOD5(ReadPassword,
               KWalletDBus::Error(int,
                                  const std::string&,
                                  const std::string&,
                                  const std::string&,
                                  absl::optional<std::string>*));

  MOCK_METHOD6(WritePassword,
               KWalletDBus::Error(int,
                                  const std::string&,
                                  const std::string&,
                                  const std::string&,
                                  const std::string&,
                                  bool*));

  MOCK_METHOD4(Close, KWalletDBus::Error(int, bool, const std::string&, bool*));

 private:
  scoped_refptr<dbus::MockBus> mock_session_bus_;
};

class KeyStorageKWalletTest : public testing::Test {
 public:
  KeyStorageKWalletTest() : key_storage_kwallet_(kDesktopEnv, "test-app") {}

  KeyStorageKWalletTest(const KeyStorageKWalletTest&) = delete;
  KeyStorageKWalletTest& operator=(const KeyStorageKWalletTest&) = delete;

  void SetUp() override {
    kwallet_dbus_mock_ = new StrictMock<MockKWalletDBus>();

    // Calls from |key_storage_kwallet_|'s destructor
    EXPECT_CALL(*kwallet_dbus_mock_, Close(_, false, _, _))
        .WillOnce(DoAll(SetArgPointee<3>(true), Return(SUCCESS)));
    EXPECT_CALL(*kwallet_dbus_mock_->GetSessionBus(), ShutdownAndBlock())
        .Times(1);
  }

  void SuccessfulInit() {
    EXPECT_CALL(*kwallet_dbus_mock_, IsEnabled(_))
        .WillOnce(DoAll(SetArgPointee<0>(true), Return(SUCCESS)));
    EXPECT_CALL(*kwallet_dbus_mock_, NetworkWallet(_))
        .WillOnce(
            DoAll(SetArgPointee<0>(std::string("mollet")), Return(SUCCESS)));

    EXPECT_TRUE(key_storage_kwallet_.InitWithKWalletDBus(
        std::unique_ptr<MockKWalletDBus>(kwallet_dbus_mock_)));
  }

 protected:
  raw_ptr<StrictMock<MockKWalletDBus>> kwallet_dbus_mock_;
  KeyStorageKWallet key_storage_kwallet_;
  const std::string wallet_name_ = "mollet";
};

TEST_F(KeyStorageKWalletTest, InitializeFolder) {
  SuccessfulInit();
  EXPECT_CALL(*kwallet_dbus_mock_, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(123), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, HasFolder(123, kExpectedFolderName, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(false), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, CreateFolder(123, kExpectedFolderName, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_,
              ReadPassword(123, kExpectedFolderName, kExpectedEntryName, _, _))
      .WillOnce(DoAll(SetArgPointee<4>("butter"), Return(SUCCESS)));

  EXPECT_EQ("butter", key_storage_kwallet_.GetKey());
}

TEST_F(KeyStorageKWalletTest, ExistingPassword) {
  SuccessfulInit();
  EXPECT_CALL(*kwallet_dbus_mock_, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(123), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, HasFolder(123, kExpectedFolderName, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_,
              ReadPassword(123, kExpectedFolderName, kExpectedEntryName, _, _))
      .WillOnce(DoAll(SetArgPointee<4>("butter"), Return(SUCCESS)));

  EXPECT_EQ("butter", key_storage_kwallet_.GetKey());
}

TEST_F(KeyStorageKWalletTest, GenerateNewPassword) {
  SuccessfulInit();
  std::string generated_password;
  EXPECT_CALL(*kwallet_dbus_mock_, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(123), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, HasFolder(123, kExpectedFolderName, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_,
              ReadPassword(123, kExpectedFolderName, kExpectedEntryName, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(absl::nullopt), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, WritePassword(123, kExpectedFolderName,
                                                 kExpectedEntryName, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&generated_password), SetArgPointee<5>(true),
                      Return(SUCCESS)));

  EXPECT_EQ(generated_password, key_storage_kwallet_.GetKey());
}

TEST_F(KeyStorageKWalletTest, InitKWalletNotEnabled) {
  EXPECT_CALL(*kwallet_dbus_mock_, IsEnabled(_))
      .WillOnce(DoAll(SetArgPointee<0>(false), Return(SUCCESS)));

  EXPECT_FALSE(key_storage_kwallet_.InitWithKWalletDBus(
      std::unique_ptr<MockKWalletDBus>(kwallet_dbus_mock_)));
}

TEST_F(KeyStorageKWalletTest, InitCannotStart) {
  EXPECT_CALL(*kwallet_dbus_mock_, IsEnabled(_))
      .WillOnce(Return(CANNOT_CONTACT));
  EXPECT_CALL(*kwallet_dbus_mock_, StartKWalletd()).WillOnce(Return(false));

  EXPECT_FALSE(key_storage_kwallet_.InitWithKWalletDBus(
      std::unique_ptr<MockKWalletDBus>(kwallet_dbus_mock_)));
}

TEST_F(KeyStorageKWalletTest, InitFailTwice) {
  EXPECT_CALL(*kwallet_dbus_mock_, IsEnabled(_))
      .WillOnce(Return(CANNOT_CONTACT))
      .WillOnce(Return(CANNOT_CONTACT));
  EXPECT_CALL(*kwallet_dbus_mock_, StartKWalletd()).WillOnce(Return(true));

  EXPECT_FALSE(key_storage_kwallet_.InitWithKWalletDBus(
      std::unique_ptr<MockKWalletDBus>(kwallet_dbus_mock_)));
}

TEST_F(KeyStorageKWalletTest, InitTryTwiceAndFail) {
  EXPECT_CALL(*kwallet_dbus_mock_, IsEnabled(_))
      .WillOnce(Return(CANNOT_CONTACT))
      .WillOnce(DoAll(SetArgPointee<0>(true), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, StartKWalletd()).WillOnce(Return(true));
  EXPECT_CALL(*kwallet_dbus_mock_, NetworkWallet(_))
      .WillOnce(Return(CANNOT_CONTACT));

  EXPECT_FALSE(key_storage_kwallet_.InitWithKWalletDBus(
      std::unique_ptr<MockKWalletDBus>(kwallet_dbus_mock_)));
}

TEST_F(KeyStorageKWalletTest, InitTryTwiceAndSuccess) {
  EXPECT_CALL(*kwallet_dbus_mock_, IsEnabled(_))
      .WillOnce(Return(CANNOT_CONTACT))
      .WillOnce(DoAll(SetArgPointee<0>(true), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, StartKWalletd()).WillOnce(Return(true));
  EXPECT_CALL(*kwallet_dbus_mock_, NetworkWallet(_))
      .WillOnce(DoAll(SetArgPointee<0>(wallet_name_), Return(SUCCESS)));

  EXPECT_TRUE(key_storage_kwallet_.InitWithKWalletDBus(
      std::unique_ptr<MockKWalletDBus>(kwallet_dbus_mock_)));
}

// Tests for a dbus connection that fails after initialization.
// Any error is expected to return an empty password.
class KeyStorageKWalletFailuresTest
    : public testing::TestWithParam<KWalletDBus::Error> {
 public:
  KeyStorageKWalletFailuresTest()
      : key_storage_kwallet_(kDesktopEnv, "test-app") {}

  KeyStorageKWalletFailuresTest(const KeyStorageKWalletFailuresTest&) = delete;
  KeyStorageKWalletFailuresTest& operator=(
      const KeyStorageKWalletFailuresTest&) = delete;

  void SetUp() override {
    // |key_storage_kwallet_| will take ownership of |kwallet_dbus_mock_|.
    kwallet_dbus_mock_ = new StrictMock<MockKWalletDBus>();

    // Successful initialization.
    EXPECT_CALL(*kwallet_dbus_mock_, IsEnabled(_))
        .WillOnce(DoAll(SetArgPointee<0>(true), Return(SUCCESS)));
    EXPECT_CALL(*kwallet_dbus_mock_, NetworkWallet(_))
        .WillOnce(
            DoAll(SetArgPointee<0>(std::string("mollet")), Return(SUCCESS)));

    EXPECT_TRUE(key_storage_kwallet_.InitWithKWalletDBus(
        std::unique_ptr<MockKWalletDBus>(kwallet_dbus_mock_)));

    // Calls from |key_storage_kwallet_|'s destructor.
    EXPECT_CALL(*kwallet_dbus_mock_, Close(_, false, _, _))
        .WillOnce(DoAll(SetArgPointee<3>(true), Return(SUCCESS)));
    EXPECT_CALL(*kwallet_dbus_mock_->GetSessionBus(), ShutdownAndBlock())
        .Times(1);
  }

 protected:
  raw_ptr<StrictMock<MockKWalletDBus>> kwallet_dbus_mock_;
  KeyStorageKWallet key_storage_kwallet_;
  const std::string wallet_name_ = "mollet";
};

INSTANTIATE_TEST_SUITE_P(All,
                         KeyStorageKWalletFailuresTest,
                         ::testing::Values(CANNOT_READ, CANNOT_CONTACT));

TEST_P(KeyStorageKWalletFailuresTest, PostInitFailureOpen) {
  EXPECT_CALL(*kwallet_dbus_mock_, Open(_, _, _)).WillOnce(Return(GetParam()));

  EXPECT_FALSE(key_storage_kwallet_.GetKey().has_value());
}

TEST_P(KeyStorageKWalletFailuresTest, PostInitFailureHasFolder) {
  EXPECT_CALL(*kwallet_dbus_mock_, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(123), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, HasFolder(123, _, _, _))
      .WillOnce(Return(GetParam()));

  EXPECT_FALSE(key_storage_kwallet_.GetKey().has_value());
}

TEST_P(KeyStorageKWalletFailuresTest, PostInitFailureCreateFolder) {
  EXPECT_CALL(*kwallet_dbus_mock_, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(123), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, HasFolder(123, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(false), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, CreateFolder(123, _, _, _))
      .WillOnce(Return(GetParam()));

  EXPECT_FALSE(key_storage_kwallet_.GetKey().has_value());
}

TEST_P(KeyStorageKWalletFailuresTest, PostInitFailureReadPassword) {
  EXPECT_CALL(*kwallet_dbus_mock_, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(123), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, HasFolder(123, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, ReadPassword(123, _, _, _, _))
      .WillOnce(Return(GetParam()));

  EXPECT_FALSE(key_storage_kwallet_.GetKey().has_value());
}

TEST_P(KeyStorageKWalletFailuresTest, PostInitFailureWritePassword) {
  EXPECT_CALL(*kwallet_dbus_mock_, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(123), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, HasFolder(123, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, ReadPassword(123, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(absl::nullopt), Return(SUCCESS)));
  EXPECT_CALL(*kwallet_dbus_mock_, WritePassword(123, _, _, _, _, _))
      .WillOnce(Return(GetParam()));

  EXPECT_FALSE(key_storage_kwallet_.GetKey().has_value());
}

}  // namespace
