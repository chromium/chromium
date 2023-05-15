// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_account_mapper.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kGCMAccountMapperSenderId[] = "745476177629";
const char kGCMAccountMapperSendTo[] = "google.com";
const char kRegistrationId[] = "reg_id";
const char kEmbeddedAppIdKey[] = "gcmb";
const char kTestAppId[] = "test_app_id";
const char kTestDataKey[] = "data_key";
const char kTestDataValue[] = "data_value";
const char kTestCollapseKey[] = "test_collapse_key";
const char kTestSenderId[] = "test_sender_id";

AccountMapping MakeAccountMapping(const CoreAccountId& account_id,
                                  AccountMapping::MappingStatus status,
                                  const base::Time& status_change_timestamp,
                                  const std::string& last_message_id) {
  AccountMapping account_mapping;
  account_mapping.account_id = account_id;
  account_mapping.email = account_id.ToString() + "@gmail.com";
  // account_mapping.access_token intentionally left empty.
  account_mapping.status = status;
  account_mapping.status_change_timestamp = status_change_timestamp;
  account_mapping.last_message_id = last_message_id;
  return account_mapping;
}

GCMClient::AccountTokenInfo MakeAccountTokenInfo(
    const CoreAccountId& account_id) {
  GCMClient::AccountTokenInfo account_token;
  account_token.account_id = account_id;
  account_token.email = account_id.ToString() + "@gmail.com";
  account_token.access_token = account_id.ToString() + "_token";
  return account_token;
}
class CustomFakeGCMDriver : public FakeGCMDriver {
 public:
  enum LastMessageAction {
    NONE,
    SEND_STARTED,
    SEND_FINISHED,
    SEND_ACKNOWLEDGED
  };

  CustomFakeGCMDriver();
  ~CustomFakeGCMDriver() override;

  // GCMDriver implementation:
  void UpdateAccountMapping(const AccountMapping& account_mapping) override;
  void RemoveAccountMapping(const CoreAccountId& account_id) override;
  void RegisterImpl(const std::string& app_id,
                    const std::vector<std::string>& sender_ids) override;

  void CompleteRegister(const std::string& registration_id,
                        GCMClient::Result result);
  void CompleteSend(const std::string& message_id, GCMClient::Result result);
  void AcknowledgeSend(const std::string& message_id);
  void MessageSendError(const std::string& message_id);

  void CompleteSendAllMessages();
  void AcknowledgeSendAllMessages();
  void SetLastMessageAction(const std::string& message_id,
                            LastMessageAction action);
  void Clear();

  const AccountMapping& last_account_mapping() const {
    return account_mapping_;
  }
  const std::string& last_message_id() const { return last_message_id_; }
  const CoreAccountId& last_removed_account_id() const {
    return last_removed_account_id_;
  }
  LastMessageAction last_action() const { return last_action_; }
  bool registration_id_requested() const { return registration_id_requested_; }

 protected:
  void SendImpl(const std::string& app_id,
                const std::string& receiver_id,
                const OutgoingMessage& message) override;

 private:
  AccountMapping account_mapping_;
  std::string last_message_id_;
  CoreAccountId last_removed_account_id_;
  LastMessageAction last_action_;
  std::map<std::string, LastMessageAction> all_messages_;
  bool registration_id_requested_;
};

CustomFakeGCMDriver::CustomFakeGCMDriver()
    : last_action_(NONE), registration_id_requested_(false) {
}

CustomFakeGCMDriver::~CustomFakeGCMDriver() {
}

void CustomFakeGCMDriver::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
  account_mapping_.email = account_mapping.email;
  account_mapping_.account_id = account_mapping.account_id;
  account_mapping_.access_token = account_mapping.access_token;
  account_mapping_.status = account_mapping.status;
  account_mapping_.status_change_timestamp =
      account_mapping.status_change_timestamp;
  account_mapping_.last_message_id = account_mapping.last_message_id;
}

void CustomFakeGCMDriver::RemoveAccountMapping(
    const CoreAccountId& account_id) {
  last_removed_account_id_ = account_id;
}

void CustomFakeGCMDriver::RegisterImpl(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  DCHECK_EQ(kGCMAccountMapperAppId, app_id);
  DCHECK_EQ(1u, sender_ids.size());
  DCHECK_EQ(kGCMAccountMapperSenderId, sender_ids[0]);
  registration_id_requested_ = true;
}

void CustomFakeGCMDriver::CompleteRegister(const std::string& registration_id,
                                           GCMClient::Result result) {
  RegisterFinished(kGCMAccountMapperAppId, registration_id, result);
}

