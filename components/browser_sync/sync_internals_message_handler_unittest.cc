// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_internals_message_handler.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/service/sync_internals_util.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/mock_sync_invalidations_service.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::sync_ui_util::kGetAllNodes;
using syncer::sync_ui_util::kOnAboutInfoUpdated;
using syncer::sync_ui_util::kOnEntityCountsUpdated;
using syncer::sync_ui_util::kRequestDataAndRegisterForUpdates;
using syncer::sync_ui_util::kRequestStart;
using syncer::sync_ui_util::kWriteUserEvent;
using testing::_;
using testing::ElementsAre;
using testing::Return;

namespace browser_sync {
namespace {

const char kChannel[] = "canary";

// Matches a given base::ValueView against `dict`.
MATCHER_P(ValueViewMatchesDict, dict, "") {
  base::Value value = arg.ToValue();
  return value.is_dict() && value.GetDict() == dict;
}

class MockDelegate : public SyncInternalsMessageHandler::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              SendEventToPage,
              (std::string_view, base::span<const base::ValueView>),
              (override));
  MOCK_METHOD(void,
              ResolvePageCallback,
              (const base::ValueView, const base::ValueView),
              (override));
};

class SyncInternalsMessageHandlerTest : public testing::Test {
 public:
  SyncInternalsMessageHandlerTest() {
    ON_CALL(mock_sync_service_, GetEntityCountsForDebugging)
        .WillByDefault(base::test::RunCallback<0>(
            syncer::TypeEntitiesCount(syncer::PASSWORDS)));
  }

  SyncInternalsMessageHandlerTest(const SyncInternalsMessageHandlerTest&) =
      delete;
  SyncInternalsMessageHandlerTest& operator=(
      const SyncInternalsMessageHandlerTest&) = delete;

  ~SyncInternalsMessageHandlerTest() override = default;

  MockDelegate* mock_delegate() { return &mock_delegate_; }

  signin::IdentityTestEnvironment* identity_test_environment() {
    return &identity_test_environment_;
  }

  syncer::MockSyncService* mock_sync_service() { return &mock_sync_service_; }

  syncer::MockSyncInvalidationsService* mock_sync_invalidations_service() {
    return &mock_sync_invalidations_service_;
  }

  syncer::FakeUserEventService* fake_user_event_service() {
    return &fake_user_event_service_;
  }

  SyncInternalsMessageHandler* handler() { return handler_.get(); }

  int get_about_sync_data_call_count() const {
    return get_about_sync_data_dall_count_;
  }

  void ResetHandler() { handler_.reset(); }

  // Fake return value for sync_ui_util::ConstructAboutInformation().
  const base::Value::Dict kAboutInformation =
      base::Value::Dict().Set("some_sync_state", "some_value");

 private:
  // Returns copies of the same constant dictionary, |kAboutInformation|.
  base::Value::Dict ConstructFakeAboutInformation(syncer::SyncService* service,
                                                  const std::string& channel) {
    ++get_about_sync_data_dall_count_;
    return kAboutInformation.Clone();
  }

  // SingleThreadTaskEnvironment is needed for IdentityTestEnvironment.
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockDelegate mock_delegate_;
  signin::IdentityTestEnvironment identity_test_environment_;
  syncer::MockSyncService mock_sync_service_;
  syncer::MockSyncInvalidationsService mock_sync_invalidations_service_;
  syncer::FakeUserEventService fake_user_event_service_;
  std::unique_ptr<SyncInternalsMessageHandler> handler_ =
      std::make_unique<SyncInternalsMessageHandler>(
          &mock_delegate_,
          base::BindRepeating(
              &SyncInternalsMessageHandlerTest::ConstructFakeAboutInformation,
              base::Unretained(this)),
          identity_test_environment_.identity_manager(),
          &mock_sync_service_,
          &mock_sync_invalidations_service_,
          &fake_user_event_service_,
          kChannel);
  int get_about_sync_data_dall_count_ = 0;
};

TEST_F(SyncInternalsMessageHandlerTest, AddRemoveObservers) {
  EXPECT_CALL(*mock_sync_service(), AddObserver);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver).Times(0);
  handler()
      ->GetMessageHandlerMap()
      .at(kRequestDataAndRegisterForUpdates)
      .Run(base::Value::List());
  testing::Mock::VerifyAndClearExpectations(mock_sync_service());

  EXPECT_CALL(*mock_sync_service(), AddObserver).Times(0);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver);
  ResetHandler();
}

TEST_F(SyncInternalsMessageHandlerTest, AddRemoveObserversDisableMessages) {
  EXPECT_CALL(*mock_sync_service(), AddObserver);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver).Times(0);
  handler()
      ->GetMessageHandlerMap()
      .at(kRequestDataAndRegisterForUpdates)
      .Run(base::Value::List());
  testing::Mock::VerifyAndClearExpectations(mock_sync_service());

  EXPECT_CALL(*mock_sync_service(), AddObserver).Times(0);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver);
  handler()->DisableMessagesToPage();
  testing::Mock::VerifyAndClearExpectations(mock_sync_service());

  // Deregistration should not repeat, no counts should increase.
  EXPECT_CALL(*mock_sync_service(), AddObserver).Times(0);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver).Times(0);
  ResetHandler();
}

