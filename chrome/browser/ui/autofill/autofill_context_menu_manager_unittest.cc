// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;

namespace autofill {

namespace {

// Generates a ContextMenuParams for the Autofill context menu options.
content::ContextMenuParams CreateContextMenuParams(
    absl::optional<autofill::FormRendererId> form_renderer_id = absl::nullopt,
    autofill::FieldRendererId field_render_id = autofill::FieldRendererId(0)) {
  content::ContextMenuParams rv;
  rv.is_editable = true;
  rv.page_url = GURL("http://test.page/");
  rv.input_field_type = blink::mojom::ContextMenuDataInputFieldType::kPlainText;
  if (form_renderer_id)
    rv.form_renderer_id = form_renderer_id->value();
  rv.field_renderer_id = field_render_id.value();
  return rv;
}

class MockAutofillDriver : public ContentAutofillDriver {
 public:
  using ContentAutofillDriver::ContentAutofillDriver;

  // Mock methods to enable testability.
  MOCK_METHOD(void,
              RendererShouldFillFieldWithValue,
              (const FieldGlobalId& field_id, const std::u16string& value),
              (override));
  MOCK_METHOD(void,
              OnContextMenuShownInField,
              (const FormGlobalId& form_global_id,
               const FieldGlobalId& field_global_id),
              (override));
  MOCK_METHOD(void,
              RendererShouldTriggerSuggestions,
              (const FieldGlobalId& field_id,
               AutofillSuggestionTriggerSource trigger_source),
              (override));
};

}  // namespace

class AutofillContextMenuManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillContextMenuManagerTest() {
    feature_.InitWithFeatures(
        {features::kAutofillFeedback,
         features::kAutofillPredictionsForAutocompleteUnrecognized,
         features::kAutofillFallbackForAutocompleteUnrecognized},
        {});
  }

  AutofillContextMenuManagerTest(const AutofillContextMenuManagerTest&) =
      delete;
  AutofillContextMenuManagerTest& operator=(
      const AutofillContextMenuManagerTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        profile(), BrowserContextKeyedServiceFactory::TestingFactory());
    NavigateAndCommit(GURL("about:blank"));
    autofill_client()->GetPersonalDataManager()->SetPrefService(
        profile()->GetPrefs());
    autofill_client()->GetPersonalDataManager()->AddProfile(
        test::GetFullProfile());
    autofill_client()->GetPersonalDataManager()->AddCreditCard(
        test::GetCreditCard());

    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    render_view_context_menu_ = std::make_unique<TestRenderViewContextMenu>(
        *main_rfh(), content::ContextMenuParams());
    render_view_context_menu_->Init();
    autofill_context_menu_manager_ =
        std::make_unique<AutofillContextMenuManager>(
            autofill_client()->GetPersonalDataManager(),
            render_view_context_menu_.get(), menu_model_.get(), nullptr,
            std::make_unique<ScopedNewBadgeTracker>(profile()));
    autofill_context_menu_manager()->set_params_for_testing(
        CreateContextMenuParams());
  }

  void TearDown() override {
    autofill_context_menu_manager_.reset();
    render_view_context_menu_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  TestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

  MockAutofillDriver* driver() { return autofill_driver_injector_[main_rfh()]; }

  BrowserAutofillManager* autofill_manager() {
    return static_cast<BrowserAutofillManager*>(driver()->autofill_manager());
  }

  ui::SimpleMenuModel* menu_model() const { return menu_model_.get(); }

  AutofillContextMenuManager* autofill_context_menu_manager() const {
    return autofill_context_menu_manager_.get();
  }

  // Sets the `form` and the `form.fields`'s `host_frame`. Since this test
  // fixture has its own render frame host, which is used by the
  // `autofill_context_menu_manager()`, this is necessary to identify the forms
  // correctly by their global ids.
  void SetHostFramesOfFormAndFields(FormData& form) {
    LocalFrameToken frame_token =
        LocalFrameToken(main_rfh()->GetFrameToken().value());
    form.host_frame = frame_token;
    for (FormFieldData& field : form.fields) {
      field.host_frame = frame_token;
    }
  }

  // Adds the `form` to the `driver()`'s manager.
  void AddSeenForm(const FormData& form) {
    TestAutofillManagerWaiter waiter(*autofill_manager(),
                                     {AutofillManagerEvent::kFormsSeen});
    autofill_manager()->OnFormsSeen(/*updated_forms=*/{form},
                                    /*removed_forms=*/{});
    ASSERT_TRUE(waiter.Wait());
  }

  // Creates a form where every field has unrecognized autocomplete attribute
  // and registers it with the manager.
  FormData SeeAutocompleteUnrecognizedForm() {
    FormData form;
    test::CreateTestAddressFormData(&form);
    for (FormFieldData& field : form.fields) {
      field.parsed_autocomplete =
          AutocompleteParsingResult{.field_type = HtmlFieldType::kUnrecognized};
    }
    SetHostFramesOfFormAndFields(form);
    AddSeenForm(form);
    return form;
  }

 private:
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<MockAutofillDriver> autofill_driver_injector_;
  std::unique_ptr<TestRenderViewContextMenu> render_view_context_menu_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<AutofillContextMenuManager> autofill_context_menu_manager_;
  base::test::ScopedFeatureList feature_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Tests that the Autofill context menu is correctly set up.
TEST_F(AutofillContextMenuManagerTest, AutofillContextMenuContents) {
  autofill_context_menu_manager()->AppendItems();
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_AUTOFILL_FEEDBACK),
            menu_model()->GetLabelAt(0));
}

