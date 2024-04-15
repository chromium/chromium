// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/address_save_metrics.h"
#include "components/autofill/core/browser/metrics/manual_fallback_metrics.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::Not;

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

// Checks if the context menu model contains any entries with manual fallback
// labels or command id. `arg` must be of type ui::SimpleMenuModel.
MATCHER(ContainsAnyAutofillFallbackEntries, "") {
  const auto kForbiddenLabels = base::MakeFlatSet<std::u16string>(
      std::array{IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_TITLE,
                 IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS,
                 IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS,
                 IDS_PLUS_ADDRESS_FALLBACK_LABEL_CONTEXT_MENU},
      /*comp=*/{},
      /*proj=*/[](auto id) { return l10n_util::GetStringUTF16(id); });
  const auto kForbiddenCommands =
      base::flat_set<int>{IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS,
                          IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS,
                          IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS};

  for (size_t i = 0; i < arg->GetItemCount(); i++) {
    if (base::Contains(kForbiddenCommands, arg->GetCommandIdAt(i)) ||
        base::Contains(kForbiddenLabels, arg->GetLabelAt(i))) {
      return true;
    }
  }
  return false;
}

// Checks if the context menu model contains the address manual fallback
// entries with correct UI strings. `arg` must be of type ui::SimpleMenuModel.
MATCHER(OnlyAddressFallbackAdded, "") {
  EXPECT_EQ(arg->GetItemCount(), 3u);
  return arg->GetTypeAt(0) == ui::MenuModel::ItemType::TYPE_TITLE &&
         arg->GetLabelAt(0) ==
             l10n_util::GetStringUTF16(
                 IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_TITLE) &&
         arg->GetLabelAt(1) ==
             l10n_util::GetStringUTF16(
                 IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS) &&
         arg->GetTypeAt(2) == ui::MenuModel::ItemType::TYPE_SEPARATOR;
}

// Checks if the context menu model contains the plus address manual fallback
// entries with correct UI strings. `arg` must be of type ui::SimpleMenuModel.
MATCHER(OnlyPlusAddressFallbackAdded, "") {
  EXPECT_EQ(arg->GetItemCount(), 3u);
  return arg->GetTypeAt(0) == ui::MenuModel::ItemType::TYPE_TITLE &&
         arg->GetLabelAt(0) ==
             l10n_util::GetStringUTF16(
                 IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_TITLE) &&
         arg->GetLabelAt(1) ==
             l10n_util::GetStringUTF16(
                 IDS_PLUS_ADDRESS_FALLBACK_LABEL_CONTEXT_MENU) &&
         arg->GetTypeAt(2) == ui::MenuModel::ItemType::TYPE_SEPARATOR;
}

// Checks if the context menu model contains the address and payments manual
// fallback entries with correct UI strings. `arg` must be of type
// ui::SimpleMenuModel.
MATCHER(AddressAndPaymentsFallbacksAdded, "") {
  EXPECT_EQ(arg->GetItemCount(), 4u);
  return arg->GetTypeAt(0) == ui::MenuModel::ItemType::TYPE_TITLE &&
         arg->GetLabelAt(0) ==
             l10n_util::GetStringUTF16(
                 IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_TITLE) &&
         arg->GetLabelAt(1) ==
             l10n_util::GetStringUTF16(
                 IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS) &&
         arg->GetLabelAt(2) ==
             l10n_util::GetStringUTF16(
                 IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS) &&
         arg->GetTypeAt(3) == ui::MenuModel::ItemType::TYPE_SEPARATOR;
}

// Generates a ContextMenuParams for the Autofill context menu options.
content::ContextMenuParams CreateContextMenuParams(
    std::optional<autofill::FormRendererId> form_renderer_id = std::nullopt,
    autofill::FieldRendererId field_render_id = autofill::FieldRendererId(0)) {
  content::ContextMenuParams rv;
  rv.is_editable = true;
  rv.page_url = GURL("http://test.page/");
  rv.form_control_type = blink::mojom::FormControlType::kInputText;
  if (form_renderer_id) {
    rv.form_renderer_id = form_renderer_id->value();
  }
  rv.field_renderer_id = field_render_id.value();
  return rv;
}

class MockAutofillDriver : public ContentAutofillDriver {
 public:
  using ContentAutofillDriver::ContentAutofillDriver;