void CustomFakeGCMDriver::CompleteSend(const std::string& message_id,
                                       GCMClient::Result result) {
  SendFinished(kGCMAccountMapperAppId, message_id, result);
  SetLastMessageAction(message_id, SEND_FINISHED);
}

void CustomFakeGCMDriver::AcknowledgeSend(const std::string& message_id) {
  GCMAppHandler* handler = GetAppHandler(kGCMAccountMapperAppId);
  if (handler)
    handler->OnSendAcknowledged(kGCMAccountMapperAppId, message_id);
  SetLastMessageAction(message_id, SEND_ACKNOWLEDGED);
}

void CustomFakeGCMDriver::MessageSendError(const std::string& message_id) {
  GCMAppHandler* handler = GetAppHandler(kGCMAccountMapperAppId);
  if (!handler)
    return;

  GCMClient::SendErrorDetails send_error;
  send_error.message_id = message_id;
  send_error.result = GCMClient::TTL_EXCEEDED;

  handler->OnSendError(kGCMAccountMapperAppId, send_error);
}

void CustomFakeGCMDriver::SendImpl(const std::string& app_id,
                                   const std::string& receiver_id,
                                   const OutgoingMessage& message) {
  DCHECK_EQ(kGCMAccountMapperAppId, app_id);
  DCHECK_EQ(kGCMAccountMapperSendTo, receiver_id);

  SetLastMessageAction(message.id, SEND_STARTED);
}

void CustomFakeGCMDriver::CompleteSendAllMessages() {
  for (std::map<std::string, LastMessageAction>::const_iterator iter =
           all_messages_.begin();
       iter != all_messages_.end();
       ++iter) {
    if (iter->second == SEND_STARTED)
      CompleteSend(iter->first, GCMClient::SUCCESS);
  }
}

void CustomFakeGCMDriver::AcknowledgeSendAllMessages() {
  for (std::map<std::string, LastMessageAction>::const_iterator iter =
           all_messages_.begin();
       iter != all_messages_.end();
       ++iter) {
    if (iter->second == SEND_FINISHED)
      AcknowledgeSend(iter->first);
  }
}

void CustomFakeGCMDriver::Clear() {
  account_mapping_ = AccountMapping();
  last_message_id_.clear();
  last_removed_account_id_ = CoreAccountId();
  last_action_ = NONE;
  registration_id_requested_ = false;
}

void CustomFakeGCMDriver::SetLastMessageAction(const std::string& message_id,
                                               LastMessageAction action) {
  last_action_ = action;
  last_message_id_ = message_id;
  all_messages_[message_id] = action;
}

}  // namespace

class GCMAccountMapperTest : public testing::Test {
 public:
  const CoreAccountId kAccountId;
  const CoreAccountId kAccountId1;
  const CoreAccountId kAccountId2;
  const CoreAccountId kAccountId3;
  const CoreAccountId kAccountId4;

  GCMAccountMapperTest();
  ~GCMAccountMapperTest() override;

  void Restart();

  void Initialize(const GCMAccountMapper::AccountMappings mappings);
  const GCMAccountMapper::AccountMappings& GetAccounts() const {
    return account_mapper_->accounts_;
  }
  void MessageReceived(const std::string& app_id,
                       const IncomingMessage& message);

  GCMAccountMapper* mapper() { return account_mapper_.get(); }

  CustomFakeGCMDriver& gcm_driver() { return gcm_driver_; }

  base::SimpleTestClock* clock() { return &clock_; }
  const std::string& last_received_app_id() const {
    return last_received_app_id_;
  }
  const IncomingMessage& last_received_message() const {
    return last_received_message_;
  }

 private:
  CustomFakeGCMDriver gcm_driver_;
  std::unique_ptr<GCMAccountMapper> account_mapper_;
  base::SimpleTestClock clock_;
  std::string last_received_app_id_;
  IncomingMessage last_received_message_;
};

GCMAccountMapperTest::GCMAccountMapperTest()
    : kAccountId(CoreAccountId::FromGaiaId("acc_id")),
      kAccountId1(CoreAccountId::FromGaiaId("acc_id1")),
      kAccountId2(CoreAccountId::FromGaiaId("acc_id2")),
      kAccountId3(CoreAccountId::FromGaiaId("acc_id3")),
      kAccountId4(CoreAccountId::FromGaiaId("acc_id4")) {
  Restart();
}

GCMAccountMapperTest::~GCMAccountMapperTest() {
}

void GCMAccountMapperTest::Restart() {
  if (account_mapper_)
    account_mapper_->ShutdownHandler();
  gcm_driver_.RemoveAppHandler(kGCMAccountMapperAppId);
  account_mapper_ = std::make_unique<GCMAccountMapper>(&gcm_driver_);
  account_mapper_->SetClockForTesting(&clock_);
}

