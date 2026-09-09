// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/critical_actions/critical_actions_page_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/features.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace critical_actions {

class CriticalActionsPageHandlerTest : public testing::Test {
 public:
  CriticalActionsPageHandlerTest() {
    feature_list_.InitAndEnableFeature(features::kCriticalActionHistory);
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    service_ = CriticalActionFactory::GetForProfile(profile_.get());
    ASSERT_NE(service_, nullptr);

    handler_ = std::make_unique<CriticalActionsPageHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(), profile_.get());
  }

  void TearDown() override {
    handler_.reset();
    handler_remote_.reset();
    service_ = nullptr;
    profile_.reset();
  }

  CriticalActionEntry CreateAction(
      std::string id,
      base::Time timestamp,
      int64_t visit_id,
      ActionType type = ActionType::kFormFill,
      ActionSource source = ActionSource::kAutofill,
      std::string conversation_id = "conv-123",
      std::string actor_task_id = "task-456",
      GURL url = GURL("https://example.com/login"),
      std::string metadata = "{\"field\":\"username\"}") {
    CriticalActionEntry entry;
    entry.critical_action_id = std::move(id);
    entry.timestamp = timestamp;
    entry.visit_id = visit_id;
    entry.action_type = type;
    entry.action_source = source;
    entry.conversation_id = std::move(conversation_id);
    entry.actor_task_id = std::move(actor_task_id);
    entry.url = std::move(url);
    entry.metadata = std::move(metadata);
    return entry;
  }

  CriticalActionService* service() { return service_; }
  CriticalActionsPageHandler* handler() { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CriticalActionsPageHandler> handler_;
  mojo::Remote<mojom::PageHandler> handler_remote_;
  raw_ptr<CriticalActionService> service_ = nullptr;
};

TEST_F(CriticalActionsPageHandlerTest, GetCriticalActionsEmpty) {
  base::test::TestFuture<mojom::CriticalActionsQueryResultPtr> future;
  handler()->GetCriticalActions(0, 10, std::nullopt, std::nullopt,
                                future.GetCallback());
  auto result = future.Take();
  ASSERT_TRUE(result->is_list());
  const auto& list = result->get_list();
  EXPECT_EQ(list->total_entries, 0u);
  EXPECT_TRUE(list->entries.empty());
  EXPECT_EQ(list->page_index, 0u);
  EXPECT_EQ(list->page_size, 10u);
}

TEST_F(CriticalActionsPageHandlerTest, GetCriticalActionsPagination) {
  base::Time now = base::Time::Now();
  for (int i = 0; i < 5; ++i) {
    service()->AddCriticalAction(CreateAction(base::StringPrintf("id-%02d", i),
                                              now + base::Seconds(i), 100 + i,
                                              ActionType::kFormFill));
  }

  // First page, page size 2
  {
    base::test::TestFuture<mojom::CriticalActionsQueryResultPtr> future;
    handler()->GetCriticalActions(0, 2, std::nullopt, std::nullopt,
                                  future.GetCallback());
    auto result = future.Take();
    ASSERT_TRUE(result->is_list());
    const auto& list = result->get_list();
    EXPECT_EQ(list->total_entries, 5u);
    EXPECT_EQ(list->entries.size(), 2u);
    EXPECT_EQ(list->page_index, 0u);
    EXPECT_EQ(list->page_size, 2u);
  }

  // Second page, page size 2
  {
    base::test::TestFuture<mojom::CriticalActionsQueryResultPtr> future;
    handler()->GetCriticalActions(1, 2, std::nullopt, std::nullopt,
                                  future.GetCallback());
    auto result = future.Take();
    ASSERT_TRUE(result->is_list());
    const auto& list = result->get_list();
    EXPECT_EQ(list->total_entries, 5u);
    EXPECT_EQ(list->entries.size(), 2u);
    EXPECT_EQ(list->page_index, 1u);
  }

  // Third page, page size 2 (only 1 item remaining)
  {
    base::test::TestFuture<mojom::CriticalActionsQueryResultPtr> future;
    handler()->GetCriticalActions(2, 2, std::nullopt, std::nullopt,
                                  future.GetCallback());
    auto result = future.Take();
    ASSERT_TRUE(result->is_list());
    const auto& list = result->get_list();
    EXPECT_EQ(list->total_entries, 5u);
    EXPECT_EQ(list->entries.size(), 1u);
    EXPECT_EQ(list->page_index, 2u);
  }

  // Out-of-bounds page index clamps to last valid page (page 2)
  {
    base::test::TestFuture<mojom::CriticalActionsQueryResultPtr> future;
    handler()->GetCriticalActions(10, 2, std::nullopt, std::nullopt,
                                  future.GetCallback());
    auto result = future.Take();
    ASSERT_TRUE(result->is_list());
    const auto& list = result->get_list();
    EXPECT_EQ(list->total_entries, 5u);
    EXPECT_EQ(list->entries.size(), 1u);
    EXPECT_EQ(list->page_index, 2u);
  }
}