// Tests that the Autofill's ContentAutofillDriver is called to record metrics
// when the context menu is triggered on a field.
TEST_F(AutofillContextMenuManagerTest, RecordContextMenuIsShownOnField) {
  FormRendererId form_renderer_id(test::MakeFormRendererId());
  FieldRendererId field_renderer_id(test::MakeFieldRendererId());
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form_renderer_id, field_renderer_id));

  FormGlobalId form_global_id{
      LocalFrameToken(main_rfh()->GetFrameToken().value()), form_renderer_id};
  FieldGlobalId field_global_id{
      LocalFrameToken(main_rfh()->GetFrameToken().value()), field_renderer_id};

  EXPECT_CALL(*driver(),
              OnContextMenuShownInField(form_global_id, field_global_id));
  autofill_context_menu_manager()->AppendItems();
}

// Tests that when triggering the context menu on an ac=unrecognized field, the
// fallback entry is part of the menu.
TEST_F(AutofillContextMenuManagerTest,
       AutocompleteUnrecognizedFallback_ContextMenuEntry) {
  // Simulate triggering the context menu on an ac=unrecognized field.
  FormData form = SeeAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.unique_renderer_id,
                              form.fields[0].unique_renderer_id));
  autofill_context_menu_manager()->AppendItems();

  // Expect to find the fallback entries at the end (after the manual fallback
  // and feedback entries).
  ASSERT_GE(menu_model()->GetItemCount(), 3u);
  const size_t fallback_index = menu_model()->GetItemCount() - 3;
  EXPECT_EQ(menu_model()->GetTypeAt(fallback_index),
            ui::MenuModel::ItemType::TYPE_TITLE);
  EXPECT_EQ(
      menu_model()->GetLabelAt(fallback_index),
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AUTOCOMPLETE_UNRECOGNIZED_TITLE));
  EXPECT_EQ(
      menu_model()->GetLabelAt(fallback_index + 1),
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AUTOCOMPLETE_UNRECOGNIZED));
  EXPECT_EQ(menu_model()->GetTypeAt(fallback_index + 2),
            ui::MenuModel::ItemType::TYPE_SEPARATOR);
}

// Tests that when the fallback entry for ac=unrecognized fields is selected,
// suggestions are triggered with suggestion trigger source
// `kManualFallbackForAutocompleteUnrecognized`.
TEST_F(AutofillContextMenuManagerTest,
       AutocompleteUnrecognizedFallback_TriggerSuggestions) {
  // Simulate triggering the context menu on an ac=unrecognized field.
  FormData form = SeeAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.unique_renderer_id,
                              form.fields[0].unique_renderer_id));
  autofill_context_menu_manager()->AppendItems();

  // Expect that when the entry is selected, suggestions are triggered from that
  // field.
  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form.fields[0].unique_renderer_id},
          AutofillSuggestionTriggerSource::
              kManualFallbackForAutocompleteUnrecognized));
  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AUTOCOMPLETE_UNRECOGNIZED);
}

TEST_F(AutofillContextMenuManagerTest,
       AutocompleteUnrecognizedFallback_ExplicitlyTriggeredMetric_NotAccepted) {
  // Simulate triggering the context menu on an ac=unrecognized field.
  FormData form = SeeAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.unique_renderer_id,
                              form.fields[0].unique_renderer_id));
  autofill_context_menu_manager()->AppendItems();

  // Expect that when the autofill_manager() is destroyed, the explicitly
  // triggered metric is emitted correctly.
  base::HistogramTester histogram_tester;
  autofill_manager()->Reset();
  histogram_tester.ExpectUniqueSample(
      "Autofill.ManualFallback.ExplicitlyTriggered."
      "ClassifiedFieldAutocompleteUnrecognized.Address",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ManualFallback.ExplicitlyTriggered.Total.Address", false, 1);
}

TEST_F(AutofillContextMenuManagerTest,
       AutocompleteUnrecognizedFallback_ExplicitlyTriggeredMetric_Accepted) {
  // Simulate triggering the context menu on an ac=unrecognized field.
  FormData form = SeeAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.unique_renderer_id,
                              form.fields[0].unique_renderer_id));
  autofill_context_menu_manager()->AppendItems();

  // Expect that when the autofill_manager() is destroyed, the explicitly
  // triggered metric is emitted correctly.
  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AUTOCOMPLETE_UNRECOGNIZED);
  base::HistogramTester histogram_tester;
  autofill_manager()->Reset();
  histogram_tester.ExpectUniqueSample(
      "Autofill.ManualFallback.ExplicitlyTriggered."
      "ClassifiedFieldAutocompleteUnrecognized.Address",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ManualFallback.ExplicitlyTriggered.Total.Address", true, 1);
}

}  // namespace autofill
