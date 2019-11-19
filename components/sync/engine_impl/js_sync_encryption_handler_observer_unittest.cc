// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/js_sync_encryption_handler_observer.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/js/js_event_details.h"
#include "components/sync/js/js_test_util.h"
#include "components/sync/syncable/directory_cryptographer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class JsSyncEncryptionHandlerObserverTest : public testing::Test {
 protected:
  JsSyncEncryptionHandlerObserverTest() {
    js_sync_encryption_handler_observer_.SetJsEventHandler(
        mock_js_event_handler_.AsWeakHandle());
  }

 private:
  // This must be destroyed after the member variables below in order
  // for WeakHandles to be destroyed properly.
  base::test::SingleThreadTaskEnvironment task_environment_;

 protected:
  StrictMock<MockJsEventHandler> mock_js_event_handler_;
  JsSyncEncryptionHandlerObserver js_sync_encryption_handler_observer_;

  void PumpLoop() { base::RunLoop().RunUntilIdle(); }
};

TEST_F(JsSyncEncryptionHandlerObserverTest, NoArgNotifiations) {
  InSequence dummy;

  EXPECT_CALL(
      mock_js_event_handler_,
      HandleJsEvent("onEncryptionComplete", HasDetails(JsEventDetails())));

  js_sync_encryption_handler_observer_.OnEncryptionComplete();
  PumpLoop();
}

TEST_F(JsSyncEncryptionHandlerObserverTest, OnPassphraseRequired) {
  InSequence dummy;

  base::DictionaryValue reason_passphrase_not_required_details;
  base::DictionaryValue reason_encryption_details;
  base::DictionaryValue reason_decryption_details;

  reason_encryption_details.SetString(
      "reason", PassphraseRequiredReasonToString(REASON_ENCRYPTION));
  reason_decryption_details.SetString(
      "reason", PassphraseRequiredReasonToString(REASON_DECRYPTION));

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onPassphraseRequired",
                            HasDetailsAsDictionary(reason_encryption_details)));
  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onPassphraseRequired",
                            HasDetailsAsDictionary(reason_decryption_details)));

  js_sync_encryption_handler_observer_.OnPassphraseRequired(
      REASON_ENCRYPTION, KeyDerivationParams::CreateForPbkdf2(),
      sync_pb::EncryptedData());
  js_sync_encryption_handler_observer_.OnPassphraseRequired(
      REASON_DECRYPTION, KeyDerivationParams::CreateForPbkdf2(),
      sync_pb::EncryptedData());
  PumpLoop();
}

TEST_F(JsSyncEncryptionHandlerObserverTest, OnBootstrapTokenUpdated) {
  base::DictionaryValue bootstrap_token_details;
  bootstrap_token_details.SetString("bootstrapToken", "<redacted>");
  bootstrap_token_details.SetString("type", "PASSPHRASE_BOOTSTRAP_TOKEN");

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onBootstrapTokenUpdated",
                            HasDetailsAsDictionary(bootstrap_token_details)));

  js_sync_encryption_handler_observer_.OnBootstrapTokenUpdated(
      "sensitive_token", PASSPHRASE_BOOTSTRAP_TOKEN);
  PumpLoop();
}

TEST_F(JsSyncEncryptionHandlerObserverTest, OnEncryptedTypesChanged) {
  auto encrypted_type_values = std::make_unique<base::ListValue>();
  ModelTypeSet encrypted_types;

  for (int i = FIRST_REAL_MODEL_TYPE; i < ModelType::NUM_ENTRIES; ++i) {
    ModelType type = ModelTypeFromInt(i);
    encrypted_types.Put(type);
    encrypted_type_values->AppendString(ModelTypeToString(type));
  }

  base::DictionaryValue expected_details;
  const bool kEncrytEverything = false;
  expected_details.Set("encryptedTypes", std::move(encrypted_type_values));
  expected_details.SetBoolean("encryptEverything", kEncrytEverything);

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onEncryptedTypesChanged",
                            HasDetailsAsDictionary(expected_details)));

  js_sync_encryption_handler_observer_.OnEncryptedTypesChanged(
      encrypted_types, kEncrytEverything);
  PumpLoop();
}

TEST_F(JsSyncEncryptionHandlerObserverTest, OnCryptographerStateChanged) {
  base::DictionaryValue expected_details;
  bool expected_ready = false;
  bool expected_pending = false;
  expected_details.SetBoolean("canEncrypt", expected_ready);
  expected_details.SetBoolean("hasPendingKeys", expected_pending);
  ModelTypeSet encrypted_types;

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onCryptographerStateChanged",
                            HasDetailsAsDictionary(expected_details)));

  DirectoryCryptographer cryptographer;
  js_sync_encryption_handler_observer_.OnCryptographerStateChanged(
      &cryptographer, /*has_pending_keys=*/false);
  PumpLoop();
}

TEST_F(JsSyncEncryptionHandlerObserverTest, OnPassphraseTypeChanged) {
  InSequence dummy;

  base::DictionaryValue passphrase_type_details;
  passphrase_type_details.SetString("passphraseType",
                                    "PassphraseType::kImplicitPassphrase");
  passphrase_type_details.SetInteger("explicitPassphraseTime", 10);
  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onPassphraseTypeChanged",
                            HasDetailsAsDictionary(passphrase_type_details)));

  js_sync_encryption_handler_observer_.OnPassphraseTypeChanged(
      PassphraseType::kImplicitPassphrase, ProtoTimeToTime(10));
  PumpLoop();
}

}  // namespace
}  // namespace syncer
