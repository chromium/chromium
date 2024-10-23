// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_suggestion_generator.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/plus_addresses/fake_plus_address_allocator.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/fake_plus_address_setting_service.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace plus_addresses {
namespace {

using autofill::AutofillSuggestionTriggerSource;
using autofill::EqualsSuggestion;
using autofill::FieldGlobalId;
using autofill::FormData;
using autofill::PasswordFormClassification;
using autofill::Suggestion;
using autofill::SuggestionType;
using autofill::test::CreateTestSignupFormData;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::SizeIs;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
auto IsCreateInlineSuggestion(
    std::optional<std::u16string> suggested_plus_address) {
  const bool is_loading = !suggested_plus_address.has_value();
  const std::optional<std::u16string> voice_over =
      suggested_plus_address
          ? l10n_util::GetStringFUTF16(
                IDS_PLUS_ADDRESS_CREATE_INLINE_SUGGESTION_A11Y_VOICE_OVER,
                *suggested_plus_address)
          : std::optional<std::u16string>();
  Suggestion::PlusAddressPayload payload(std::move(suggested_plus_address));
  payload.offer_refresh = !is_loading;
  return AllOf(
      EqualsSuggestion(SuggestionType::kCreateNewPlusAddressInline),
      Property(&Suggestion::GetPayload<Suggestion::PlusAddressPayload>,
               std::move(payload)),
      Field(&Suggestion::is_loading, Suggestion::IsLoading(is_loading)),
      Field(&Suggestion::voice_over, voice_over));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Returns `form` with a non-null host form id and frame token.
FormData SetGeneratedFrameTokenAndHostFormId(FormData form) {
  // Ensure that the form is not unowned.
  form.set_renderer_id(autofill::test::MakeFormRendererId());
  std::vector<autofill::FormFieldData> fields = form.ExtractFields();
  for (autofill::FormFieldData& field : fields) {
    field.set_host_form_id(form.renderer_id());
  }
  form.set_fields(std::move(fields));

  // Set the same non-zero host frame for all fields.
  return autofill::test::CreateFormDataForFrame(
      std::move(form), autofill::test::MakeLocalFrameToken());
}

class PlusAddressSuggestionGeneratorTest : public ::testing::Test {
 public:
  PlusAddressSuggestionGeneratorTest() = default;

 protected:
  FakePlusAddressAllocator& allocator() { return allocator_; }
  FakePlusAddressSettingService& setting_service() { return setting_service_; }

 private:
  base::test::ScopedFeatureList features_{
      features::kPlusAddressUserOnboardingEnabled};
  autofill::test::AutofillUnitTestEnvironment autofill_env_;

  FakePlusAddressAllocator allocator_;
  FakePlusAddressSettingService setting_service_;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Tests that an empty PlusAddressPayload is set if there are no cached plus
// addresses.
TEST_F(PlusAddressSuggestionGeneratorTest,
       InlineGenerationWithoutPreallocatedAddresses) {
  base::test::ScopedFeatureList inline_creation_feature(
      features::kPlusAddressInlineCreation);

  allocator().set_is_next_allocation_synchronous(false);
  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")));
  FormData form = CreateTestSignupFormData();
  EXPECT_THAT(
      generator.GetSuggestions(
          /*affiliated_plus_addresses=*/{},
          /*is_creation_enabled=*/true, form, /*form_field_type_groups=*/{},
          PasswordFormClassification(), form.fields()[0].global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked),
      ElementsAre(IsCreateInlineSuggestion(
          /*suggested_plus_address=*/std::nullopt)));
}

// Tests that if there are cached plus addresses available, then one is set is
// the PlusAddressPayload.
TEST_F(PlusAddressSuggestionGeneratorTest,
       InlineGenerationWithPreallocatedAddresses) {
  base::test::ScopedFeatureList inline_creation_feature(
      features::kPlusAddressInlineCreation);

  allocator().set_is_next_allocation_synchronous(true);
  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")));
  FormData form = CreateTestSignupFormData();
  EXPECT_THAT(
      generator.GetSuggestions(
          /*affiliated_plus_addresses=*/{},
          /*is_creation_enabled=*/true, form, /*form_field_type_groups=*/{},
          PasswordFormClassification(), form.fields()[0].global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked),
      ElementsAre(IsCreateInlineSuggestion(
          /*suggested_plus_address=*/base::UTF8ToUTF16(
              *test::CreatePlusProfile().plus_address))));
}

TEST_F(PlusAddressSuggestionGeneratorTest,
       SetSuggestedPlusAddressForSuggestion) {
  const PlusAddress plus_address("plus@foo.com");
  Suggestion suggestion(SuggestionType::kCreateNewPlusAddressInline);
  suggestion.payload = Suggestion::PlusAddressPayload();
  suggestion.is_loading = Suggestion::IsLoading(true);
  PlusAddressSuggestionGenerator::SetSuggestedPlusAddressForSuggestion(
      plus_address, suggestion);

  EXPECT_FALSE(suggestion.is_loading);
  EXPECT_EQ(suggestion.GetPayload<Suggestion::PlusAddressPayload>().address,
            base::UTF8ToUTF16(*plus_address));
}

TEST_F(PlusAddressSuggestionGeneratorTest, GetPlusAddressErrorSuggestion) {
  const Suggestion suggestion(
      PlusAddressSuggestionGenerator::GetPlusAddressErrorSuggestion(
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

TEST_F(PlusAddressSuggestionGeneratorTest,
       GetPlusAddressErrorSuggestionForQuotaError) {
  const auto error =
      PlusAddressRequestError::AsNetworkError(net::HTTP_TOO_MANY_REQUESTS);
  ASSERT_TRUE(error.IsQuotaError());

  const Suggestion suggestion(
      PlusAddressSuggestionGenerator::GetPlusAddressErrorSuggestion(error));
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
TEST_F(PlusAddressSuggestionGeneratorTest, LoadingStateProperties) {
  Suggestion inline_suggestion(SuggestionType::kCreateNewPlusAddressInline);
  inline_suggestion.payload = Suggestion::PlusAddressPayload();

  PlusAddressSuggestionGenerator::SetLoadingStateForSuggestion(
      /*is_loading=*/true, inline_suggestion);
  EXPECT_TRUE(inline_suggestion.is_loading);
  EXPECT_FALSE(inline_suggestion.IsAcceptable());
  EXPECT_FALSE(inline_suggestion.GetPayload<Suggestion::PlusAddressPayload>()
                   .offer_refresh);

  PlusAddressSuggestionGenerator::SetSuggestedPlusAddressForSuggestion(
      PlusAddress("foo@moo.com"), inline_suggestion);
  EXPECT_FALSE(inline_suggestion.is_loading);
  EXPECT_TRUE(inline_suggestion.GetPayload<Suggestion::PlusAddressPayload>()
                  .offer_refresh);
  EXPECT_TRUE(inline_suggestion.IsAcceptable());
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Tests that the creation suggestion contains no labels if the notice has not
// been accepted.
TEST_F(PlusAddressSuggestionGeneratorTest, FirstTimeCreateSuggestion) {
  setting_service().set_has_accepted_notice(false);

  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")));
  FormData form = CreateTestSignupFormData();
  EXPECT_THAT(
      generator.GetSuggestions(
          /*affiliated_plus_addresses=*/{},
          /*is_creation_enabled=*/true, form, /*form_field_type_groups=*/{},
          PasswordFormClassification(), form.fields()[0].global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked),
      ElementsAre(AllOf(EqualsSuggestion(SuggestionType::kCreateNewPlusAddress),
                        Field(&Suggestion::labels, IsEmpty()))));
}

// Tests that no creation suggestion is shown on a login form.
TEST_F(PlusAddressSuggestionGeneratorTest, NoSuggestionsOnLoginForm) {
  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")));
  const FormData login_form = SetGeneratedFrameTokenAndHostFormId(
      autofill::test::CreateTestPasswordFormData());
  ASSERT_THAT(login_form.fields(), SizeIs(2));
  const FieldGlobalId focused_field_id = login_form.fields()[0].global_id();
  const base::flat_map<FieldGlobalId, autofill::FieldTypeGroup>
      form_field_type_groups = {
          {focused_field_id, autofill::FieldTypeGroup::kUsernameField},
          {login_form.fields()[1].global_id(),
           autofill::FieldTypeGroup::kPasswordField}};
  PasswordFormClassification classification;
  classification.type = PasswordFormClassification::Type::kLoginForm;
  classification.username_field = focused_field_id;
  EXPECT_THAT(generator.GetSuggestions(
                  /*affiliated_plus_addresses=*/{},
                  /*is_creation_enabled=*/true, login_form,
                  form_field_type_groups, classification, focused_field_id,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked),
              IsEmpty());
}

// Tests that creation is offered on forms classified by PWM as login forms if
// they have name or address fields included.
TEST_F(PlusAddressSuggestionGeneratorTest,
       SuggestionsOnLoginFormWithNameFields) {
  base::test::ScopedFeatureList feature_list{
      features::kPlusAddressRefinedPasswordFormClassification};

  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")));
  FormData form = autofill::test::CreateTestPasswordFormData();
  {
    std::vector<autofill::FormFieldData> fields = form.ExtractFields();
    fields.push_back(autofill::test::CreateTestFormField(
        /*label=*/"First name", /*name=*/"first_name",
        /*value=*/"", autofill::FormControlType::kInputText));
    form.set_fields(std::move(fields));
  }
  form = SetGeneratedFrameTokenAndHostFormId(std::move(form));
  ASSERT_THAT(form.fields(), SizeIs(3));
  const FieldGlobalId focused_field_id = form.fields()[0].global_id();
  const base::flat_map<FieldGlobalId, autofill::FieldTypeGroup>
      form_field_type_groups = {
          {focused_field_id, autofill::FieldTypeGroup::kUsernameField},
          {form.fields()[1].global_id(),
           autofill::FieldTypeGroup::kPasswordField},
          {form.fields()[2].global_id(), autofill::FieldTypeGroup::kName}};
  PasswordFormClassification classification;
  classification.type = PasswordFormClassification::Type::kLoginForm;
  classification.username_field = focused_field_id;
  EXPECT_THAT(
      generator.GetSuggestions(
          /*affiliated_plus_addresses=*/{},
          /*is_creation_enabled=*/true, form, form_field_type_groups,
          classification, focused_field_id,
          AutofillSuggestionTriggerSource::kFormControlElementClicked),
      ElementsAre(EqualsSuggestion(SuggestionType::kCreateNewPlusAddress)));
}

// Tests that creation is offered on forms classified by PWM as login forms if
// the password field is hidden.
TEST_F(PlusAddressSuggestionGeneratorTest,
       SuggestionsOnLoginFormWithHiddenPasswordField) {
  base::test::ScopedFeatureList feature_list{
      features::kPlusAddressOfferCreationIfPasswordFieldIsNotVisible};

  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")));
  FormData form = autofill::test::CreateTestPasswordFormData();
  {
    std::vector<autofill::FormFieldData> fields = form.ExtractFields();
    fields[1].set_is_visible(false);
    form.set_fields(std::move(fields));
  }
  form = SetGeneratedFrameTokenAndHostFormId(std::move(form));
  const FieldGlobalId focused_field_id = form.fields()[0].global_id();
  PasswordFormClassification classification;
  classification.type = PasswordFormClassification::Type::kLoginForm;
  classification.username_field = focused_field_id;
  classification.password_field = form.fields()[1].global_id();
  EXPECT_THAT(
      generator.GetSuggestions(
          /*affiliated_plus_addresses=*/{},
          /*is_creation_enabled=*/true, form, /*form_field_type_groups=*/{},
          classification, focused_field_id,
          AutofillSuggestionTriggerSource::kFormControlElementClicked),
      ElementsAre(EqualsSuggestion(SuggestionType::kCreateNewPlusAddress)));
}

}  // namespace
}  // namespace plus_addresses
