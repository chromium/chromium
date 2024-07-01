// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals/sync_internals_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/service/sync_internals_util.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"

using sync_pb::UserEventSpecifics;
using syncer::FakeUserEventService;
using syncer::SyncService;
using syncer::SyncServiceObserver;

namespace {

class TestableSyncInternalsMessageHandler : public SyncInternalsMessageHandler {
 public:
  TestableSyncInternalsMessageHandler(
      content::WebUI* web_ui,
      AboutSyncDataDelegate about_sync_data_delegate)
      : SyncInternalsMessageHandler(std::move(about_sync_data_delegate)) {
    set_web_ui(web_ui);
  }
};

static std::unique_ptr<KeyedService> BuildMockSyncService(
    content::BrowserContext* context) {
  auto sync_service = std::make_unique<syncer::MockSyncService>();
  ON_CALL(*sync_service, GetEntityCountsForDebugging)
      .WillByDefault(base::test::RunCallback<0>(
          syncer::TypeEntitiesCount(syncer::PASSWORDS)));
  return sync_service;
}

static std::unique_ptr<KeyedService> BuildFakeUserEventService(
    content::BrowserContext* context) {
  return std::make_unique<FakeUserEventService>();
}

class SyncInternalsMessageHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  SyncInternalsMessageHandlerTest(const SyncInternalsMessageHandlerTest&) =
      delete;
  SyncInternalsMessageHandlerTest& operator=(
      const SyncInternalsMessageHandlerTest&) = delete;

 protected:
  SyncInternalsMessageHandlerTest() = default;
  ~SyncInternalsMessageHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    about_information_.Set("some_sync_state", "some_value");

    web_ui_.set_web_contents(web_contents());
    fake_user_event_service_ = static_cast<FakeUserEventService*>(
        browser_sync::UserEventServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(), base::BindRepeating(&BuildFakeUserEventService)));
    handler_ = std::make_unique<TestableSyncInternalsMessageHandler>(
        &web_ui_,
        base::BindRepeating(
            &SyncInternalsMessageHandlerTest::ConstructFakeAboutInformation,
            base::Unretained(this)));
  }

  void TearDown() override {
    // Destroy |handler_| before |web_contents()|.
    handler_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockSyncService)}};
  }

  // Returns copies of the same constant dictionary, |about_information_|.
  base::Value::Dict ConstructFakeAboutInformation(SyncService* service,
                                                  const std::string& channel) {
    ++about_sync_data_delegate_call_count_;
    last_delegate_sync_service_ = service;
    return about_information_.Clone();
  }

  syncer::MockSyncService* mock_sync_service() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  FakeUserEventService* fake_user_event_service() {
    return fake_user_event_service_;
  }

  TestableSyncInternalsMessageHandler* handler() { return handler_.get(); }

  int CallCountWithName(const std::string& function_name) {
    int count = 0;
    for (const auto& call_data : web_ui_.call_data()) {
      if (call_data->function_name() == function_name) {
        count++;
      }
    }
    return count;
  }

  int about_sync_data_delegate_call_count() const {
    return about_sync_data_delegate_call_count_;
  }

  const SyncService* last_delegate_sync_service() const {
    return last_delegate_sync_service_;
  }

  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>& call_data()
      const {
    return web_ui_.call_data();
  }

  const base::Value::Dict& about_information() { return about_information_; }

  void ResetHandler() { handler_.reset(); }

 private:
  content::TestWebUI web_ui_;
  raw_ptr<FakeUserEventService, DanglingUntriaged> fake_user_event_service_ =
      nullptr;
  std::unique_ptr<TestableSyncInternalsMessageHandler> handler_;
  int about_sync_data_delegate_call_count_ = 0;
  raw_ptr<SyncService, DanglingUntriaged> last_delegate_sync_service_ = nullptr;
  // Fake return value for sync_ui_util::ConstructAboutInformation().
  base::Value::Dict about_information_;
};

TEST_F(SyncInternalsMessageHandlerTest, AddRemoveObservers) {
  EXPECT_CALL(*mock_sync_service(), AddObserver);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver).Times(0);
  handler()->HandleRequestDataAndRegisterForUpdates(base::Value::List());
  testing::Mock::VerifyAndClearExpectations(mock_sync_service());

  EXPECT_CALL(*mock_sync_service(), AddObserver).Times(0);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver);
  ResetHandler();
}

TEST_F(SyncInternalsMessageHandlerTest, AddRemoveObserversDisallowJavascript) {
  EXPECT_CALL(*mock_sync_service(), AddObserver);
  EXPECT_CALL(*mock_sync_service(), RemoveObserver).Times(0);
  handler()->HandleRequestDataAndRegisterForUpdates(base::Value::List());
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
  SyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), BrowserContextKeyedServiceFactory::TestingFactory());

  handler()->HandleRequestDataAndRegisterForUpdates(base::Value::List());
  handler()->DisallowJavascript();
  // Cannot verify observer methods on sync services were not called, because
  // there is no sync service. Rather, we're just making sure the handler hasn't
  // performed any invalid operations when the sync service is missing.
}