  MOCK_METHOD(void,
              RendererShouldTriggerSuggestions,
              (const FieldGlobalId& field_id,
               AutofillSuggestionTriggerSource trigger_source),
              (override));
};

}  // namespace

// TODO(crbug.com/40286010): Simplify test setup.
class BaseAutofillContextMenuManagerTest : public InProcessBrowserTest {
 public:
  BaseAutofillContextMenuManagerTest() = default;

  BaseAutofillContextMenuManagerTest(
      const BaseAutofillContextMenuManagerTest&) = delete;
  BaseAutofillContextMenuManagerTest& operator=(
      const BaseAutofillContextMenuManagerTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    personal_data_ = PersonalDataManagerFactory::GetForProfile(profile());

    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    render_view_context_menu_ = std::make_unique<TestRenderViewContextMenu>(
        *main_rfh(), content::ContextMenuParams());
    render_view_context_menu_->Init();
    autofill_context_menu_manager_ =
        std::make_unique<AutofillContextMenuManager>(
            personal_data_, render_view_context_menu_.get(), menu_model_.get());
    autofill_context_menu_manager()->set_params_for_testing(
        CreateContextMenuParams());
  }

  void AddAutofillProfile(const autofill::AutofillProfile& profile) {
    size_t profile_count = personal_data_->GetProfiles().size();
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->AddProfile(profile);
    std::move(waiter).Wait();
    EXPECT_EQ(profile_count + 1, personal_data_->GetProfiles().size());
  }

  void AddCreditCard(const autofill::CreditCard& card) {
    if (card.record_type() != autofill::CreditCard::RecordType::kLocalCard) {
      personal_data_->AddServerCreditCardForTest(
          std::make_unique<autofill::CreditCard>(card));
      return;
    }
    size_t card_count = personal_data_->GetCreditCards().size();
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->AddCreditCard(card);
    std::move(waiter).Wait();
    EXPECT_EQ(card_count + 1, personal_data_->GetCreditCards().size());
  }

  content::RenderFrameHost* main_rfh() {
    return web_contents()->GetPrimaryMainFrame();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  Profile* profile() { return browser()->profile(); }

  void TearDownOnMainThread() override {
    autofill_context_menu_manager_.reset();
    render_view_context_menu_.reset();
    personal_data_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  TestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

  MockAutofillDriver* driver() { return autofill_driver_injector_[main_rfh()]; }

  BrowserAutofillManager& autofill_manager() {
    return static_cast<BrowserAutofillManager&>(driver()->GetAutofillManager());
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

  // Makes the form identifiable by its global id and adds the `form` to the
  // `driver()`'s manager.
  void AttachForm(FormData& form) {
    SetHostFramesOfFormAndFields(form);
    TestAutofillManagerWaiter waiter(autofill_manager(),
                                     {AutofillManagerEvent::kFormsSeen});
    autofill_manager().OnFormsSeen(/*updated_forms=*/{form},
                                   /*removed_forms=*/{});
    ASSERT_TRUE(waiter.Wait());
  }

  // Creates a form with classifiable fields and registers it with the manager.
  FormData CreateAndAttachClassifiedForm() {
    FormData form = test::CreateTestAddressFormData();
    AttachForm(form);
    return form;
  }

  // Creates a form where every field has unrecognized autocomplete attribute
  // and registers it with the manager.
  FormData CreateAndAttachAutocompleteUnrecognizedForm() {
    FormData form = test::CreateTestAddressFormData();
    for (FormFieldData& field : form.fields) {
      field.parsed_autocomplete =
          AutocompleteParsingResult{.field_type = HtmlFieldType::kUnrecognized};
    }
    AttachForm(form);
    return form;
  }

  // Creates a form with unclassifiable fields and registers it with the
  // manager.
  FormData CreateAndAttachUnclassifiedForm() {
    FormData form = test::CreateTestAddressFormData();
    for (FormFieldData& field : form.fields) {
      field.label = u"unclassifiable";
      field.set_name(u"unclassifiable");
    }
    AttachForm(form);
    return form;
  }

  PrefService& pref_service() { return *profile()->GetPrefs(); }

 protected:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  raw_ptr<PersonalDataManager> personal_data_ = nullptr;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<MockAutofillDriver> autofill_driver_injector_;
  std::unique_ptr<TestRenderViewContextMenu> render_view_context_menu_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<AutofillContextMenuManager> autofill_context_menu_manager_;
};

class AutocompleteUnrecognizedFieldsTest
    : public BaseAutofillContextMenuManagerTest {
 public:
  AutocompleteUnrecognizedFieldsTest() {
    feature_.InitAndDisableFeature(
        features::kAutofillForUnclassifiedFieldsAvailable);
  }

 private:
  base::test::ScopedFeatureList feature_;
};

// Tests that when triggering the context menu on an unclassified field, the
// fallback entry is not part of the menu.
IN_PROC_BROWSER_TEST_F(AutocompleteUnrecognizedFieldsTest,
                       UnclassifiedFormShown_FallbackOptionsNotPresent) {
  AddAutofillProfile(test::GetFullProfile());
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), Not(ContainsAnyAutofillFallbackEntries()));
}

// Tests that when triggering the context menu on an ac=unrecognized field, the
// fallback entry is not part of the menu if the user has no AutofillProfiles
// stored.
IN_PROC_BROWSER_TEST_F(
    AutocompleteUnrecognizedFieldsTest,
    AutocompleteUnrecognizedFormShown_NoAutofillProfiles_FallbackOptionsNotPresent) {
  FormData form = CreateAndAttachAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), Not(ContainsAnyAutofillFallbackEntries()));
}