void GCMAccountMapperTest::Initialize(
    const GCMAccountMapper::AccountMappings mappings) {
  mapper()->Initialize(
      mappings, base::BindRepeating(&GCMAccountMapperTest::MessageReceived,
                                    base::Unretained(this)));
}

void GCMAccountMapperTest::MessageReceived(const std::string& app_id,
                                           const IncomingMessage& message) {
  last_received_app_id_ = app_id;
  last_received_message_ = message;
}

// Tests the initialization of account mappings (from the store) when empty.
// It also checks that initialization triggers registration ID request.
TEST_F(GCMAccountMapperTest, InitializeAccountMappingsEmpty) {
  EXPECT_FALSE(gcm_driver().registration_id_requested());
  Initialize(GCMAccountMapper::AccountMappings());
  EXPECT_TRUE(GetAccounts().empty());
  EXPECT_TRUE(gcm_driver().registration_id_requested());
}

// Tests that registration is retried, when new tokens are delivered and in no
// other circumstances.
TEST_F(GCMAccountMapperTest, RegistrationRetryUponFailure) {
  Initialize(GCMAccountMapper::AccountMappings());
  EXPECT_TRUE(gcm_driver().registration_id_requested());
  gcm_driver().Clear();

  gcm_driver().CompleteRegister(kRegistrationId, GCMClient::UNKNOWN_ERROR);
  EXPECT_FALSE(gcm_driver().registration_id_requested());
  gcm_driver().Clear();

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  account_tokens.push_back(MakeAccountTokenInfo(kAccountId2));
  mapper()->SetAccountTokens(account_tokens);
  EXPECT_TRUE(gcm_driver().registration_id_requested());
  gcm_driver().Clear();

  gcm_driver().CompleteRegister(kRegistrationId,
                                GCMClient::ASYNC_OPERATION_PENDING);
  EXPECT_FALSE(gcm_driver().registration_id_requested());
}

// Tests the initialization of account mappings (from the store).
TEST_F(GCMAccountMapperTest, InitializeAccountMappings) {
  GCMAccountMapper::AccountMappings account_mappings;
  AccountMapping account_mapping1 = MakeAccountMapping(
      kAccountId1, AccountMapping::MAPPED, base::Time::Now(), std::string());
  AccountMapping account_mapping2 = MakeAccountMapping(
      kAccountId2, AccountMapping::ADDING, base::Time::Now(), "add_message_1");
  account_mappings.push_back(account_mapping1);
  account_mappings.push_back(account_mapping2);

  Initialize(account_mappings);

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  EXPECT_EQ(2UL, mappings.size());
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();

  EXPECT_EQ(account_mapping1.account_id, iter->account_id);
  EXPECT_EQ(account_mapping1.email, iter->email);
  EXPECT_TRUE(account_mapping1.access_token.empty());
  EXPECT_EQ(account_mapping1.status, iter->status);
  EXPECT_EQ(account_mapping1.status_change_timestamp,
            iter->status_change_timestamp);
  EXPECT_TRUE(account_mapping1.last_message_id.empty());

  ++iter;
  EXPECT_EQ(account_mapping2.account_id, iter->account_id);
  EXPECT_EQ(account_mapping2.email, iter->email);
  EXPECT_TRUE(account_mapping2.access_token.empty());
  EXPECT_EQ(account_mapping2.status, iter->status);
  EXPECT_EQ(account_mapping2.status_change_timestamp,
            iter->status_change_timestamp);
  EXPECT_EQ(account_mapping2.last_message_id, iter->last_message_id);
}

// Tests that account tokens are not processed until registration ID is
// available.
TEST_F(GCMAccountMapperTest, SetAccountTokensOnlyWorksWithRegisterationId) {
  // Start with empty list.
  Initialize(GCMAccountMapper::AccountMappings());

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  account_tokens.push_back(MakeAccountTokenInfo(kAccountId));
  mapper()->SetAccountTokens(account_tokens);

  EXPECT_TRUE(GetAccounts().empty());

  account_tokens.clear();
  account_tokens.push_back(MakeAccountTokenInfo(kAccountId1));
  account_tokens.push_back(MakeAccountTokenInfo(kAccountId2));
  mapper()->SetAccountTokens(account_tokens);

  EXPECT_TRUE(GetAccounts().empty());

  gcm_driver().CompleteRegister(kRegistrationId, GCMClient::SUCCESS);

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  EXPECT_EQ(2UL, mappings.size());
  EXPECT_EQ(kAccountId1, mappings[0].account_id);
  EXPECT_EQ(kAccountId2, mappings[1].account_id);
}

