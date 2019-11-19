// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_result.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/app/arc_playstore_search_request_state.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

namespace app_list {

class ArcPlayStoreSearchProviderTest : public AppListTestBase {
 public:
  ArcPlayStoreSearchProviderTest() = default;
  ~ArcPlayStoreSearchProviderTest() override = default;

  // AppListTestBase:
  void SetUp() override {
    AppListTestBase::SetUp();
    arc_test_.SetUp(profile());
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
  }

  void TearDown() override {
    controller_.reset();
    arc_test_.TearDown();
    AppListTestBase::TearDown();
  }

 protected:
  std::unique_ptr<ArcPlayStoreSearchProvider> CreateSearch(int max_results) {
    return std::make_unique<ArcPlayStoreSearchProvider>(
        max_results, profile_.get(), controller_.get());
  }

  scoped_refptr<const extensions::Extension> CreateExtension(
      const std::string& id) {
    return extensions::ExtensionBuilder("test").SetID(id).Build();
  }

  void AddExtension(const extensions::Extension* extension) {
    service()->AddExtension(extension);
  }

 private:
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreSearchProviderTest);
};

TEST_F(ArcPlayStoreSearchProviderTest, Basic) {
  constexpr size_t kMaxResults = 12;
  constexpr char kQuery[] = "Play App";

  std::unique_ptr<ArcPlayStoreSearchProvider> provider =
      CreateSearch(kMaxResults);
  EXPECT_TRUE(provider->results().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  AddExtension(CreateExtension(extension_misc::kGmailAppId).get());

  // Check that the result size of a query doesn't exceed the |kMaxResults|.
  provider->Start(base::UTF8ToUTF16(kQuery));
  const SearchProvider::Results& results = provider->results();
  ASSERT_GT(results.size(), 0u);
  // Play Store returns |kMaxResults| results, but the first one (GMail) already
  // has Chrome extension installed, so it will be skipped.
  ASSERT_EQ(kMaxResults - 1, results.size());

  // Check that information is correctly set in each result.
  for (size_t i = 0; i < results.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing result %zu", i));
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->title()),
              base::StringPrintf("%s %zu", kQuery, i));
    EXPECT_EQ(results[i]->display_type(), ash::SearchResultDisplayType::kTile);
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->formatted_price()),
              base::StringPrintf("$%zu.22", i));
    EXPECT_EQ(results[i]->rating(), i);
    const bool is_instant_app = i % 2 == 0;
    EXPECT_EQ(results[i]->result_type(),
              is_instant_app ? ash::AppListSearchResultType::kInstantApp
                             : ash::AppListSearchResultType::kPlayStoreApp);
  }
}

TEST_F(ArcPlayStoreSearchProviderTest, FailedQuery) {
  constexpr size_t kMaxResults = 12;
  constexpr char kQuery[] = "Play App";
  const base::string16 kQueryString16 = base::UTF8ToUTF16(kQuery);

  std::unique_ptr<ArcPlayStoreSearchProvider> provider =
      CreateSearch(kMaxResults);
  EXPECT_TRUE(provider->results().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  // Test for empty queries.
  // Create a non-empty query.
  provider->Start(kQueryString16);
  EXPECT_GT(provider->results().size(), 0u);

  // Create an empty query and it should clear the result list.
  provider->Start(base::string16());
  EXPECT_EQ(0u, provider->results().size());

  // Test for queries with a failure state code.
  constexpr char kFailedQueryPrefix[] = "FailedQueryWithCode-";
  using RequestState = arc::ArcPlayStoreSearchRequestState;
  const std::array<RequestState, 15> kErrorStates = {
      RequestState::PLAY_STORE_PROXY_NOT_AVAILABLE,
      RequestState::FAILED_TO_CALL_CANCEL,
      RequestState::FAILED_TO_CALL_FINDAPPS,
      RequestState::REQUEST_HAS_INVALID_PARAMS,
      RequestState::REQUEST_TIMEOUT,
      RequestState::PHONESKY_RESULT_REQUEST_CODE_UNMATCHED,
      RequestState::PHONESKY_RESULT_SESSION_ID_UNMATCHED,
      RequestState::PHONESKY_REQUEST_REQUEST_CODE_UNMATCHED,
      RequestState::PHONESKY_APP_DISCOVERY_NOT_AVAILABLE,
      RequestState::PHONESKY_VERSION_NOT_SUPPORTED,
      RequestState::PHONESKY_UNEXPECTED_EXCEPTION,
      RequestState::PHONESKY_MALFORMED_QUERY,
      RequestState::PHONESKY_INTERNAL_ERROR,
      RequestState::PHONESKY_RESULT_INVALID_DATA,
      RequestState::CHROME_GOT_INVALID_RESULT,
  };
  static_assert(
      kErrorStates.size() == static_cast<size_t>(RequestState::STATE_COUNT) - 3,
      "Missing entries");
  for (const auto& error_state : kErrorStates) {
    // Create a non-empty query.
    provider->Start(kQueryString16);
    EXPECT_GT(provider->results().size(), 0u);

    // Fabricate a failing query and it should clear the result list.
    provider->Start(base::UTF8ToUTF16(
        base::StringPrintf("%s%d", kFailedQueryPrefix, error_state)));
    EXPECT_EQ(0u, provider->results().size());
  }
}
}  // namespace app_list
