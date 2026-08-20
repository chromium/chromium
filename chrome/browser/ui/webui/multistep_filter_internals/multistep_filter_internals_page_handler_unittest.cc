// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_page_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router_impl.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter_internals {
namespace {

class MockPage : public mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnLogEntryAdded,
              (multistep_filter_internals::mojom::LogEntryPtr entry),
              (override));

 private:
  mojo::Receiver<mojom::Page> receiver_{this};
};

class MockFilterStore : public multistep_filter::FilterStore {
 public:
  MockFilterStore() = default;
  ~MockFilterStore() override = default;

  MOCK_METHOD(void,
              DeleteAnnotationsForHosts,
              (std::vector<std::string> hosts,
               base::Time delete_begin,
               base::Time delete_end,
               base::OnceCallback<void(std::optional<int64_t>)> callback),
              (override));
};

class MockMultistepFilterService
    : public multistep_filter::MultistepFilterService {
 public:
  MockMultistepFilterService(
      std::unique_ptr<multistep_filter::AnnotationIndexClient> index_client,
      std::unique_ptr<multistep_filter::FilterStore> filter_store,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager)
      : MultistepFilterService([&]() {
          MultistepFilterService::Params params;
          params.annotation_index_client = std::move(index_client);
          params.filter_store = std::move(filter_store);
          params.identity_manager = identity_manager;
          params.pref_service = pref_service;
          return params;
        }()) {}
  ~MockMultistepFilterService() override = default;

  MOCK_METHOD(multistep_filter::AccountState,
              GetAccountState,
              (),
              (const, override));
  MOCK_METHOD(multistep_filter::ConsentState,
              GetConsentState,
              (),
              (const, override));
  MOCK_METHOD(multistep_filter::SettingsState,
              GetSettingsState,
              (),
              (const, override));
};

class MultistepFilterInternalsPageHandlerTest : public testing::Test {
 public:
  MultistepFilterInternalsPageHandlerTest() = default;

  static constexpr int64_t kTestNavigation1 = 1;
  static constexpr int64_t kTestNavigation2 = 2;
  static constexpr int64_t kTestNavigation3 = 3;
  static constexpr int64_t kTestNavigation4 = 4;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  TestingProfile profile_;
  multistep_filter::MultistepFilterLogRouterImpl log_router_;
  testing::NiceMock<MockPage> page_;
  mojo::Remote<mojom::PageHandler> handler_remote_;
  std::unique_ptr<MultistepFilterInternalsPageHandler> handler_;
};

TEST_F(MultistepFilterInternalsPageHandlerTest, ForwardsLogEntries) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &profile_, &log_router_);

  multistep_filter::LogEntry entry(
      kTestNavigation1, multistep_filter::LogEventType::kUrlEligibilityCheck,
      "example.com");

  multistep_filter_internals::mojom::LogEntryPtr captured_val;
  base::RunLoop run_loop;
  EXPECT_CALL(page_, OnLogEntryAdded(testing::_))
      .WillOnce([&](multistep_filter_internals::mojom::LogEntryPtr val) {
        captured_val = std::move(val);
        run_loop.Quit();
      });

  log_router_.RouteLogMessage(std::move(entry));

  run_loop.Run();

  ASSERT_TRUE(captured_val);
  EXPECT_EQ(captured_val->navigation_id, kTestNavigation1);
  EXPECT_EQ(captured_val->event_type, "Url Eligibility Check");
}

TEST_F(MultistepFilterInternalsPageHandlerTest, GetBufferedLogs_NullRouter) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &profile_, nullptr);

  base::test::TestFuture<
      std::vector<multistep_filter_internals::mojom::LogEntryPtr>>
      future;
  handler_remote_->GetBufferedLogs(future.GetCallback());

  std::vector<multistep_filter_internals::mojom::LogEntryPtr> logs =
      future.Take();
  EXPECT_TRUE(logs.empty());
}

TEST_F(MultistepFilterInternalsPageHandlerTest, LogRouterShutdown) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &profile_, &log_router_);

  // Simulate the log router shutting down.
  handler_->OnLogRouterShutdown();

  base::test::TestFuture<
      std::vector<multistep_filter_internals::mojom::LogEntryPtr>>
      future;
  handler_remote_->GetBufferedLogs(future.GetCallback());

  std::vector<multistep_filter_internals::mojom::LogEntryPtr> logs =
      future.Take();
  EXPECT_TRUE(logs.empty());
}

TEST_F(MultistepFilterInternalsPageHandlerTest, GetBufferedLogs_Empty) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &profile_, &log_router_);

  base::test::TestFuture<
      std::vector<multistep_filter_internals::mojom::LogEntryPtr>>
      future;
  handler_remote_->GetBufferedLogs(future.GetCallback());

  std::vector<multistep_filter_internals::mojom::LogEntryPtr> logs =
      future.Take();
  EXPECT_TRUE(logs.empty());
}

