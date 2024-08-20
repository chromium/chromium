// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals/chrome_sync_internals_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/service/sync_internals_util.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/mock_sync_invalidations_service.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::sync_ui_util::kGetAllNodes;
using syncer::sync_ui_util::kRequestDataAndRegisterForUpdates;
using syncer::sync_ui_util::kWriteUserEvent;

namespace {

const char kChannel[] = "canary";

// TODO(crbug.com/360321896): Move to //components and test the cross-platform
// class instead. That's also why the file and fixture weren't renamed yet.
class TestableSyncInternalsMessageHandler
    : public ChromeSyncInternalsMessageHandler {
 public:
  TestableSyncInternalsMessageHandler(
      AboutSyncDataDelegate about_sync_data_delegate,
      syncer::SyncService* sync_service,
      syncer::SyncInvalidationsService* sync_invalidations_service,
      syncer::UserEventService* user_event_service)
      : ChromeSyncInternalsMessageHandler(std::move(about_sync_data_delegate),
                                          sync_service,
                                          sync_invalidations_service,
                                          user_event_service,
                                          kChannel) {}

  using ChromeSyncInternalsMessageHandler::GetMessageHandlerMap;
};

class SyncInternalsMessageHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  SyncInternalsMessageHandlerTest() = default;

  SyncInternalsMessageHandlerTest(const SyncInternalsMessageHandlerTest&) =
      delete;
  SyncInternalsMessageHandlerTest& operator=(
      const SyncInternalsMessageHandlerTest&) = delete;

  ~SyncInternalsMessageHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(*mock_sync_service(), GetEntityCountsForDebugging)
        .WillByDefault(base::test::RunCallback<0>(
            syncer::TypeEntitiesCount(syncer::PASSWORDS)));
    web_ui_->set_web_contents(web_contents());
    handler_ = new TestableSyncInternalsMessageHandler(
        base::BindRepeating(
            &SyncInternalsMessageHandlerTest::ConstructFakeAboutInformation,
            base::Unretained(this)),
        &mock_sync_service_, &mock_sync_invalidations_service_,
        &fake_user_event_service_);
    web_ui_->AddMessageHandler(
        std::unique_ptr<content::WebUIMessageHandler>(handler_));
    // In production, RegisterMessages() ensures that AllowJavascript() is
    // called before any Handle*() method. But these tests are calling the
    // handlers directly, so enable javascript here to avoid crashes.
    handler_->AllowJavascriptForTesting();
  }

  void TearDown() override {
    // Destroy |handler_| before |web_contents()|.
    handler_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }

  syncer::MockSyncService* mock_sync_service() { return &mock_sync_service_; }

  syncer::MockSyncInvalidationsService* mock_sync_invalidations_service() {
    return &mock_sync_invalidations_service_;
  }

  syncer::FakeUserEventService* fake_user_event_service() {
    return &fake_user_event_service_;
  }

  TestableSyncInternalsMessageHandler* handler() { return handler_.get(); }

  int CallCountWithName(const std::string& function_name) {
    int count = 0;
    for (const auto& call_data : web_ui_->call_data()) {
      if (call_data->function_name() == function_name) {
        count++;
      }
    }
    return count;
  }

  int about_sync_data_delegate_call_count() const {
    return about_sync_data_delegate_call_count_;
  }

  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>& call_data()
      const {
    return web_ui_->call_data();
  }

  void ResetHandler() {
    handler_ = nullptr;
    web_ui_.reset();
  }

  // Fake return value for sync_ui_util::ConstructAboutInformation().
  const base::Value::Dict kAboutInformation =
      base::Value::Dict().Set("some_sync_state", "some_value");

 private:
  // Returns copies of the same constant dictionary, |kAboutInformation|.
  base::Value::Dict ConstructFakeAboutInformation(syncer::SyncService* service,
                                                  const std::string& channel) {
    ++about_sync_data_delegate_call_count_;
    return kAboutInformation.Clone();
  }

  std::unique_ptr<content::TestWebUI> web_ui_ =
      std::make_unique<content::TestWebUI>();
  syncer::MockSyncService mock_sync_service_;
  syncer::MockSyncInvalidationsService mock_sync_invalidations_service_;
  syncer::FakeUserEventService fake_user_event_service_;
  raw_ptr<TestableSyncInternalsMessageHandler> handler_ = nullptr;
  int about_sync_data_delegate_call_count_ = 0;
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