// Tests that when triggering the context menu on an ac=unrecognized field, the
// fallback entry is not part of the menu if there's no suitable AutofillProfile
// data to fill in.
IN_PROC_BROWSER_TEST_F(
    AutocompleteUnrecognizedFieldsTest,
    AutocompleteUnrecognizedFormShown_NoSuitableData_FallbackOptionsNotPresent) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(COMPANY_NAME, u"company");
  AddAutofillProfile(profile);
  FormData form = CreateAndAttachAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), Not(ContainsAnyAutofillFallbackEntries()));
}

// Tests that when triggering the context menu on a classified field that
// has a profile, the fallback entry is not part of the menu if Autofill is
// disabled.
IN_PROC_BROWSER_TEST_F(
    AutocompleteUnrecognizedFieldsTest,
    AutocompleteUnrecognizedFormShown_AutofillDisabled_FallbackOptionsNotPresent) {
  AddAutofillProfile(test::GetFullProfile());
  pref_service().SetBoolean(prefs::kAutofillProfileEnabled, false);
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), Not(ContainsAnyAutofillFallbackEntries()));
}

// Tests that when triggering the context menu on a classified field, the
// fallback entry is part of the menu.
IN_PROC_BROWSER_TEST_F(AutocompleteUnrecognizedFieldsTest,
                       ClassifiedFormShown_FallbackOptionsNotPresent) {
  AddAutofillProfile(test::GetFullProfile());
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), OnlyAddressFallbackAdded());
}

// Tests that when triggering the context menu on an ac=unrecognized field, the
// fallback entry is part of the menu.
IN_PROC_BROWSER_TEST_F(
    AutocompleteUnrecognizedFieldsTest,
    AutocompleteUnrecognizedFormShown_FallbackOptionsPresent) {
  AddAutofillProfile(test::GetFullProfile());
  FormData form = CreateAndAttachAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), OnlyAddressFallbackAdded());
}

// Tests that when the fallback entry for ac=unrecognized fields is selected,
// suggestions are triggered with suggestion trigger source
// `kManualFallbackAddress`.
IN_PROC_BROWSER_TEST_F(AutocompleteUnrecognizedFieldsTest,
                       AutocompleteUnrecognizedFallback_TriggerSuggestions) {
  AddAutofillProfile(test::GetFullProfile());
  FormData form = CreateAndAttachAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  // Expect that when the entry is selected, suggestions are triggered from that
  // field.
  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form.fields[0].renderer_id()},
          AutofillSuggestionTriggerSource::kManualFallbackAddress));
  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS);
}

class UnclassifiedFieldsTest : public BaseAutofillContextMenuManagerTest {
 private:
  base::test::ScopedFeatureList feature_{
      features::kAutofillForUnclassifiedFieldsAvailable};
};