TEST_F(SyncInternalsMessageHandlerTest, HandleGetAllNodes) {
  base::Value::List args;
  args.Append("getAllNodes_0");
  base::OnceCallback<void(base::Value::List)> get_all_nodes_callback;
  ON_CALL(*mock_sync_service(), GetAllNodesForDebugging)
      .WillByDefault(MoveArg<0>(&get_all_nodes_callback));
  handler()->HandleGetAllNodes(args);
  std::move(get_all_nodes_callback).Run(base::Value::List());
  EXPECT_EQ(1, CallCountWithName("cr.webUIResponse"));

  base::Value::List args2;
  args2.Append("getAllNodes_1");
  handler()->HandleGetAllNodes(args2);
  // This  breaks the weak ref the callback is hanging onto. Which results in
  // the call count not incrementing.
  handler()->DisallowJavascript();
  std::move(get_all_nodes_callback).Run(base::Value::List());
  EXPECT_EQ(1, CallCountWithName("cr.webUIResponse"));

  base::Value::List args3;
  args3.Append("getAllNodes_2");
  handler()->HandleGetAllNodes(args3);
  std::move(get_all_nodes_callback).Run(base::Value::List());
  EXPECT_EQ(2, CallCountWithName("cr.webUIResponse"));
}

TEST_F(SyncInternalsMessageHandlerTest, SendAboutInfo) {
  handler()->AllowJavascriptForTesting();
  handler()->OnStateChanged(nullptr);
  EXPECT_EQ(1, about_sync_data_delegate_call_count());
  EXPECT_NE(nullptr, last_delegate_sync_service());

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
  EXPECT_EQ(about_information(), *about_info_call_data.arg2());

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

TEST_F(SyncInternalsMessageHandlerTest, SendAboutInfoSyncDisabled) {
  // Simulate completely disabling sync by flag or other mechanism.
  SyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), BrowserContextKeyedServiceFactory::TestingFactory());

  handler()->AllowJavascriptForTesting();
  handler()->OnStateChanged(nullptr);
  EXPECT_EQ(1, about_sync_data_delegate_call_count());
  EXPECT_EQ(nullptr, last_delegate_sync_service());

  // There should be one kOnAboutInfoUpdated event (sent by the MessageHandler
  // even if there's no SyncService), but no kOnEntityCountsUpdated events.
  ASSERT_EQ(1u, call_data().size());

  // Check the syncer::sync_ui_util::kOnAboutInfoUpdated event dispatch.
  const content::TestWebUI::CallData& about_info_call_data = *call_data()[0];
  EXPECT_EQ("cr.webUIListenerCallback", about_info_call_data.function_name());
  ASSERT_NE(nullptr, about_info_call_data.arg1());
  EXPECT_EQ(base::Value(syncer::sync_ui_util::kOnAboutInfoUpdated),
            *about_info_call_data.arg1());
  ASSERT_NE(nullptr, about_info_call_data.arg2());
  EXPECT_EQ(about_information(), *about_info_call_data.arg2());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEvent) {
  base::Value::List args;
  args.Append("1000000000000000000");
  args.Append("-1");
  handler()->HandleWriteUserEvent(args);

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_EQ(1000000000000000000, event.event_time_usec());
  EXPECT_EQ(-1, event.navigation_id());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEventBadParse) {
  base::Value::List args;
  args.Append("123abc");
  args.Append("abcdefghijklmnopqrstuvwxyz");
  handler()->HandleWriteUserEvent(args);

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_EQ(0, event.event_time_usec());
  EXPECT_EQ(0, event.navigation_id());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEventBlank) {
  base::Value::List args;
  args.Append("");
  args.Append("");
  handler()->HandleWriteUserEvent(args);

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_TRUE(event.has_event_time_usec());
  EXPECT_EQ(0, event.event_time_usec());
  // Should not have a navigation_id because that means something different to
  // the UserEvents logic.
  EXPECT_FALSE(event.has_navigation_id());
}

TEST_F(SyncInternalsMessageHandlerTest, WriteUserEventZero) {
  base::Value::List args;
  args.Append("0");
  args.Append("0");
  handler()->HandleWriteUserEvent(args);

  ASSERT_EQ(1u, fake_user_event_service()->GetRecordedUserEvents().size());
  const UserEventSpecifics& event =
      *fake_user_event_service()->GetRecordedUserEvents().begin();
  EXPECT_EQ(UserEventSpecifics::kTestEvent, event.event_case());
  EXPECT_TRUE(event.has_event_time_usec());
  EXPECT_EQ(0, event.event_time_usec());
  // Should have a navigation_id, even though the value is 0.
  EXPECT_TRUE(event.has_navigation_id());
  EXPECT_EQ(0, event.navigation_id());
}

}  // namespace
