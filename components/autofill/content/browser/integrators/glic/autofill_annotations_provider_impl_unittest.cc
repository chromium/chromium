// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/glic/autofill_annotations_provider_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using autofill::AddressCountryCode;
using autofill::AutofillProfile;
using autofill::CreditCard;
using autofill::FormData;
using autofill::LocalFrameToken;
using autofill::TestAutofillClientInjector;
using autofill::TestAutofillDriverInjector;
using autofill::TestAutofillManagerInjector;
using autofill::TestBrowserAutofillManager;
using autofill::TestContentAutofillClient;
using autofill::TestContentAutofillDriver;
using autofill::test::AutofillUnitTestEnvironment;
using autofill::test::FormDescription;

class AutofillAnnotationsProviderImplTest
    : public content::RenderViewHostImplTestHarness {
 public:
  AutofillAnnotationsProviderImplTest() = default;
  ~AutofillAnnotationsProviderImplTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
    NavigateAndCommit(GURL("about:blank"));
  }

  AutofillProfile CreateAddress() {
    return AutofillProfile(AutofillProfile::RecordType::kAccount,
                           AddressCountryCode("ES"));
  }

  CreditCard CreateCreditCard() {
    CreditCard local_card;
    autofill::test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                                      "4234567890123463",  // Visa
                                      "08", "2999", "1");
    local_card.set_guid("00000000-0000-0000-0000-000000000009");
    local_card.set_record_type(CreditCard::RecordType::kLocalCard);
    local_card.usage_history().set_use_count(5);
    return local_card;
  }

 protected:
  TestBrowserAutofillManager* autofill_manager() {
    return autofill_manager_injector_[contents()];
  }

  TestContentAutofillClient* client() {
    return autofill_client_injector_[web_contents()];
  }

  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<TestContentAutofillDriver>
      autofill_driver_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;

  AutofillAnnotationsProviderImpl autofill_annotations_provider_;
};

// Ensure that `section_id` and `coarse_field_type` are properly returned by
// GetAutofillFieldData.
TEST_F(AutofillAnnotationsProviderImplTest,
       GetAutofillFieldData_AddAnnotations) {
  // Register a form to the Autofill Manager.
  FormDescription form_description = {
      .fields = {
          {.server_type = autofill::NAME_FULL,
           .host_frame = LocalFrameToken(
               contents()->GetPrimaryMainFrame()->GetFrameToken().value()),
           .label = u"name",
           .name = u"name"},
      }};
  FormData form = autofill::test::GetFormData(form_description);
  autofill_manager()->AddSeenForm(
      form, autofill::test::GetHeuristicTypes(form_description),
      autofill::test::GetServerTypes(form_description));

  ConvertAIPageContentToProtoSession session;
  std::optional<AutofillFieldMetadata> metadata =
      autofill_annotations_provider_.GetAutofillFieldData(
          *contents()->GetPrimaryMainFrame(),
          form.fields()[0].renderer_id().GetUnsafeValue(), session);

  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->section_id, 0u);
  EXPECT_EQ(metadata->coarse_field_type,
            optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_ADDRESS);
}

TEST_F(AutofillAnnotationsProviderImplTest, GetAutofillFieldData_Redaction) {
  // Register a form to the Autofill Manager.
  FormDescription form_description = {
      .fields = {
          {.server_type = autofill::NAME_FULL,
           .host_frame = LocalFrameToken(
               contents()->GetPrimaryMainFrame()->GetFrameToken().value()),
           .label = u"name",
           .name = u"name",
           .value = u"Albert Smith"},
          {.server_type = autofill::PHONE_HOME_WHOLE_NUMBER,
           .host_frame = LocalFrameToken(
               contents()->GetPrimaryMainFrame()->GetFrameToken().value()),
           .label = u"phone",
           .name = u"phone",
           .value = u"1234567890"},
          {.server_type = autofill::ADDRESS_HOME_STREET_ADDRESS,
           .host_frame = LocalFrameToken(
               contents()->GetPrimaryMainFrame()->GetFrameToken().value()),
           .label = u"address",
           .name = u"address",
           .value = u"123 Main St"},
          {.server_type = autofill::CREDIT_CARD_NUMBER,
           .host_frame = LocalFrameToken(
               contents()->GetPrimaryMainFrame()->GetFrameToken().value()),
           .label = u"cc-number",
           .name = u"cc-number",
           .value = u"411111111111"},
          {.server_type = autofill::UNKNOWN_TYPE,
           .host_frame = LocalFrameToken(
               contents()->GetPrimaryMainFrame()->GetFrameToken().value()),
           .label = u"empty",
           .name = u"empty",
           .value = u""},
      }};
  FormData form = autofill::test::GetFormData(form_description);
  autofill_manager()->AddSeenForm(
      form, autofill::test::GetHeuristicTypes(form_description),
      autofill::test::GetServerTypes(form_description));

  ConvertAIPageContentToProtoSession session;
  std::optional<AutofillFieldMetadata> name_metadata =
      autofill_annotations_provider_.GetAutofillFieldData(
          *contents()->GetPrimaryMainFrame(),
          form.fields()[0].renderer_id().GetUnsafeValue(), session);
  ASSERT_TRUE(name_metadata);
  EXPECT_EQ(name_metadata->redaction_reason,
            AutofillFieldRedactionReason::kNoRedactionNeeded);

  std::optional<AutofillFieldMetadata> phone_metadata =
      autofill_annotations_provider_.GetAutofillFieldData(
          *contents()->GetPrimaryMainFrame(),
          form.fields()[1].renderer_id().GetUnsafeValue(), session);
  ASSERT_TRUE(phone_metadata);
  EXPECT_EQ(phone_metadata->redaction_reason,
            AutofillFieldRedactionReason::kNoRedactionNeeded);

  std::optional<AutofillFieldMetadata> address_metadata =
      autofill_annotations_provider_.GetAutofillFieldData(
          *contents()->GetPrimaryMainFrame(),
          form.fields()[2].renderer_id().GetUnsafeValue(), session);
  ASSERT_TRUE(address_metadata);
  EXPECT_EQ(address_metadata->redaction_reason,
            AutofillFieldRedactionReason::kNoRedactionNeeded);

  std::optional<AutofillFieldMetadata> cc_metadata =
      autofill_annotations_provider_.GetAutofillFieldData(
          *contents()->GetPrimaryMainFrame(),
          form.fields()[3].renderer_id().GetUnsafeValue(), session);
  ASSERT_TRUE(cc_metadata);
  EXPECT_EQ(cc_metadata->redaction_reason,
            AutofillFieldRedactionReason::kShouldRedactForPayments);

  std::optional<AutofillFieldMetadata> unknown_metadata =
      autofill_annotations_provider_.GetAutofillFieldData(
          *contents()->GetPrimaryMainFrame(),
          form.fields()[4].renderer_id().GetUnsafeValue(), session);
  ASSERT_TRUE(unknown_metadata);
  EXPECT_EQ(unknown_metadata->redaction_reason,
            AutofillFieldRedactionReason::kNoRedactionNeeded);
}