/// Tests a case when ADD message times out for a MAPPED account.
TEST_F(GCMAccountMapperTest, AddMappingMessageSendErrorForMappedAccount) {
  // Start with one account that is mapped.
  base::Time status_change_timestamp = base::Time::Now();
  AccountMapping mapping =
      MakeAccountMapping(kAccountId, AccountMapping::MAPPED,
                         status_change_timestamp, "add_message_id");

  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(mapping);
  Initialize(stored_mappings);
  gcm_driver().AddAppHandler(kGCMAccountMapperAppId, mapper());
  gcm_driver().CompleteRegister(kRegistrationId, GCMClient::SUCCESS);

  clock()->SetNow(base::Time::Now());
  gcm_driver().MessageSendError("add_message_id");

  // No new message is sent because of the send error, as the token is stale.
  // Because the account was new, the entry should be deleted.
  EXPECT_TRUE(gcm_driver().last_message_id().empty());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ(mapping.email, iter->email);
  EXPECT_EQ(mapping.account_id, iter->account_id);
  EXPECT_EQ(mapping.access_token, iter->access_token);
  EXPECT_EQ(AccountMapping::MAPPED, iter->status);
  EXPECT_EQ(status_change_timestamp, iter->status_change_timestamp);
  EXPECT_TRUE(iter->last_message_id.empty());
}

// Tests that account removing proceeds, when a removing message is acked after
// Chrome was restarted.
TEST_F(GCMAccountMapperTest, RemoveMappingMessageAckedAfterRestart) {
  // Start with one account that is mapped.
  AccountMapping mapping =
      MakeAccountMapping(kAccountId, AccountMapping::REMOVING,
                         base::Time::Now(), "remove_message_id");

  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(mapping);
  Initialize(stored_mappings);
  gcm_driver().AddAppHandler(kGCMAccountMapperAppId, mapper());

  gcm_driver().AcknowledgeSend("remove_message_id");

  EXPECT_EQ(mapping.account_id, gcm_driver().last_removed_account_id());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  EXPECT_TRUE(mappings.empty());
}

// Tests that account removing proceeds, when a removing message is acked after
// Chrome was restarted.
TEST_F(GCMAccountMapperTest, RemoveMappingMessageSendError) {
  // Start with one account that is mapped.
  base::Time status_change_timestamp = base::Time::Now();
  AccountMapping mapping =
      MakeAccountMapping(kAccountId, AccountMapping::REMOVING,
                         status_change_timestamp, "remove_message_id");

  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(mapping);
  Initialize(stored_mappings);
  gcm_driver().AddAppHandler(kGCMAccountMapperAppId, mapper());

  clock()->SetNow(base::Time::Now());
  gcm_driver().MessageSendError("remove_message_id");

  EXPECT_TRUE(gcm_driver().last_removed_account_id().empty());

  EXPECT_EQ(mapping.account_id, gcm_driver().last_account_mapping().account_id);
  EXPECT_EQ(mapping.email, gcm_driver().last_account_mapping().email);
  EXPECT_EQ(AccountMapping::REMOVING,
            gcm_driver().last_account_mapping().status);
  EXPECT_EQ(status_change_timestamp,
            gcm_driver().last_account_mapping().status_change_timestamp);
  // Message is not persisted, until send is completed.
  EXPECT_TRUE(gcm_driver().last_account_mapping().last_message_id.empty());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ(mapping.email, iter->email);
  EXPECT_EQ(mapping.account_id, iter->account_id);
  EXPECT_TRUE(iter->access_token.empty());
  EXPECT_EQ(AccountMapping::REMOVING, iter->status);
  EXPECT_EQ(status_change_timestamp, iter->status_change_timestamp);
  EXPECT_TRUE(iter->last_message_id.empty());
}

TEST_F(GCMAccountMapperTest, DispatchMessageSentToGaiaID) {
  Initialize(GCMAccountMapper::AccountMappings());
  gcm_driver().AddAppHandler(kGCMAccountMapperAppId, mapper());
  IncomingMessage message;
  message.data[kEmbeddedAppIdKey] = kTestAppId;
  message.data[kTestDataKey] = kTestDataValue;
  message.collapse_key = kTestCollapseKey;
  message.sender_id = kTestSenderId;
  mapper()->OnMessage(kGCMAccountMapperAppId, message);

  EXPECT_EQ(kTestAppId, last_received_app_id());
  EXPECT_EQ(1UL, last_received_message().data.size());
  auto it = last_received_message().data.find(kTestDataKey);
  EXPECT_TRUE(it != last_received_message().data.end());
  EXPECT_EQ(kTestDataValue, it->second);
  EXPECT_EQ(kTestCollapseKey, last_received_message().collapse_key);
  EXPECT_EQ(kTestSenderId, last_received_message().sender_id);
}

}  // namespace gcm
