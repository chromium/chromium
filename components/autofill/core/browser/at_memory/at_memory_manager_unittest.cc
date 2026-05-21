// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/accessibility_annotator/core/mock_accessibility_query_service.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::SaveArg;

class AtMemoryManagerTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
                                                 TestAutofillDriver,
                                                 TestBrowserAutofillManager> {
 public:
  void SetUp() override {
    InitAutofillClient();
    auto mock_query_service = std::make_unique<testing::NiceMock<
        accessibility_annotator::MockAccessibilityQueryService>>();
    mock_query_service_ptr_ = mock_query_service.get();
    autofill_client().set_accessibility_query_service(
        std::move(mock_query_service));
    CreateAutofillDriver();
  }

  void TearDown() override {
    mock_query_service_ptr_ = nullptr;
    DestroyAutofillClient();
  }

 protected:
  AtMemoryManager& manager() { return autofill_manager().GetAtMemoryManager(); }

  accessibility_annotator::MockAccessibilityQueryService& mock_query_service() {
    return *mock_query_service_ptr_;
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<accessibility_annotator::MockAccessibilityQueryService>
      mock_query_service_ptr_ = nullptr;
};

// Tests that attempting to start a subsequent incremental (type-ahead) search
// does not cancel an ongoing full search query, and the full search can still
// successfully complete.
TEST_F(AtMemoryManagerTest, IncrementalSearchBlockedByOngoingFullSearch) {
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         update_callback.Get());

  // Trigger a full search.
  base::RepeatingCallback<void(accessibility_annotator::MemorySearchResults)>
      full_search_callback;
  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"query"), /*full_search=*/true, _))
      .WillOnce(SaveArg<2>(&full_search_callback));

  manager().OnSearchSubmitted(u"query");

  // Attempt to start a subsequent incremental search while the full search is
  // running - it should be blocked.
  EXPECT_CALL(mock_query_service(), Query(std::u16string_view(u"query2"), _, _))
      .Times(0);
  manager().OnFilterChanged(u"query2");

  // Simulate results arriving for the full search and verify it completes.
  std::vector<accessibility_annotator::MemorySearchResult> full_entries;
  full_entries.emplace_back(accessibility_annotator::EntryType::kAddressFull,
                            u"Address", u"Full Address");
  accessibility_annotator::MemorySearchResults full_results(
      accessibility_annotator::MemorySearchStatus::kFinalResponseSuccess,
      std::move(full_entries));

  EXPECT_CALL(update_callback,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory));

  full_search_callback.Run(std::move(full_results));
}

}  // namespace

}  // namespace autofill