// Tests that when triggering the context menu on an unclassified form the
// address manual fallback is added even if the user has no profile stored.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       NoUserData_AddressManualFallbackPresent) {
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), OnlyAddressFallbackAdded());
}

// Tests that when triggering the context menu on an unclassified form, address
// manual fallback entries are not added when Autofill is disabled, even if the
// user has address data stored.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       HasAddressData_AddressManualFallbackAdded) {
  AddAutofillProfile(test::GetFullProfile());
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), OnlyAddressFallbackAdded());
}

// Tests that when triggering the context menu on an unclassified form, address
// manual fallback entries are not added when Autofill is disabled, even if user
// has address data stored.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       AutofillDisabled_FallbackOptionsNotPresent) {
  AddAutofillProfile(test::GetFullProfile());
  pref_service().SetBoolean(prefs::kAutofillProfileEnabled, false);
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), Not(ContainsAnyAutofillFallbackEntries()));
}

// Tests that when triggering the context menu on an unclassified form the
// address manual fallback is not added in incognito mode.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       NoUserData_IncognitoMode_FallbackOptionsNotPresent) {
  autofill_client()->set_is_off_the_record(true);
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), Not(ContainsAnyAutofillFallbackEntries()));
}

// Tests that even in incognito mode, when triggering the context menu on an
// unclassified form, address manual fallback entries are added when the user
// has address data stored.
IN_PROC_BROWSER_TEST_F(
    UnclassifiedFieldsTest,
    HasAddressData_IncognitoMode_AddressManualFallbackAdded) {
  autofill_client()->set_is_off_the_record(true);
  AddAutofillProfile(test::GetFullProfile());
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), OnlyAddressFallbackAdded());
}

// Tests that when triggering the context menu on an unclassified form, payments
// manual fallback entries are added when the user has credit card data stored.
// Note that the address manual fallback option is always present, unless the
// user is in incognito mode.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       HasCreditCardData_PaymentsManualFallbackAdded) {
  AddCreditCard(test::GetCreditCard());
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), AddressAndPaymentsFallbacksAdded());
}

// Tests that when triggering the context menu on an unclassified form, payments
// manual fallback entries are NOT added if Autofill for payments is disabled.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       PaymentsDisabled_PaymentsManualFallbackNotAdded) {
  AddCreditCard(test::GetCreditCard());
  pref_service().SetBoolean(prefs::kAutofillCreditCardEnabled, false);
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), OnlyAddressFallbackAdded());
}

// Tests that when triggering the context menu on an unclassified form, the
// fallback entry is part of the menu.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       UnclassifiedFormShown_ManualFallbacksPresent) {
  AddAutofillProfile(test::GetFullProfile());
  AddCreditCard(test::GetCreditCard());
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), AddressAndPaymentsFallbacksAdded());
}

// Tests that when triggering the context menu on an autocomplete unrecognized
// field, the fallback entry is part of the menu.
IN_PROC_BROWSER_TEST_F(
    UnclassifiedFieldsTest,
    AutocompleteUnrecognizedFieldShown_ManualFallbacksPresent) {
  AddAutofillProfile(test::GetFullProfile());
  AddCreditCard(test::GetCreditCard());
  FormData form = CreateAndAttachAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), AddressAndPaymentsFallbacksAdded());
}

// Tests that when triggering the context menu on a classified form, the
// fallback entry is part of the menu.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       ClassifiedFormShown_ManualFallbacksPresent) {
  AddAutofillProfile(test::GetFullProfile());
  AddCreditCard(test::GetCreditCard());
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), AddressAndPaymentsFallbacksAdded());
}

// Tests that when the address manual fallback entry for the unclassified fields
// is selected, suggestions are triggered.
IN_PROC_BROWSER_TEST_F(
    UnclassifiedFieldsTest,
    UnclassifiedFormShown_AddressFallbackTriggersSuggestion) {
  AddAutofillProfile(test::GetFullProfile());
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  // Expect that when the entry is selected, suggestions are triggered.
  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form.fields[0].renderer_id()},
          AutofillSuggestionTriggerSource::kManualFallbackAddress));
  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS);
}

