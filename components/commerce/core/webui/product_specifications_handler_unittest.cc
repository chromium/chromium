// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/product_specifications_handler.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/commerce/core/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {

using ::testing::_;

const std::string kTestUrl1 = "http://www.example.com/1";
const std::string kTestUrl2 = "http://www.example.com/2";
const std::string kTestHistoryResultTitle = "Product title";

class MockPage : public product_specifications::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<product_specifications::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<product_specifications::mojom::Page> receiver_{this};

  MOCK_METHOD(void,
              OnProductSpecificationsSetAdded,
              (shared::mojom::ProductSpecificationsSetPtr set),
              (override));
  MOCK_METHOD(void,
              OnProductSpecificationsSetUpdated,
              (shared::mojom::ProductSpecificationsSetPtr set),
              (override));
  MOCK_METHOD(void,
              OnProductSpecificationsSetRemoved,
              (const base::Uuid& uuid),
              (override));
};

class MockDelegate : public ProductSpecificationsHandler::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              ShowDisclosureDialog,
              (const std::vector<GURL>& urls,
               const std::string& name,
               const std::string& set_id),
              (override));
  MOCK_METHOD(void,
              ShowProductSpecificationsSetForUuid,
              (const base::Uuid& uuid, bool in_new_tab),
              (override));
  MOCK_METHOD(void, ShowSyncSetupFlow, (), (override));
};

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;
  ~MockHistoryService() override = default;

  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              QueryURL,
              (const GURL& url,
               bool want_visits,
               QueryURLCallback callback,
               base::CancelableTaskTracker* tracker));
};

}  // namespace

class ProductSpecificationsHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    history_service_ = std::make_unique<MockHistoryService>();

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterCommercePrefs(pref_service_->registry());
    SetTabCompareEnterprisePolicyPref(pref_service_.get(), 0);
    SetShoppingListEnterprisePolicyPref(pref_service_.get(), true);

    product_specs_service_ =
        std::make_unique<MockProductSpecificationsService>();

    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    handler_ = std::make_unique<commerce::ProductSpecificationsHandler>(
        page_.BindAndGetRemote(),
        mojo::PendingReceiver<
            product_specifications::mojom::ProductSpecificationsHandler>(),
        std::move(delegate), history_service_.get(), pref_service_.get(),
        product_specs_service_.get());
  }

  std::unique_ptr<MockHistoryService> history_service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<MockProductSpecificationsService> product_specs_service_;

  MockPage page_;
  std::unique_ptr<commerce::ProductSpecificationsHandler> handler_;
  raw_ptr<MockDelegate> delegate_;

  base::test::ScopedFeatureList features_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ProductSpecificationsHandlerTest,
       TestMaybeShowProductSpecificationDisclosure_NotShow) {
  EXPECT_CALL(*delegate_, ShowDisclosureDialog).Times(0);

  pref_service_->SetInteger(
      kProductSpecificationsAcceptedDisclosureVersion,
      static_cast<int>(product_specifications::mojom::DisclosureVersion::kV1));

  base::RunLoop run_loop;
  handler_->MaybeShowDisclosure({}, "", "", base::BindOnce([](bool show) {
                                              ASSERT_FALSE(show);
                                            }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ProductSpecificationsHandlerTest,
       TestMaybeShowProductSpecificationDisclosure_Show) {
  std::vector<GURL> urls{GURL(kTestUrl1)};
  std::string name = "test_name";
  std::string set_id = "test_id";
  EXPECT_CALL(*delegate_, ShowDisclosureDialog(urls, name, set_id)).Times(1);

  pref_service_->SetInteger(
      kProductSpecificationsAcceptedDisclosureVersion,
      static_cast<int>(
          product_specifications::mojom::DisclosureVersion::kUnknown));

  base::RunLoop run_loop;
  handler_->MaybeShowDisclosure(urls, name, set_id,
                                base::BindOnce([](bool show) {
                                  ASSERT_TRUE(show);
                                }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ProductSpecificationsHandlerTest, TestSetDisclosureVersion) {
  handler_->SetAcceptedDisclosureVersion(
      product_specifications::mojom::DisclosureVersion::kV1);
  EXPECT_EQ(
      static_cast<int>(product_specifications::mojom::DisclosureVersion::kV1),
      pref_service_->GetInteger(
          kProductSpecificationsAcceptedDisclosureVersion));
}

TEST_F(ProductSpecificationsHandlerTest,
       TestSetDisclosureVersion_DefaultValue) {
  EXPECT_EQ(static_cast<int>(
                product_specifications::mojom::DisclosureVersion::kUnknown),
            pref_service_->GetInteger(
                kProductSpecificationsAcceptedDisclosureVersion));
}

TEST_F(ProductSpecificationsHandlerTest,
       TestDeclineProductSpecificationDisclosure) {
  ASSERT_EQ(0,
            pref_service_->GetInteger(
                commerce::kProductSpecificationsEntryPointShowIntervalInDays));
  base::Time last_dismiss_time = pref_service_->GetTime(
      commerce::kProductSpecificationsEntryPointLastDismissedTime);

  handler_->DeclineDisclosure();

  ASSERT_EQ(1,
            pref_service_->GetInteger(
                commerce::kProductSpecificationsEntryPointShowIntervalInDays));
  ASSERT_GT(pref_service_->GetTime(
                commerce::kProductSpecificationsEntryPointLastDismissedTime),
            last_dismiss_time);

  last_dismiss_time = pref_service_->GetTime(
      commerce::kProductSpecificationsEntryPointLastDismissedTime);
  handler_->DeclineDisclosure();

  ASSERT_EQ(2,
            pref_service_->GetInteger(
                commerce::kProductSpecificationsEntryPointShowIntervalInDays));
  ASSERT_GT(pref_service_->GetTime(
                commerce::kProductSpecificationsEntryPointLastDismissedTime),
            last_dismiss_time);
}

TEST_F(ProductSpecificationsHandlerTest, TestShowSyncSetupFlow) {
  EXPECT_CALL(*delegate_, ShowSyncSetupFlow).Times(1);
  handler_->ShowSyncSetupFlow();
}

TEST_F(ProductSpecificationsHandlerTest,
       TestShowProductSpecificationsSetForUuid) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*delegate_, ShowProductSpecificationsSetForUuid(uuid, true))
      .Times(1);
  handler_->ShowProductSpecificationsSetForUuid(uuid, true);
}

TEST_F(ProductSpecificationsHandlerTest, TestGetPageTitleFromHistory_Found) {
  EXPECT_CALL(*history_service_, QueryURL)
      .WillOnce([](const GURL& url, bool want_visits,
                   history::HistoryService::QueryURLCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryURLResult result;
        result.success = true;
        result.row.set_url(url);
        result.row.set_last_visit(base::Time::Now());
        result.row.set_title(base::UTF8ToUTF16(kTestHistoryResultTitle));
        std::move(callback).Run(result);
        return base::CancelableTaskTracker::TaskId();
      });

  base::RunLoop run_loop;
  handler_->GetPageTitleFromHistory(
      GURL(kTestUrl1), base::BindOnce([](const std::string& title) {
                         ASSERT_EQ(kTestHistoryResultTitle, title);
                       }).Then(run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(ProductSpecificationsHandlerTest, TestGetPageTitleFromHistory_NotFound) {
  EXPECT_CALL(*history_service_, QueryURL)
      .WillOnce([](const GURL& url, bool want_visits,
                   history::HistoryService::QueryURLCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryURLResult result;
        result.success = false;
        std::move(callback).Run(result);
        return base::CancelableTaskTracker::TaskId();
      });

  base::RunLoop run_loop;
  handler_->GetPageTitleFromHistory(
      GURL(kTestUrl1), base::BindOnce([](const std::string& title) {
                         ASSERT_EQ("", title);
                       }).Then(run_loop.QuitClosure()));

  run_loop.Run();
}

}  // namespace commerce