TEST_F(SyncInternalsMessageHandlerTest, AddRemoveObserversSyncDisabled) {
  // Simulate completely disabling sync by flag or other mechanism.
  auto handler = std::make_unique<SyncInternalsMessageHandler>(
      mock_delegate(),
      base::BindLambdaForTesting([&](syncer::SyncService*, const std::string&) {
        return kAboutInformation.Clone();
      }),
      identity_test_environment()->identity_manager(),
      /*sync_service=*/nullptr, mock_sync_invalidations_service(),
      fake_user_event_service(), kChannel);
  handler->GetMessageHandlerMap()
      .at(kRequestDataAndRegisterForUpdates)
      .Run(base::Value::List());
  handler->DisableMessagesToPage();
  // Cannot verify observer methods on sync services were not called, because
  // there is no sync service. Rather, we're just making sure the handler hasn't
  // performed any invalid operations when the sync service is missing.
}

TEST_F(SyncInternalsMessageHandlerTest, HandleGetAllNodes) {
  base::OnceCallback<void(base::Value::List)> get_all_nodes_callback;
  ON_CALL(*mock_sync_service(), GetAllNodesForDebugging)
      .WillByDefault(MoveArg<0>(&get_all_nodes_callback));
  handler()
      ->GetMessageHandlerMap()
      .at(kGetAllNodes)
      .Run(base::Value::List().Append("getAllNodes_0"));
  EXPECT_CALL(*mock_delegate(), ResolvePageCallback);
  std::move(get_all_nodes_callback).Run(base::Value::List());
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  handler()
      ->GetMessageHandlerMap()
      .at(kGetAllNodes)
      .Run(base::Value::List().Append("getAllNodes_1"));
  // This  breaks the weak ref the callback is hanging onto. Which results in
  // the call count not incrementing.
  handler()->DisableMessagesToPage();
  EXPECT_CALL(*mock_delegate(), ResolvePageCallback).Times(0);
  std::move(get_all_nodes_callback).Run(base::Value::List());
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  handler()
      ->GetMessageHandlerMap()
      .at(kGetAllNodes)
      .Run(base::Value::List().Append("getAllNodes_2"));
  EXPECT_CALL(*mock_delegate(), ResolvePageCallback);
  std::move(get_all_nodes_callback).Run(base::Value::List());
}

TEST_F(SyncInternalsMessageHandlerTest, SendAboutInfo) {
  EXPECT_CALL(
      *mock_delegate(),
      SendEventToPage(kOnAboutInfoUpdated, ElementsAre(ValueViewMatchesDict(
                                               std::cref(kAboutInformation)))));
  EXPECT_CALL(*mock_delegate(), SendEventToPage(kOnEntityCountsUpdated, _));
  static_cast<syncer::SyncServiceObserver*>(handler())->OnStateChanged(
      mock_sync_service());
  EXPECT_EQ(1, get_about_sync_data_call_count());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEvent) {
  handler()
      ->GetMessageHandlerMap()
      .at(kWriteUserEvent)
      .Run(base::Value::List().Append("1000000000000000000").Append("-1"));

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(sync_pb::UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_EQ(1000000000000000000, event.event_time_usec());
  EXPECT_EQ(-1, event.navigation_id());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEventBadParse) {
  handler()
      ->GetMessageHandlerMap()
      .at(kWriteUserEvent)
      .Run(base::Value::List().Append("123abc").Append("abcde"));

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(sync_pb::UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_EQ(0, event.event_time_usec());
  EXPECT_EQ(0, event.navigation_id());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEventBlank) {
  handler()
      ->GetMessageHandlerMap()
      .at(kWriteUserEvent)
      .Run(base::Value::List().Append("").Append(""));

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(sync_pb::UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_TRUE(event.has_event_time_usec());
  EXPECT_EQ(0, event.event_time_usec());
  // Should not have a navigation_id because that means something different to
  // the UserEvents logic.
  EXPECT_FALSE(event.has_navigation_id());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEventZero) {
  handler()
      ->GetMessageHandlerMap()
      .at(kWriteUserEvent)
      .Run(base::Value::List().Append("0").Append("0"));

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(sync_pb::UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_TRUE(event.has_event_time_usec());
  EXPECT_EQ(0, event.event_time_usec());
  // Should have a navigation_id, even though the value is 0.
  EXPECT_TRUE(event.has_navigation_id());
  EXPECT_EQ(0, event.navigation_id());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncInternalsMessageHandlerTest, RequestStart) {
  identity_test_environment()->MakePrimaryAccountAvailable(
      "foo@gmail.com", signin::ConsentLevel::kSignin);
  EXPECT_CALL(*mock_sync_service()->GetMockUserSettings(),
              SetInitialSyncFeatureSetupComplete);

  handler()->GetMessageHandlerMap().at(kRequestStart).Run(base::Value::List());

  CoreAccountInfo account_info =
      identity_test_environment()->identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSync);
  EXPECT_FALSE(account_info.IsEmpty());
  EXPECT_EQ(account_info.email, "foo@gmail.com");
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
}  // namespace browser_sync