class AddNewAddressBubbleTest : public UnclassifiedFieldsTest {
 public:
  void SetUpOnMainThread() override {
    UnclassifiedFieldsTest::SetUpOnMainThread();

    autofill_client()->GetPersonalDataManager()->SetAutofillProfileEnabled(
        true);

    form_ = CreateAndAttachUnclassifiedForm();
    autofill_context_menu_manager()->set_params_for_testing(
        CreateContextMenuParams(form_.renderer_id,
                                form_.fields[0].renderer_id()));
    autofill_context_menu_manager()->AppendItems();

    ASSERT_EQ(AddressBubblesController::FromWebContents(web_contents()),
              nullptr);

    autofill_context_menu_manager()->ExecuteCommand(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS);

    ASSERT_NE(bubble_controller(), nullptr);
  }

 protected:
  AddressBubblesController* bubble_controller() {
    return AddressBubblesController::FromWebContents(web_contents());
  }
  const FormData& form() { return form_; }

 private:
  FormData form_;
};

// Tests that when the address manual fallback entry is selected and there are
// no saved profiles, the "Add new address" bubble is triggered.
IN_PROC_BROWSER_TEST_F(
    AddNewAddressBubbleTest,
    UnclassifiedFormShown_AddressFallbackTriggersAddNewAddressBubble) {
  // Expect that when the entry is selected, the "add new address" bubble is
  // triggered.
  EXPECT_EQ(
      bubble_controller()->GetPageActionIconTootip(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_NEW_ADDRESS_PROMPT_TITLE));
}

// Tests that the "Autofill.ManualFallback.AddNewAddressPromptShown" metric is
// sent when the user accepts the prompt and saves an address via the editor and
// the manual fallback suggestions are triggered.
IN_PROC_BROWSER_TEST_F(AddNewAddressBubbleTest,
                       UnclassifiedFormShown_AddAddressSave) {
  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form().fields[0].renderer_id()},
          AutofillSuggestionTriggerSource::kManualFallbackAddress));

  PersonalDataChangedWaiter waiter(*personal_data_);
  base::HistogramTester histogram_tester;

  // Imitate the user's decision.
  bubble_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kEditAccepted,
      test::GetFullProfile());

  histogram_tester.ExpectUniqueSample(
      "Autofill.ManualFallback.AddNewAddressPromptShown",
      autofill_metrics::AutofillAddNewAddressPromptOutcome::kSaved,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AddedNewAddress",
      autofill_metrics::AutofillManuallyAddedAddressSurface::kContextMenuPrompt,
      /*expected_bucket_count=*/1);

  // Make sure the PDM's async work is done and the callbacks are called.
  std::move(waiter).Wait();
}

// Tests that the "Autofill.ManualFallback.AddNewAddressPromptShown" metric is
// sent when the user declines the prompt.
IN_PROC_BROWSER_TEST_F(AddNewAddressBubbleTest,
                       UnclassifiedFormShown_AddAddressMetricsAreSentOnCancel) {
  base::HistogramTester histogram_tester;

  // Imitate the user's decision.
  bubble_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kDeclined, std::nullopt);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ManualFallback.AddNewAddressPromptShown",
      autofill_metrics::AutofillAddNewAddressPromptOutcome::kCanceled,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AddedNewAddress",
      autofill_metrics::AutofillManuallyAddedAddressSurface::kContextMenuPrompt,
      /*expected_bucket_count=*/0);
}

// Tests that when the payments manual fallback entry for the unclassified
// fields is selected, suggestions are triggered with correct field global id
// and suggestions trigger source.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       UnclassifiedFormShown_PaymentsFallbackTriggersFallback) {
  AddCreditCard(test::GetCreditCard());
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  // Expect that when the entry is selected, suggestions are triggered from that
  // field.
  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form.fields[0].renderer_id()},
          AutofillSuggestionTriggerSource::kManualFallbackPayments));
  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS);
}

// Test parameter data for asserting metrics emission when triggering Autofill
// via manual fallback.
struct ManualFallbackMetricsTestParams {
  // Fallback option displayed in the context menu (address, payments etc).
  const AutofillSuggestionTriggerSource manual_fallback_option;
  // Whether the option above was accepted by the user.
  const bool option_accepted;
  // Whether the field where manual fallback was used is classified or not. If
  // false, an address field with ac=unrecognized in used.
  const bool is_field_unclassified;
  const std::string test_name;
};