TEST_F(MultistepFilterInternalsPageHandlerTest, GetBufferedLogs_Single) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &profile_, &log_router_);

  multistep_filter::LogEntry entry(
      kTestNavigation2, multistep_filter::LogEventType::kUrlEligibilityCheck,
      "example.com");
  entry.details.Set("string_key", "string_val");
  entry.details.Set("bool_key", true);

  log_router_.RouteLogMessage(std::move(entry));

  base::test::TestFuture<
      std::vector<multistep_filter_internals::mojom::LogEntryPtr>>
      future;
  handler_remote_->GetBufferedLogs(future.GetCallback());

  std::vector<multistep_filter_internals::mojom::LogEntryPtr> logs =
      future.Take();
  ASSERT_EQ(logs.size(), 1u);
  ASSERT_TRUE(logs[0]);

  EXPECT_EQ(logs[0]->navigation_id, kTestNavigation2);
  EXPECT_EQ(logs[0]->event_type, "Url Eligibility Check");
  EXPECT_EQ(logs[0]->details, "bool_key: true, string_key: string_val");
}

TEST_F(MultistepFilterInternalsPageHandlerTest, GetBufferedLogs_Multiple) {
  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &profile_, &log_router_);

  multistep_filter::LogEntry entry1(
      kTestNavigation3, multistep_filter::LogEventType::kNavigationStarted,
      "example1.com");
  log_router_.RouteLogMessage(std::move(entry1));

  multistep_filter::LogEntry entry2(
      kTestNavigation4, multistep_filter::LogEventType::kUrlEligibilityCheck,
      "example2.com");
  log_router_.RouteLogMessage(std::move(entry2));

  base::test::TestFuture<
      std::vector<multistep_filter_internals::mojom::LogEntryPtr>>
      future;
  handler_remote_->GetBufferedLogs(future.GetCallback());

  std::vector<multistep_filter_internals::mojom::LogEntryPtr> logs =
      future.Take();
  ASSERT_EQ(logs.size(), 2u);
  ASSERT_TRUE(logs[0]);
  ASSERT_TRUE(logs[1]);

  EXPECT_EQ(logs[0]->navigation_id, kTestNavigation3);
  EXPECT_EQ(logs[1]->navigation_id, kTestNavigation4);
}

TEST_F(MultistepFilterInternalsPageHandlerTest, GetDebugInfo) {
  multistep_filter::MultistepFilterServiceFactory::GetInstance()
      ->SetTestingFactory(
          &profile_,
          base::BindRepeating(
              [](signin::IdentityManager* identity_manager,
                 content::BrowserContext* context)
                  -> std::unique_ptr<KeyedService> {
                Profile* profile = Profile::FromBrowserContext(context);
                auto service = std::make_unique<
                    testing::NiceMock<MockMultistepFilterService>>(
                    std::make_unique<
                        multistep_filter::MockAnnotationIndexClient>(),
                    std::make_unique<MockFilterStore>(), profile->GetPrefs(),
                    identity_manager);

                multistep_filter::AccountState account_state;
                account_state.is_signed_in = true;
                account_state.can_use_model_execution_features = true;

                multistep_filter::ConsentState consent_state;
                consent_state.is_msbb_enabled = true;
                consent_state.is_history_sync_enabled = true;

                multistep_filter::SettingsState settings_state;
                settings_state.opt_in_state =
                    optimization_guide::prefs::FeatureOptInState::kEnabled;
                settings_state.policy_state =
                    multistep_filter::SuggestionsPolicyState::kEnabled;

                ON_CALL(*service, GetAccountState())
                    .WillByDefault(testing::Return(account_state));
                ON_CALL(*service, GetConsentState())
                    .WillByDefault(testing::Return(consent_state));
                ON_CALL(*service, GetSettingsState())
                    .WillByDefault(testing::Return(settings_state));

                return service;
              },
              identity_test_env_.identity_manager()));

  handler_ = std::make_unique<MultistepFilterInternalsPageHandler>(
      handler_remote_.BindNewPipeAndPassReceiver(), page_.BindAndGetRemote(),
      &profile_, &log_router_);

  base::test::TestFuture<multistep_filter_internals::mojom::DebugInfoPtr>
      future;
  handler_remote_->GetDebugInfo(future.GetCallback());

  multistep_filter_internals::mojom::DebugInfoPtr info = future.Take();
  ASSERT_TRUE(info);

  EXPECT_TRUE(info->account_status->is_signed_in);
  EXPECT_TRUE(info->account_status->can_use_model_execution_features);

  EXPECT_TRUE(info->consent_status->is_msbb_enabled);
  EXPECT_TRUE(info->consent_status->is_history_sync_enabled);

  EXPECT_EQ(info->settings_status->contextual_cueing_opt_in_state, "Enabled");
  EXPECT_EQ(info->settings_status->chrome_suggestions_policy_state,
            "Enabled (0)");

  EXPECT_TRUE(info->is_eligible);
}

}  // namespace
}  // namespace multistep_filter_internals