TEST_F(CriticalActionsPageHandlerTest, FilterByActionTypeAndSearchQuery) {
  base::Time now = base::Time::Now();
  service()->AddCriticalAction(CreateAction(
      "id-ff", now, 101, ActionType::kFormFill, ActionSource::kAutofill,
      "conv-1", "task-1", GURL("https://forms.example.com")));
  service()->AddCriticalAction(
      CreateAction("id-dl", now + base::Seconds(1), 102, ActionType::kDownload,
                   ActionSource::kActor, "conv-2", "task-2",
                   GURL("https://download.example.com/file.zip")));

  // Filter by Download action type (kDownload = 2)
  {
    base::test::TestFuture<mojom::CriticalActionsQueryResultPtr> future;
    handler()->GetCriticalActions(0, 10, std::nullopt,
                                  static_cast<int32_t>(ActionType::kDownload),
                                  future.GetCallback());
    auto result = future.Take();
    ASSERT_TRUE(result->is_list());
    const auto& list = result->get_list();
    EXPECT_EQ(list->total_entries, 1u);
    ASSERT_EQ(list->entries.size(), 1u);
    EXPECT_EQ(list->entries[0]->critical_action_id, "id-dl");
    EXPECT_EQ(list->entries[0]->action_type_str, "Download");
  }

  // Search by keyword "file.zip"
  {
    base::test::TestFuture<mojom::CriticalActionsQueryResultPtr> future;
    handler()->GetCriticalActions(0, 10, "file.zip", std::nullopt,
                                  future.GetCallback());
    auto result = future.Take();
    ASSERT_TRUE(result->is_list());
    const auto& list = result->get_list();
    EXPECT_EQ(list->total_entries, 1u);
    ASSERT_EQ(list->entries.size(), 1u);
    EXPECT_EQ(list->entries[0]->critical_action_id, "id-dl");
  }
}

TEST_F(CriticalActionsPageHandlerTest, DeleteAndClearAll) {
  base::Time now = base::Time::Now();
  service()->AddCriticalAction(CreateAction("id-1", now, 101));
  service()->AddCriticalAction(
      CreateAction("id-2", now + base::Seconds(1), 102));

  // Delete id-1
  {
    base::test::TestFuture<bool> delete_future;
    handler()->DeleteCriticalAction("id-1", delete_future.GetCallback());
    EXPECT_TRUE(delete_future.Get());

    base::test::TestFuture<mojom::CriticalActionsQueryResultPtr> future;
    handler()->GetCriticalActions(0, 10, std::nullopt, std::nullopt,
                                  future.GetCallback());
    auto result = future.Take();
    ASSERT_TRUE(result->is_list());
    const auto& list = result->get_list();
    EXPECT_EQ(list->total_entries, 1u);
    ASSERT_EQ(list->entries.size(), 1u);
    EXPECT_EQ(list->entries[0]->critical_action_id, "id-2");
  }

  // Clear all
  {
    base::test::TestFuture<bool> clear_future;
    handler()->ClearAllCriticalActions(clear_future.GetCallback());
    EXPECT_TRUE(clear_future.Get());

    base::test::TestFuture<mojom::CriticalActionsQueryResultPtr> future;
    handler()->GetCriticalActions(0, 10, std::nullopt, std::nullopt,
                                  future.GetCallback());
    auto result = future.Take();
    ASSERT_TRUE(result->is_list());
    const auto& list = result->get_list();
    EXPECT_EQ(list->total_entries, 0u);
  }
}

TEST_F(CriticalActionsPageHandlerTest, FeatureDisabledReturnsError) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kCriticalActionHistory);

  auto empty_profile = std::make_unique<TestingProfile>();
  mojo::Remote<mojom::PageHandler> remote;
  CriticalActionsPageHandler no_service_handler(
      remote.BindNewPipeAndPassReceiver(), empty_profile.get());

  base::test::TestFuture<mojom::CriticalActionsQueryResultPtr> future;
  no_service_handler.GetCriticalActions(0, 10, std::nullopt, std::nullopt,
                                        future.GetCallback());
  auto result = future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), mojom::CriticalActionsError::kFeatureDisabled);
}

}  // namespace critical_actions