// Test fixture that covers metrics emitted when Autofill is triggered via the
// context menu.
class ManualFallbackMetricsTest
    : public BaseAutofillContextMenuManagerTest,
      public ::testing::WithParamInterface<ManualFallbackMetricsTestParams> {
 public:
  // Returns the expected metric that should be emitted depending on the
  // option displayed in the context menu and whether the user accepted it.
  std::string GetExplicitlyTriggeredMetricName() const {
    const ManualFallbackMetricsTestParams& params = GetParam();
    std::string classified_or_unclassified_field_metric_name_substr =
        params.is_field_unclassified
            ? "NotClassifiedAsTargetFilling"
            : "ClassifiedFieldAutocompleteUnrecognized";
    return "Autofill.ManualFallback.ExplicitlyTriggered." +
           classified_or_unclassified_field_metric_name_substr +
           GetFillingProductBucketName();
  }

  // Similar to the method above, but for the total bucket.
  std::string GetExpectedTotalMetricName() const {
    const ManualFallbackMetricsTestParams& params = GetParam();
    if (params.is_field_unclassified) {
      return "Autofill.ManualFallback.ExplicitlyTriggered."
             "NotClassifiedAsTargetFilling.Total";
    }
    return "Autofill.ManualFallback.ExplicitlyTriggered.Total" +
           GetFillingProductBucketName();
  }

 private:
  // Returns the expected bucket (Address or CreditCard) depending on the
  // fallback option being tested.
  std::string GetFillingProductBucketName() const {
    return GetParam().manual_fallback_option ==
                   AutofillSuggestionTriggerSource::kManualFallbackAddress
               ? ".Address"
               : ".CreditCard";
  }
  base::test::ScopedFeatureList feature_{
      features::kAutofillForUnclassifiedFieldsAvailable};
};

IN_PROC_BROWSER_TEST_P(ManualFallbackMetricsTest,
                       EmitExplicitlyTriggeredMetric) {
  const ManualFallbackMetricsTestParams& params = GetParam();
  const bool is_address_manual_fallback =
      params.manual_fallback_option ==
      AutofillSuggestionTriggerSource::kManualFallbackAddress;
  if (is_address_manual_fallback) {
    AddAutofillProfile(test::GetFullProfile());
  } else {
    // When testing credit cards, make sure address fallback is not shown.
    // This makes this test simpler since we will not have to handle the
    // metrics also being emitted when the address manual fallback is shown,
    // therefore also making the test more self contained.
    // Address fallbacks are not shown when no profile exists and the user is in
    // incognito mode.
    autofill_client()->set_is_off_the_record(true);
    AddCreditCard(test::GetCreditCard());
  }
  FormData form = params.is_field_unclassified
                      ? CreateAndAttachUnclassifiedForm()
                      : CreateAndAttachAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  if (params.option_accepted) {
    autofill_context_menu_manager()->ExecuteCommand(
        is_address_manual_fallback
            ? IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS
            : IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS);
  }
  // Expect that when the autofill_manager() is destroyed, the explicitly
  // triggered metric is emitted correctly.
  base::HistogramTester histogram_tester;
  autofill_manager().Reset();

  histogram_tester.ExpectUniqueSample(GetExplicitlyTriggeredMetricName(),
                                      params.option_accepted, 1);
  histogram_tester.ExpectUniqueSample(GetExpectedTotalMetricName(),
                                      params.option_accepted, 1);
}