TEST_F(SyncInternalsMessageHandlerTest, AddRemoveObserversDisallowJavascript) {
  EXPECT_CALL(*mock_sync_service(), AddObserver);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver).Times(0);
  handler()
      ->GetMessageHandlerMap()
      .at(kRequestDataAndRegisterForUpdates)
      .Run(base::Value::List());
  testing::Mock::VerifyAndClearExpectations(mock_sync_service());

  EXPECT_CALL(*mock_sync_service(), AddObserver).Times(0);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver);
  handler()->DisallowJavascript();
  testing::Mock::VerifyAndClearExpectations(mock_sync_service());

  // Deregistration should not repeat, no counts should increase.
  EXPECT_CALL(*mock_sync_service(), AddObserver).Times(0);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver).Times(0);
  ResetHandler();
}

TEST_F(SyncInternalsMessageHandlerTest, AddRemoveObserversSyncDisabled) {
  // Simulate completely disabling sync by flag or other mechanism.
  auto* handler = new TestableSyncInternalsMessageHandler(
      base::BindLambdaForTesting([&](syncer::SyncService*, const std::string&) {
        return kAboutInformation.Clone();
      }),
      /*sync_service=*/nullptr, mock_sync_invalidations_service(),
      fake_user_event_service());
  web_ui()->AddMessageHandler(
      std::unique_ptr<content::WebUIMessageHandler>(handler));
  handler->AllowJavascriptForTesting();
  handler->GetMessageHandlerMap()
      .at(kRequestDataAndRegisterForUpdates)
      .Run(base::Value::List());
  handler->DisallowJavascript();
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
  std::move(get_all_nodes_callback).Run(base::Value::List());
  EXPECT_EQ(1, CallCountWithName("cr.webUIResponse"));

  handler()
      ->GetMessageHandlerMap()
      .at(kGetAllNodes)
      .Run(base::Value::List().Append("getAllNodes_1"));
  // This  breaks the weak ref the callback is hanging onto. Which results in
  // the call count not incrementing.
  handler()->DisallowJavascript();
  std::move(get_all_nodes_callback).Run(base::Value::List());
  EXPECT_EQ(1, CallCountWithName("cr.webUIResponse"));

  handler()->AllowJavascriptForTesting();
  handler()
      ->GetMessageHandlerMap()
      .at(kGetAllNodes)
      .Run(base::Value::List().Append("getAllNodes_2"));
  std::move(get_all_nodes_callback).Run(base::Value::List());
  EXPECT_EQ(2, CallCountWithName("cr.webUIResponse"));
}

TEST_F(SyncInternalsMessageHandlerTest, SendAboutInfo) {
  handler()->AllowJavascriptForTesting();
  static_cast<syncer::SyncServiceObserver*>(handler())->OnStateChanged(
      mock_sync_service());
  EXPECT_EQ(1, about_sync_data_delegate_call_count());

  // There should be one kOnAboutInfoUpdated event, and one
  // kOnEntityCountsUpdated event (because TestSyncService responds with the
  // entity count for a single data type).
  ASSERT_EQ(2u, call_data().size());

  // Check the syncer::sync_ui_util::kOnAboutInfoUpdated event dispatch.
  const content::TestWebUI::CallData& about_info_call_data = *call_data()[0];
  EXPECT_EQ("cr.webUIListenerCallback", about_info_call_data.function_name());
  ASSERT_NE(nullptr, about_info_call_data.arg1());
  EXPECT_EQ(base::Value(syncer::sync_ui_util::kOnAboutInfoUpdated),
            *about_info_call_data.arg1());
  ASSERT_NE(nullptr, about_info_call_data.arg2());
  EXPECT_EQ(kAboutInformation, *about_info_call_data.arg2());

  // TestSyncService::GetEntityCountsForDebugging() responds synchronously and
  // for a single data type, so check for a single
  // syncer::sync_ui_util::kOnEntityCountsUpdated event dispatch.
  const content::TestWebUI::CallData& entity_counts_updated_call_data =
      *call_data()[1];
  EXPECT_EQ("cr.webUIListenerCallback",
            entity_counts_updated_call_data.function_name());
  ASSERT_NE(nullptr, entity_counts_updated_call_data.arg1());
  EXPECT_EQ(base::Value(syncer::sync_ui_util::kOnEntityCountsUpdated),
            *entity_counts_updated_call_data.arg1());
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

}  // namespace
