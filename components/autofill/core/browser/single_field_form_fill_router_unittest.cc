// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_form_fill_router.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/version_info/version_info.h"

namespace autofill {

namespace {
class MockSuggestionsHandler
    : public SingleFieldFormFiller::SuggestionsHandler {
 public:
  MockSuggestionsHandler() = default;
  MockSuggestionsHandler(const MockSuggestionsHandler&) = delete;
  MockSuggestionsHandler& operator=(const MockSuggestionsHandler&) = delete;
  ~MockSuggestionsHandler() override = default;

  MOCK_METHOD(void,
              OnSuggestionsReturned,
              (int query_id,
               bool autoselect_first_suggestion,
               const std::vector<Suggestion>& suggestions),
              (override));

  base::WeakPtr<MockSuggestionsHandler> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSuggestionsHandler> weak_ptr_factory_{this};
};
}  // namespace

class SingleFieldFormFillRouterTest : public testing::Test {
 protected:
  SingleFieldFormFillRouterTest() {
    prefs_ = test::PrefServiceForTesting();

    // Mock such that we don't trigger the cleanup.
    prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                       CHROME_VERSION_MAJOR);

    web_data_service_ = base::MakeRefCounted<MockAutofillWebDataService>();
    autocomplete_history_manager_ =
        std::make_unique<MockAutocompleteHistoryManager>();
    autocomplete_history_manager_->Init(web_data_service_, prefs_.get(), false);
    single_field_form_fill_router_ =
        std::make_unique<SingleFieldFormFillRouter>(
            autocomplete_history_manager_.get());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<SingleFieldFormFillRouter> single_field_form_fill_router_;
  scoped_refptr<MockAutofillWebDataService> web_data_service_;
  std::unique_ptr<MockAutocompleteHistoryManager> autocomplete_history_manager_;
  std::unique_ptr<PrefService> prefs_;
};

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAutocompleteHistoryManager_OnGetSingleFieldSuggestions) {
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  EXPECT_CALL(*autocomplete_history_manager_, OnGetSingleFieldSuggestions);

  single_field_form_fill_router_->OnGetSingleFieldSuggestions(
      /*query_id=*/2, /*is_autocomplete_enabled=*/true,
      /*autoselect_first_suggestion=*/false, /*name=*/u"Some Field Name",
      /*prefix=*/u"SomePrefix",
      /*form_control_type=*/"SomeType", suggestions_handler->GetWeakPtr());
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnWillSubmitForm call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAutocompleteHistoryManager_OnWillSubmitForm) {
  EXPECT_CALL(*autocomplete_history_manager_, OnWillSubmitForm);

  single_field_form_fill_router_->OnWillSubmitForm(
      FormData(), /*is_autocomplete_enabled=*/true);
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// CancelPendingQueries call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAutocompleteHistoryManager_CancelPendingQueries) {
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  EXPECT_CALL(*autocomplete_history_manager_, CancelPendingQueries);

  single_field_form_fill_router_->CancelPendingQueries(
      suggestions_handler.get());
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnRemoveCurrentSingleFieldSuggestion call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAutocompleteHistoryManager_OnRemoveCurrentSingleFieldSuggestion) {
  EXPECT_CALL(*autocomplete_history_manager_,
              OnRemoveCurrentSingleFieldSuggestion);

  single_field_form_fill_router_->OnRemoveCurrentSingleFieldSuggestion(
      /*field_name=*/u"Field Name", /*value=*/u"Value");
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnSingleFieldSuggestionSelected call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAutocompleteHistoryManager_OnSingleFieldSuggestionSelected) {
  EXPECT_CALL(*autocomplete_history_manager_, OnSingleFieldSuggestionSelected);

  single_field_form_fill_router_->OnSingleFieldSuggestionSelected(
      /*value=*/u"Value");
}

}  // namespace autofill