INSTANTIATE_TEST_SUITE_P(
    BaseAutofillContextMenuManagerTest,
    ManualFallbackMetricsTest,
    ::testing::ValuesIn(std::vector<ManualFallbackMetricsTestParams>(
        {{
             .manual_fallback_option =
                 AutofillSuggestionTriggerSource::kManualFallbackAddress,
             .option_accepted = true,
             .is_field_unclassified = true,
             .test_name = "UnclassifiedField_Address_Accepted",
         },
         {
             .manual_fallback_option =
                 AutofillSuggestionTriggerSource::kManualFallbackAddress,
             .option_accepted = false,
             .is_field_unclassified = true,
             .test_name = "UnclassifiedField_Address_NotAccepted",
         },

         {
             .manual_fallback_option =
                 AutofillSuggestionTriggerSource::kManualFallbackPayments,
             .option_accepted = true,
             .is_field_unclassified = true,
             .test_name = "UnclassifiedField_Payments_Accepted",
         },
         {
             .manual_fallback_option =
                 AutofillSuggestionTriggerSource::kManualFallbackPayments,
             .option_accepted = false,
             .is_field_unclassified = true,
             .test_name = "UnclassifiedField_Payments_NotAccepted",
         },
         {
             .manual_fallback_option =
                 AutofillSuggestionTriggerSource::kManualFallbackAddress,
             .option_accepted = true,
             // This effectively means testing manual fallback on
             // ac=unrecognized fields.
             .is_field_unclassified = false,
             .test_name = "ClassifiedField_Address_NotAccepted",
         },
         {
             .manual_fallback_option =
                 AutofillSuggestionTriggerSource::kManualFallbackAddress,
             .option_accepted = false,
             // This effectively means testing manual fallback on
             // ac=unrecognized fields.
             .is_field_unclassified = false,
             .test_name = "ClassifiedField_Address_Accepted",
         }})),
    [](const ::testing::TestParamInfo<ManualFallbackMetricsTest::ParamType>&
           info) { return info.param.test_name; });

class PlusAddressContextMenuManagerTest
    : public SigninBrowserTestBaseT<BaseAutofillContextMenuManagerTest> {
 public:
  static constexpr char kExcludedDomainEtldPlus1[] = "muh.mah";
  static constexpr char kExcludedDomainUrl[] = "https://muh.mah";

  PlusAddressContextMenuManagerTest() {
    // TODO(b/327562692): Create and use a `PlusAddressTestEnvironment`.
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{plus_addresses::features::kPlusAddressesEnabled,
          {{plus_addresses::features::kEnterprisePlusAddressServerUrl.name,
            "https://foo.bar"},
           {plus_addresses::features::kPlusAddressExcludedSites.name,
            kExcludedDomainEtldPlus1}}},
         {plus_addresses::features::kPlusAddressFallbackFromContextMenu, {}}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    SigninBrowserTestBaseT<
        BaseAutofillContextMenuManagerTest>::SetUpOnMainThread();
    identity_test_env()->MakePrimaryAccountAvailable(
        "plus@plus.plus", signin::ConsentLevel::kSignin);
  }

  plus_addresses::PlusAddressService* plus_address_service() {
    return PlusAddressServiceFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that Plus Address fallbacks are added to unclassified forms.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest, UnclassifiedForm) {
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), OnlyPlusAddressFallbackAdded());
}

// Tests that Plus Address fallbacks are added to classified forms.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest, ClassifiedForm) {
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), OnlyPlusAddressFallbackAdded());
}

// Tests that Plus Address fallbacks are not added in incognito mode if the user
// does not have a Plus Address for the domain.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest,
                       IncognitoModeWithoutPlusAddress) {
  autofill_client()->set_is_off_the_record(true);
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), Not(ContainsAnyAutofillFallbackEntries()));
}

// Tests that Plus Address fallbacks are added in incognito mode if the user
// has a Plus Address for the domain.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest,
                       IncognitoModeWithPlusAddress) {
  const auto kUrl = GURL("https://foo.bar");
  autofill_client()->set_is_off_the_record(true);
  autofill_client()->set_last_committed_primary_main_frame_url(kUrl);
  plus_address_service()->SavePlusProfile(
      url::Origin::Create(kUrl), {.plus_address = "plus+plus@plus.plus"});

  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), OnlyPlusAddressFallbackAdded());
}

// Tests that no Plus Address fallbacks are added on excluded domains.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest, ExcludedDomain) {
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));

  // No entries are added on excluded domains.
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL(kExcludedDomainUrl));
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), Not(ContainsAnyAutofillFallbackEntries()));

  // That is also true for subdirectories on the domain.
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL(kExcludedDomainUrl).Resolve("sub/index.html"));
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), Not(ContainsAnyAutofillFallbackEntries()));

  // On non-excluded sites, the expected context menu entries are added.
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL("https://non-excluded-site.com"));
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), OnlyPlusAddressFallbackAdded());
}

// Tests that selecting the Plus Address manual fallback entry results in
// triggering suggestions with correct field global id and trigger source.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest,
                       ActionTriggersSuggestions) {
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id, form.fields[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form.fields[0].renderer_id()},
          AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses));

  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS);
}

}  // namespace autofill
