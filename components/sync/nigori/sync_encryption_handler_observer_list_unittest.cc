// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/sync_encryption_handler_observer_list.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "components/sync/base/custom_passphrase_bootstrap_token.h"
#include "components/sync/engine/cryptographer.h"
#include "components/sync/engine/required_passphrase_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;

class MockObserver : public SyncEncryptionHandler::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnPassphraseRequired,
              (std::unique_ptr<RequiredPassphraseVerifier>),
              (override));
  MOCK_METHOD(void,
              OnPassphraseAccepted,
              (const CustomPassphraseBootstrapToken&),
              (override));
  MOCK_METHOD(void, OnTrustedVaultKeyRequired, (), (override));
  MOCK_METHOD(void, OnTrustedVaultKeyAccepted, (), (override));
  MOCK_METHOD(void, OnKeystoreKeysRequired, (), (override));
  MOCK_METHOD(void, OnKeystoreKeysAccepted, (), (override));
  MOCK_METHOD(void, OnEncryptedTypesChanged, (DataTypeSet, bool), (override));
  MOCK_METHOD(void,
              OnCryptographerStateChanged,
              (Cryptographer*, bool has_pending_keys),
              (override));
  MOCK_METHOD(void,
              OnPassphraseTypeChanged,
              (PassphraseType, base::Time),
              (override));
};

// A fake implementation of RequiredPassphraseVerifier to test
// OnPassphraseRequired.
class FakeRequiredPassphraseVerifier : public RequiredPassphraseVerifier {
 public:
  FakeRequiredPassphraseVerifier() = default;
  ~FakeRequiredPassphraseVerifier() override = default;

  bool IsValidDecryptionPassphrase(const std::string& passphrase) override {
    return false;
  }
  bool IsValidDecryptionBootstrapToken(
      const CustomPassphraseBootstrapToken& bootstrap_token) override {
    return false;
  }
  std::unique_ptr<RequiredPassphraseVerifier> Clone() const override {
    return std::make_unique<FakeRequiredPassphraseVerifier>();
  }
};

class SyncEncryptionHandlerObserverListTest : public testing::Test {
 protected:
  SyncEncryptionHandlerObserverListTest() = default;
  ~SyncEncryptionHandlerObserverListTest() override = default;

  void SetUp() override { observer_list_.AddObserver(&observer_); }

  void TearDown() override { observer_list_.RemoveObserver(&observer_); }

  SyncEncryptionHandlerObserverList observer_list_;
  testing::StrictMock<MockObserver> observer_;
};

TEST_F(SyncEncryptionHandlerObserverListTest, ShouldNotifyPassphraseRequired) {
  FakeRequiredPassphraseVerifier verifier;
  EXPECT_CALL(observer_, OnPassphraseRequired);
  observer_list_.NotifyPassphraseRequired(verifier);
}

TEST_F(SyncEncryptionHandlerObserverListTest, ShouldNotifyPassphraseAccepted) {
  CustomPassphraseBootstrapToken token;
  EXPECT_CALL(observer_, OnPassphraseAccepted);
  observer_list_.NotifyPassphraseAccepted(token);
}

TEST_F(SyncEncryptionHandlerObserverListTest,
       ShouldNotifyTrustedVaultKeyRequired) {
  EXPECT_CALL(observer_, OnTrustedVaultKeyRequired);
  observer_list_.NotifyTrustedVaultKeyRequired();
}

TEST_F(SyncEncryptionHandlerObserverListTest,
       ShouldNotifyTrustedVaultKeyAccepted) {
  EXPECT_CALL(observer_, OnTrustedVaultKeyAccepted);
  observer_list_.NotifyTrustedVaultKeyAccepted();
}

TEST_F(SyncEncryptionHandlerObserverListTest,
       ShouldNotifyKeystoreKeysRequired) {
  EXPECT_CALL(observer_, OnKeystoreKeysRequired);
  observer_list_.NotifyKeystoreKeysRequired();
}

TEST_F(SyncEncryptionHandlerObserverListTest,
       ShouldNotifyKeystoreKeysAccepted) {
  EXPECT_CALL(observer_, OnKeystoreKeysAccepted);
  observer_list_.NotifyKeystoreKeysAccepted();
}

TEST_F(SyncEncryptionHandlerObserverListTest,
       ShouldNotifyEncryptedTypesChanged) {
  EXPECT_CALL(observer_, OnEncryptedTypesChanged);
  observer_list_.NotifyEncryptedTypesChanged(DataTypeSet(), true);
}

TEST_F(SyncEncryptionHandlerObserverListTest,
       ShouldNotifyCryptographerStateChanged) {
  EXPECT_CALL(observer_, OnCryptographerStateChanged);
  observer_list_.NotifyCryptographerStateChanged(nullptr, true);
}

TEST_F(SyncEncryptionHandlerObserverListTest,
       ShouldNotifyPassphraseTypeChanged) {
  EXPECT_CALL(observer_, OnPassphraseTypeChanged);
  observer_list_.NotifyPassphraseTypeChanged(
      PassphraseType::kImplicitPassphrase, base::Time());
}

TEST_F(SyncEncryptionHandlerObserverListTest,
       ShouldNotNotifyUnregisteredObserver) {
  SyncEncryptionHandlerObserverList local_observer_list;
  testing::StrictMock<MockObserver> unregistered_observer;

  FakeRequiredPassphraseVerifier verifier;
  local_observer_list.NotifyPassphraseRequired(verifier);
  local_observer_list.NotifyTrustedVaultKeyRequired();
}

}  // namespace

}  // namespace syncer
