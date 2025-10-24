// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/glic/autofill_annotations_provider_impl.h"

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
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
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

namespace {

// Returns pointers to all `FormControlData` elements in `page_content`.
std::vector<const optimization_guide::proto::FormControlData*>
GetFormControlDatas(const optimization_guide::proto::AnnotatedPageContent&
                        page_content LIFETIME_BOUND) {
  std::vector<const optimization_guide::proto::FormControlData*>
      form_control_datas;

  optimization_guide::VisitContentNodes(
      page_content.root_node(),
      page_content.main_frame_data().document_identifier().serialized_token(),
      [&](const optimization_guide::proto::ContentNode& node,
          std::string_view document_identifier) {
        if (node.content_attributes().attribute_type() ==
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL) {
          form_control_datas.push_back(
              &node.content_attributes().form_control_data());
        }
      });

  return form_control_datas;
}

}  // namespace

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

// Ensures that `autofill_section_id` and `coarse_autofill_field_type` are
// properly populated in `AnnotatedPageContext` nodes for form controls.
TEST_F(AutofillAnnotationsProviderImplTest, AddAutofillAnnotations) {
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

  // Build an AnnotatedPageContent that contains the form.
  std::string token = *DocumentIdentifierUserData::GetDocumentIdentifier(
      contents()->GetPrimaryMainFrame()->GetGlobalFrameToken());

  proto::AnnotatedPageContent page_content;

  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(token);

  proto::ContentNode* node =
      page_content.mutable_root_node()->add_children_nodes();
  proto::ContentAttributes* node_attributes =
      node->mutable_content_attributes();
  node_attributes->set_attribute_type(proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  node_attributes->set_common_ancestor_dom_node_id(
      form.fields()[0].renderer_id().GetUnsafeValue());
  proto::FormControlData* form_control_data =
      node_attributes->mutable_form_control_data();
  form_control_data->set_field_name("name");

  ConvertAIPageContentToProtoSession session;
  autofill_annotations_provider_.AddAutofillAnnotations(
      *contents()->GetPrimaryMainFrame(), session, node_attributes);

  std::vector<const optimization_guide::proto::FormControlData*>
      form_control_datas = GetFormControlDatas(page_content);
  ASSERT_EQ(form_control_datas.size(), 1u);
  EXPECT_TRUE(form_control_datas[0]->has_autofill_section_id());
  EXPECT_EQ(form_control_datas[0]->autofill_section_id(), 0u);
  ASSERT_EQ(form_control_datas[0]->coarse_autofill_field_type_size(), 1);
  EXPECT_EQ(form_control_datas[0]->coarse_autofill_field_type(0),
            optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_ADDRESS);
}

// Ensures that the `autofill_information` in `AnnotatedPageContent` shows no
// records in case the PersonalDataManager does not show any data.
TEST_F(AutofillAnnotationsProviderImplTest, AddAutofillInformation_NoData) {
  proto::AnnotatedPageContent page_content;
  autofill_annotations_provider_.AddAutofillInformation(
      *contents()->GetPrimaryMainFrame(),
      page_content.mutable_profile_information()
          ->mutable_autofill_information());
  EXPECT_EQ(page_content.profile_information()
                .autofill_information()
                .fillable_data_size(),
            0);
}

// Ensures that the `autofill_information` in `AnnotatedPageContent` reports an
// autofillable address if it exists.
TEST_F(AutofillAnnotationsProviderImplTest,
       AddAutofillInformation_AddressProfile) {
  client()->GetPersonalDataManager().address_data_manager().AddProfile(
      CreateAddress());

  proto::AnnotatedPageContent page_content;
  autofill_annotations_provider_.AddAutofillInformation(
      *contents()->GetPrimaryMainFrame(),
      page_content.mutable_profile_information()
          ->mutable_autofill_information());
  EXPECT_EQ(page_content.profile_information()
                .autofill_information()
                .fillable_data_size(),
            1);
}

// Ensures that the `autofill_information` in `AnnotatedPageContent` reports NO
// autofillable address if it exists but autofill is disabled.
TEST_F(AutofillAnnotationsProviderImplTest,
       AddAutofillInformation_AddressProfile_AutofillDisabled) {
  client()->SetAutofillProfileEnabled(false);

  client()->GetPersonalDataManager().address_data_manager().AddProfile(
      CreateAddress());
  proto::AnnotatedPageContent page_content;
  autofill_annotations_provider_.AddAutofillInformation(
      *contents()->GetPrimaryMainFrame(),
      page_content.mutable_profile_information()
          ->mutable_autofill_information());
  EXPECT_EQ(page_content.profile_information()
                .autofill_information()
                .fillable_data_size(),
            0);
}

// Ensures that the `autofill_information` in `AnnotatedPageContent` reports an
// autofillable credit card if it exists.
TEST_F(AutofillAnnotationsProviderImplTest, AddAutofillInformation_CreditCard) {
  client()->GetPersonalDataManager().payments_data_manager().AddCreditCard(
      CreateCreditCard());
  proto::AnnotatedPageContent page_content;
  autofill_annotations_provider_.AddAutofillInformation(
      *contents()->GetPrimaryMainFrame(),
      page_content.mutable_profile_information()
          ->mutable_autofill_information());
  EXPECT_EQ(page_content.profile_information()
                .autofill_information()
                .fillable_data_size(),
            1);
}

// Ensures that the `autofill_information` in `AnnotatedPageContent` reports NO
// autofillable credit card if it exists but autofill is disabled.
TEST_F(AutofillAnnotationsProviderImplTest,
       AddAutofillInformation_CreditCard_AutofillDisabled) {
  client()->SetAutofillPaymentMethodsEnabled(false);

  client()->GetPersonalDataManager().payments_data_manager().AddCreditCard(
      CreateCreditCard());
  proto::AnnotatedPageContent page_content;
  autofill_annotations_provider_.AddAutofillInformation(
      *contents()->GetPrimaryMainFrame(),
      page_content.mutable_profile_information()
          ->mutable_autofill_information());
  EXPECT_EQ(page_content.profile_information()
                .autofill_information()
                .fillable_data_size(),
            0);
}

}  // namespace optimization_guide
