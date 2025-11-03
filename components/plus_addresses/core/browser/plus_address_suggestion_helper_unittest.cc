// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/core/browser/plus_address_suggestion_helper.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/plus_addresses/core/browser/fake_plus_address_allocator.h"
#include "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#include "components/plus_addresses/core/browser/plus_address_allocator.h"
#include "components/plus_addresses/core/browser/plus_address_test_utils.h"
#include "components/plus_addresses/core/browser/plus_address_types.h"
#include "components/plus_addresses/core/browser/settings/fake_plus_address_setting_service.h"
#include "components/plus_addresses/core/common/features.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace plus_addresses {
namespace {

using autofill::Suggestion;
using autofill::SuggestionType;
using ::testing::ElementsAre;

class PlusAddressSuggestionHelperTest : public ::testing::Test {
 public:
  PlusAddressSuggestionHelperTest() = default;

 protected:
  FakePlusAddressAllocator& allocator() { return allocator_; }
  FakePlusAddressSettingService& setting_service() { return setting_service_; }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_env_;

  FakePlusAddressAllocator allocator_;
  FakePlusAddressSettingService setting_service_;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(PlusAddressSuggestionHelperTest, SetSuggestedPlusAddressForSuggestion) {
  const PlusAddress plus_address("plus@foo.com");
  Suggestion suggestion(SuggestionType::kCreateNewPlusAddressInline);
  suggestion.payload = Suggestion::PlusAddressPayload();
  suggestion.is_loading = Suggestion::IsLoading(true);
  PlusAddressSuggestionHelper::SetSuggestedPlusAddressForSuggestion(
      plus_address, suggestion);

  EXPECT_FALSE(suggestion.is_loading);
  EXPECT_EQ(suggestion.GetPayload<Suggestion::PlusAddressPayload>().address,
            base::UTF8ToUTF16(*plus_address));
}

TEST_F(PlusAddressSuggestionHelperTest, GetPlusAddressErrorSuggestion) {
  const Suggestion suggestion(
      PlusAddressSuggestionHelper::GetPlusAddressErrorSuggestion(
          PlusAddressRequestError::AsNetworkError(net::HTTP_BAD_REQUEST)));
  EXPECT_EQ(suggestion.type, SuggestionType::kPlusAddressError);
  EXPECT_EQ(
      suggestion.main_text.value,
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT));
  EXPECT_EQ(suggestion.icon, Suggestion::Icon::kError);
  EXPECT_TRUE(
      suggestion.GetPayload<Suggestion::PlusAddressPayload>().offer_refresh);
  EXPECT_THAT(
      suggestion.labels,
      ElementsAre(ElementsAre(Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_RESERVE_GENERIC_ERROR_TEXT)))));
}

TEST_F(PlusAddressSuggestionHelperTest,
       GetPlusAddressErrorSuggestionForQuotaError) {
  const auto error =
      PlusAddressRequestError::AsNetworkError(net::HTTP_TOO_MANY_REQUESTS);
  ASSERT_TRUE(error.IsQuotaError());

  const Suggestion suggestion(
      PlusAddressSuggestionHelper::GetPlusAddressErrorSuggestion(error));
  EXPECT_EQ(suggestion.type, SuggestionType::kPlusAddressError);
  EXPECT_EQ(
      suggestion.main_text.value,
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT));
  EXPECT_EQ(suggestion.icon, Suggestion::Icon::kError);
  EXPECT_FALSE(
      suggestion.GetPayload<Suggestion::PlusAddressPayload>().offer_refresh);
  EXPECT_THAT(
      suggestion.labels,
      ElementsAre(ElementsAre(Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_RESERVE_QUOTA_ERROR_TEXT)))));
}

// Tests that suggestions in the `is_loading` state do not have a refresh
// button and is not acceptable.
TEST_F(PlusAddressSuggestionHelperTest, LoadingStateProperties) {
  Suggestion inline_suggestion(SuggestionType::kCreateNewPlusAddressInline);
  inline_suggestion.payload = Suggestion::PlusAddressPayload();

  PlusAddressSuggestionHelper::SetLoadingStateForSuggestion(
      /*is_loading=*/true, inline_suggestion);
  EXPECT_TRUE(inline_suggestion.is_loading);
  EXPECT_FALSE(inline_suggestion.IsAcceptable());
  EXPECT_FALSE(inline_suggestion.GetPayload<Suggestion::PlusAddressPayload>()
                   .offer_refresh);

  PlusAddressSuggestionHelper::SetSuggestedPlusAddressForSuggestion(
      PlusAddress("foo@moo.com"), inline_suggestion);
  EXPECT_FALSE(inline_suggestion.is_loading);
  EXPECT_TRUE(inline_suggestion.GetPayload<Suggestion::PlusAddressPayload>()
                  .offer_refresh);
  EXPECT_TRUE(inline_suggestion.IsAcceptable());
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Tests that filling is offered on fields where autofill was previously
// triggered and prefix-matching is not applied.
TEST_F(PlusAddressSuggestionHelperTest,
       FillingSuggestionOnPreviouslyAutofilledFields) {
  PlusAddressSuggestionHelper generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")));
  const std::string plus_address = "test+plus@test.com";

  autofill::FormFieldData focused_field;
  EXPECT_THAT(generator.GetSuggestions(
                  /*affiliated_plus_addresses=*/{plus_address}, focused_field,
                  /*is_plus_address_manually_triggered=*/false),
              test::IsSingleFillPlusAddressSuggestion(plus_address));

  // Field got autofilled. The values does not prefix-match any plus address.
  focused_field.set_is_autofilled(true);
  focused_field.set_value(u"pp");
  EXPECT_THAT(generator.GetSuggestions(
                  /*affiliated_plus_addresses=*/{plus_address}, focused_field,
                  /*is_plus_address_manually_triggered=*/false),
              test::IsSingleFillPlusAddressSuggestion(plus_address));
}
}  // namespace
}  // namespace plus_addresses