// Ensures that GetAutofillAvailability returns no availability when there are
// no addresses or credit cards.
TEST_F(AutofillAnnotationsProviderImplTest, GetAutofillAvailability_NoData) {
  AutofillAvailability availability =
      autofill_annotations_provider_.GetAutofillAvailability(
          *contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(availability.has_fillable_address);
  EXPECT_FALSE(availability.has_fillable_credit_card);
}

// Ensures that GetAutofillAvailability reports an autofillable address if it
// exists.
TEST_F(AutofillAnnotationsProviderImplTest,
       GetAutofillAvailability_AddressProfile) {
  client()->GetPersonalDataManager().address_data_manager().AddProfile(
      CreateAddress());

  AutofillAvailability availability =
      autofill_annotations_provider_.GetAutofillAvailability(
          *contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(availability.has_fillable_address);
  EXPECT_FALSE(availability.has_fillable_credit_card);
}

// Ensures that GetAutofillAvailability reports NO autofillable address if it
// exists but autofill is disabled.
TEST_F(AutofillAnnotationsProviderImplTest,
       GetAutofillAvailability_AddressProfile_AutofillDisabled) {
  client()->SetAutofillProfileEnabled(false);

  client()->GetPersonalDataManager().address_data_manager().AddProfile(
      CreateAddress());
  AutofillAvailability availability =
      autofill_annotations_provider_.GetAutofillAvailability(
          *contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(availability.has_fillable_address);
  EXPECT_FALSE(availability.has_fillable_credit_card);
}

// Ensures that GetAutofillAvailability reports an autofillable credit card if
// it exists.
TEST_F(AutofillAnnotationsProviderImplTest,
       GetAutofillAvailability_CreditCard) {
  client()->GetPersonalDataManager().payments_data_manager().AddCreditCard(
      CreateCreditCard());
  AutofillAvailability availability =
      autofill_annotations_provider_.GetAutofillAvailability(
          *contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(availability.has_fillable_address);
  EXPECT_TRUE(availability.has_fillable_credit_card);
}

// Ensures that GetAutofillAvailability reports NO autofillable credit card if
// it exists but autofill is disabled.
TEST_F(AutofillAnnotationsProviderImplTest,
       GetAutofillAvailability_CreditCard_AutofillDisabled) {
  client()->GetPaymentsAutofillClient()->SetAutofillPaymentMethodsEnabled(
      false);

  client()->GetPersonalDataManager().payments_data_manager().AddCreditCard(
      CreateCreditCard());
  AutofillAvailability availability =
      autofill_annotations_provider_.GetAutofillAvailability(
          *contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(availability.has_fillable_address);
  EXPECT_FALSE(availability.has_fillable_credit_card);
}

// Ensures that GetAutofillAvailability reports both an autofillable address and
// credit card if they exist.
TEST_F(AutofillAnnotationsProviderImplTest,
       GetAutofillAvailability_AddressAndCreditCard) {
  client()->GetPersonalDataManager().address_data_manager().AddProfile(
      CreateAddress());
  client()->GetPersonalDataManager().payments_data_manager().AddCreditCard(
      CreateCreditCard());

  AutofillAvailability availability =
      autofill_annotations_provider_.GetAutofillAvailability(
          *contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(availability.has_fillable_address);
  EXPECT_TRUE(availability.has_fillable_credit_card);
}

}  // namespace optimization_guide
