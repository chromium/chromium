// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_uitest_util.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/address_save_metrics.h"
#include "components/autofill/core/browser/metrics/manual_fallback_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/plus_addresses/blocked_facets.pb.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_blocklist_data.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync/test/test_sync_user_settings.h"
#include "components/user_manager/user_names.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Property;

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

// Checks if the context menu model contains any entries with
// address/payments/plus address manual fallback labels or command ids. `arg`
// must be of type `ui::SimpleMenuModel`.
MATCHER(ContainsAnyAddressPaymentsOrPlusAddressFallbackEntries, "") {
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

// Checks if the context menu model contains any entries with plus address
// manual fallback labels or command ids. `arg` must be of type
// `ui::SimpleMenuModel`.
MATCHER(ContainsAnyPlusAddressFallbackEntries, "") {
  for (size_t i = 0; i < arg->GetItemCount(); i++) {
    if (arg->GetCommandIdAt(i) ==
            IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS ||
        arg->GetLabelAt(i) ==
            l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_FALLBACK_LABEL_CONTEXT_MENU)) {
      return true;
    }
  }
  return false;
}

// Checks if the context menu model contains the address manual fallback
// entries with correct UI strings. `arg` must be of type `ui::SimpleMenuModel`.
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

// Checks if the context menu model contains the prediction improvement
// entry with correct UI strings. `arg` must be of type `ui::SimpleMenuModel`.
MATCHER(ContainsPredictionImprovementsEntry, "") {
  for (size_t i = 0; i < arg->GetItemCount(); i++) {
    if (arg->GetCommandIdAt(i) ==
            IDC_CONTENT_CONTEXT_AUTOFILL_PREDICTION_IMPROVEMENTS &&
        arg->GetLabelAt(i) ==
            l10n_util::GetStringUTF16(
                IDS_CONTENT_CONTEXT_AUTOFILL_PREDICTION_IMPROVEMENTS)) {
      return true;
    }
  }
  return false;
}

// Checks if the context menu model contains the plus address manual fallback
// entries with correct UI strings. `arg` must be of type `ui::SimpleMenuModel`.
MATCHER(PlusAddressFallbackAdded, "") {
  // TODO(crbug.com/40285811): Remove "if feature enabled" from the comment,
  // once the feature is rolled out.
  // There can be more than 3 entries, because the address manual fallback is
  // always present, on any field, if the autofill for unclassified fields
  // feature is enabled.
  EXPECT_GE(arg->GetItemCount(), 3u);
  EXPECT_EQ(arg->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_TITLE);
  EXPECT_EQ(arg->GetTypeAt(arg->GetItemCount() - 1),
            ui::MenuModel::ItemType::TYPE_SEPARATOR);

  for (size_t i = 0; i < arg->GetItemCount(); i++) {
    if (arg->GetLabelAt(i) ==
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_FALLBACK_LABEL_CONTEXT_MENU)) {
      return true;
    }
  }
  return false;
}

// Checks if the context menu model contains the address and payments manual
// fallback entries with correct UI strings. `arg` must be of type
// `ui::SimpleMenuModel`.
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

// Checks if the context menu model contains the passwords manual fallback
// entries with correct UI strings. `arg` must be of type `ui::SimpleMenuModel`,
// `has_passwords_saved`, `is_password_generation_enabled_for_current_field` and
// `is_passkey_from_another_device_in_context_menu` must be bool.
//
// `has_passwords_saved` is true if the user has any account or
// profile passwords stored.
//
// `is_password_generation_enabled_for_current_field` is true if the password
// generation feature is enabled for this user (note that some non-syncing users
// can also generate passwords, in special conditions) and for the current
// field.
//
// `is_passkey_from_another_device_in_context_menu` is true if passkey
// fallback entry is supposed to be in context menu.
MATCHER_P3(OnlyPasswordsFallbackAdded,
           has_passwords_saved,
           is_password_generation_enabled_for_current_field,
           is_passkey_from_another_device_in_context_menu,
           "") {
  EXPECT_EQ(arg->GetItemCount(), 3u);
  EXPECT_EQ(arg->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_TITLE);
  EXPECT_EQ(
      arg->GetLabelAt(0),
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_TITLE));
  EXPECT_EQ(
      arg->GetLabelAt(1),
      l10n_util::GetStringUTF16(
          is_passkey_from_another_device_in_context_menu
              ? IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORD_AND_PASSKEYS
              : IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS));
  EXPECT_EQ(arg->GetTypeAt(2), ui::MenuModel::ItemType::TYPE_SEPARATOR);

  const bool add_select_password_submenu_option =
      (is_password_generation_enabled_for_current_field &&
       has_passwords_saved) ||
      is_passkey_from_another_device_in_context_menu;
  const bool add_import_passwords_submenu_option = !has_passwords_saved;
  const bool add_submenu =
      add_select_password_submenu_option || add_import_passwords_submenu_option;

  if (!add_submenu) {
    return arg->GetTypeAt(1) == ui::MenuModel::ItemType::TYPE_COMMAND;
  }

  EXPECT_EQ(arg->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SUBMENU);
  ui::MenuModel* submenu = arg->GetSubmenuModelAt(1);

  if (is_password_generation_enabled_for_current_field && has_passwords_saved) {
    EXPECT_EQ(submenu->GetItemCount(),
              is_passkey_from_another_device_in_context_menu ? 3u : 2u);
    EXPECT_EQ(
        submenu->GetLabelAt(0),
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD));
    EXPECT_EQ(
        submenu->GetLabelAt(1),
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD));
    EXPECT_EQ(submenu->GetItemCount(),
              is_passkey_from_another_device_in_context_menu ? 3u : 2u);
    if (is_passkey_from_another_device_in_context_menu) {
      EXPECT_EQ(
          submenu->GetLabelAt(2),
          l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE));
    }
  } else if (add_import_passwords_submenu_option) {
    if (is_password_generation_enabled_for_current_field &&
        is_passkey_from_another_device_in_context_menu) {
      EXPECT_EQ(submenu->GetItemCount(), 4u);
      EXPECT_EQ(
          submenu->GetLabelAt(2),
          l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD));
      EXPECT_EQ(
          submenu->GetLabelAt(3),
          l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE));
    } else if (is_password_generation_enabled_for_current_field) {
      EXPECT_EQ(submenu->GetItemCount(), 3u);
      EXPECT_EQ(
          submenu->GetLabelAt(2),
          l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD));
    } else if (is_passkey_from_another_device_in_context_menu) {
      EXPECT_EQ(submenu->GetItemCount(), 3u);
      EXPECT_EQ(
          submenu->GetLabelAt(2),
          l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE));
    } else {
      EXPECT_EQ(submenu->GetItemCount(), 2u);
    }

    EXPECT_EQ(
        submenu->GetLabelAt(0),
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_NO_SAVED_PASSWORDS));
    EXPECT_EQ(submenu->IsEnabledAt(0), false);
    EXPECT_EQ(
        submenu->GetLabelAt(1),
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS));
  } else {
    EXPECT_EQ(submenu->GetItemCount(), 2u);
    EXPECT_EQ(
        submenu->GetLabelAt(0),
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD));
    EXPECT_EQ(
        submenu->GetLabelAt(1),
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE));
  }
  return true;
}

// Generates a ContextMenuParams for the Autofill context menu options.
content::ContextMenuParams CreateContextMenuParams(
    std::optional<autofill::FormRendererId> form_renderer_id = std::nullopt,
    autofill::FieldRendererId field_render_id = autofill::FieldRendererId(0),
    blink::mojom::FormControlType form_control_type =
        blink::mojom::FormControlType::kInputText) {
  content::ContextMenuParams rv;
  rv.is_editable = true;
  rv.page_url = GURL("http://test.page/");
  rv.form_control_type = form_control_type;
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

// TODO(crbug.com/40286010): Simplify test setup.
class BaseAutofillContextMenuManagerTest : public InProcessBrowserTest {
 public:
  BaseAutofillContextMenuManagerTest() = default;

  BaseAutofillContextMenuManagerTest(
      const BaseAutofillContextMenuManagerTest&) = delete;
  BaseAutofillContextMenuManagerTest& operator=(
      const BaseAutofillContextMenuManagerTest&) = delete;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("http://test.com")));
    personal_data_ =
        PersonalDataManagerFactory::GetForBrowserContext(profile());

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
    size_t profile_count =
        personal_data_->address_data_manager().GetProfiles().size();
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->address_data_manager().AddProfile(profile);
    std::move(waiter).Wait();
    EXPECT_EQ(profile_count + 1,
              personal_data_->address_data_manager().GetProfiles().size());
  }

  void AddCreditCard(const autofill::CreditCard& card) {
    size_t card_count =
        personal_data_->payments_data_manager().GetCreditCards().size();
    PersonalDataChangedWaiter waiter(*personal_data_);
    if (card.record_type() == CreditCard::RecordType::kLocalCard) {
      personal_data_->payments_data_manager().AddCreditCard(card);
    } else {
      test_api(personal_data_->payments_data_manager())
          .AddServerCreditCard(card);
    }
    std::move(waiter).Wait();
    EXPECT_EQ(card_count + 1,
              personal_data_->payments_data_manager().GetCreditCards().size());
  }

  content::RenderFrameHost* main_rfh() {
    return web_contents()->GetPrimaryMainFrame();
  }

  virtual content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  virtual Profile* profile() { return browser()->profile(); }

  ChromePasswordManagerClient* password_manager_client() {
    return ChromePasswordManagerClient::FromWebContents(web_contents());
  }

  password_manager::ContentPasswordManagerDriver* password_manager_driver() {
    return password_manager::ContentPasswordManagerDriver::
        GetForRenderFrameHost(main_rfh());
  }

  void TearDownOnMainThread() override {
    autofill_context_menu_manager_.reset();
    render_view_context_menu_.reset();
    personal_data_ = nullptr;
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
    form.set_host_frame(frame_token);
    for (FormFieldData& field : test_api(form).fields()) {
      field.set_host_frame(frame_token);
    }
  }

  // Makes the form identifiable by its global id and adds the `form` to the
  // `driver()`'s manager.
  void AttachForm(FormData& form) {
    SetHostFramesOfFormAndFields(form);
    TestAutofillManagerSingleEventWaiter wait_for_forms_seen(
        autofill_manager(), &AutofillManager::Observer::OnAfterFormsSeen,
        ElementsAre(form.global_id()), IsEmpty());
    autofill_manager().OnFormsSeen(/*updated_forms=*/{form},
                                   /*removed_forms=*/{});
    ASSERT_TRUE(std::move(wait_for_forms_seen).Wait());
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
    for (FormFieldData& field : test_api(form).fields()) {
      field.set_parsed_autocomplete(AutocompleteParsingResult{
          .field_type = HtmlFieldType::kUnrecognized});
    }
    AttachForm(form);
    return form;
  }

  // Creates a form with unclassifiable fields and registers it with the
  // manager.
  FormData CreateAndAttachUnclassifiedForm() {
    FormData form = test::CreateTestAddressFormData();
    for (FormFieldData& field : test_api(form).fields()) {
      field.set_label(u"unclassifiable");
      field.set_name(u"unclassifiable");
    }
    AttachForm(form);
    return form;
  }

  // Creates a form with a password field and registers it with the
  // manager.
  FormData CreateAndAttachPasswordForm() {
    FormData form;
    form.set_renderer_id(test::MakeFormRendererId());
    form.set_name(u"MyForm");
    form.set_url(GURL("https://myform.com/form.html"));
    form.set_action(GURL("https://myform.com/submit.html"));
    form.set_fields({test::CreateTestFormField(
        "Password", "password", "", FormControlType::kInputPassword)});
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(),
              Not(ContainsAnyAddressPaymentsOrPlusAddressFallbackEntries()));
}

// Tests that when triggering the context menu on an ac=unrecognized field, the
// fallback entry is not part of the menu if the user has no AutofillProfiles
// stored.
IN_PROC_BROWSER_TEST_F(
    AutocompleteUnrecognizedFieldsTest,
    AutocompleteUnrecognizedFormShown_NoAutofillProfiles_FallbackOptionsNotPresent) {
  FormData form = CreateAndAttachAutocompleteUnrecognizedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(),
              Not(ContainsAnyAddressPaymentsOrPlusAddressFallbackEntries()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(),
              Not(ContainsAnyAddressPaymentsOrPlusAddressFallbackEntries()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(),
              Not(ContainsAnyAddressPaymentsOrPlusAddressFallbackEntries()));
}

// Tests that when triggering the context menu on a classified field, the
// fallback entry is part of the menu.
IN_PROC_BROWSER_TEST_F(AutocompleteUnrecognizedFieldsTest,
                       ClassifiedFormShown_FallbackOptionsNotPresent) {
  AddAutofillProfile(test::GetFullProfile());
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  // Expect that when the entry is selected, suggestions are triggered from that
  // field.
  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form.fields()[0].renderer_id()},
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(),
              Not(ContainsAnyAddressPaymentsOrPlusAddressFallbackEntries()));
}

// Tests that when triggering the context menu on an unclassified form the
// address manual fallback is not added in incognito mode.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       NoUserData_IncognitoMode_FallbackOptionsNotPresent) {
  autofill_client()->set_is_off_the_record(true);
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(),
              Not(ContainsAnyAddressPaymentsOrPlusAddressFallbackEntries()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), AddressAndPaymentsFallbacksAdded());
}

// Tests if the prediction improvements entry is not added based on
// `ShouldProvidePredictionImprovements()` returning `false`.
class PredictionImprovementsDisabledTest
    : public BaseAutofillContextMenuManagerTest {
 public:
  void SetUpOnMainThread() override {
    BaseAutofillContextMenuManagerTest::SetUpOnMainThread();
    ON_CALL(*autofill_client()->GetAutofillPredictionImprovementsDelegate(),
            ShouldProvidePredictionImprovements)
        .WillByDefault(::testing::Return(false));
  }
};

// Tests that when triggering the context menu on any form field, the improved
// predictions fallback is not added when the feature is disabled.
IN_PROC_BROWSER_TEST_F(PredictionImprovementsDisabledTest,
                       PredictionImprovementsEntryNotAdded) {
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), Not(ContainsPredictionImprovementsEntry()));
}

// Tests if the prediction improvements entry is added based on
// `ShouldProvidePredictionImprovements()` returning `true`.
class PredictionImprovementsEnabledTest
    : public BaseAutofillContextMenuManagerTest {
 public:
  void SetUpOnMainThread() override {
    BaseAutofillContextMenuManagerTest::SetUpOnMainThread();
    ON_CALL(*autofill_client()->GetAutofillPredictionImprovementsDelegate(),
            ShouldProvidePredictionImprovements)
        .WillByDefault(::testing::Return(true));
  }
};

// Tests that when triggering the context menu on any form field, the improved
// prediction entry point is added.
// TODO(crbug.com/372158654): Implement suitable criteria or remove the entry.
IN_PROC_BROWSER_TEST_F(PredictionImprovementsEnabledTest,
                       PredictionImprovementsEntryAdded) {
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), Not(ContainsPredictionImprovementsEntry()));
}

// Tests that selecting the improved predictions triggers the right command.
IN_PROC_BROWSER_TEST_F(PredictionImprovementsEnabledTest,
                       ActionTriggersSuggestions) {
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form.fields()[0].renderer_id()},
          AutofillSuggestionTriggerSource::kPredictionImprovements));

  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_PREDICTION_IMPROVEMENTS);
}

// Tests that when triggering the context menu on an unclassified form, payments
// manual fallback entries are NOT added if Autofill for payments is disabled.
IN_PROC_BROWSER_TEST_F(UnclassifiedFieldsTest,
                       PaymentsDisabled_PaymentsManualFallbackNotAdded) {
  AddCreditCard(test::GetCreditCard());
  pref_service().SetBoolean(prefs::kAutofillCreditCardEnabled, false);
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  // Expect that when the entry is selected, suggestions are triggered.
  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form.fields()[0].renderer_id()},
          AutofillSuggestionTriggerSource::kManualFallbackAddress));
  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS);
}

class AddNewAddressBubbleTest : public UnclassifiedFieldsTest {
 public:
  void SetUpOnMainThread() override {
    UnclassifiedFieldsTest::SetUpOnMainThread();

    autofill_client()
        ->GetPersonalDataManager()
        ->test_address_data_manager()
        .SetAutofillProfileEnabled(true);

    form_ = CreateAndAttachUnclassifiedForm();
    autofill_context_menu_manager()->set_params_for_testing(
        CreateContextMenuParams(form_.renderer_id(),
                                form_.fields()[0].renderer_id()));
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
                        form().fields()[0].renderer_id()},
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  // Expect that when the entry is selected, suggestions are triggered from that
  // field.
  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form.fields()[0].renderer_id()},
          AutofillSuggestionTriggerSource::kManualFallbackPayments));
  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS);
}

class PasswordsFallbackTest : public BaseAutofillContextMenuManagerTest {
 public:
  PasswordsFallbackTest() {
    feature_list_.InitWithFeatures(
        {password_manager::features::
             kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu,
         password_manager::features::kPasswordManualFallbackAvailable},
        {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    BaseAutofillContextMenuManagerTest::SetUpInProcessBrowserTestFixture();
    // Setting up a testing `SyncServiceFactory`, which returns a
    // `syncer::TestSyncService`. Therefore, syncing can be easily enabled or
    // disabled.
    // Note that in browser tests, one needs to use
    // `BrowserContextDependencyManager::RegisterCreateServicesCallbackForTesting()`
    // in order to set up a testing factory.
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  SyncServiceFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating([](content::BrowserContext*)
                                              -> std::unique_ptr<KeyedService> {
                        return std::make_unique<syncer::TestSyncService>();
                      }));
                }));
  }

  void SetUpOnMainThread() override {
    BaseAutofillContextMenuManagerTest::SetUpOnMainThread();
    // Make sure address fallback is not shown, so that it doesn't interfere
    // with tests which check for the presence of password fallback.
    // Address fallbacks are not shown when no profile exists and the user is in
    // incognito mode.
    autofill_client()->set_is_off_the_record(true);

    form_ = CreateAndAttachPasswordForm();
    autofill_context_menu_manager()->set_params_for_testing(
        CreateContextMenuParams(form_.renderer_id(),
                                form_.fields()[0].renderer_id(),
                                blink::mojom::FormControlType::kInputPassword));
  }

  // This method is used in order to enable/disable password generation. Syncing
  // users are one category of users who have password generation enabled.
  void UpdateSyncStatus(bool sync_enabled) {
    SyncServiceFactory::GetForProfile(profile())
        ->GetUserSettings()
        ->SetSelectedType(syncer::UserSelectableType::kPasswords, sync_enabled);
  }

  const FormData& form() { return form_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription subscription_;
  FormData form_;
};

IN_PROC_BROWSER_TEST_F(
    PasswordsFallbackTest,
    PasswordGenerationEnabled_NoPasswordsSaved_ManualFallbackAddedWithGeneratePasswordOptionAndImportPasswordsOption) {
  UpdateSyncStatus(/*sync_enabled=*/true);
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(),
              OnlyPasswordsFallbackAdded(
                  /*has_passwords_saved=*/false,
                  /*is_password_generation_enabled_for_current_field=*/true,
                  /*is_passkey_from_another_device_in_context_menu=*/true));
}

IN_PROC_BROWSER_TEST_F(
    PasswordsFallbackTest,
    PasswordGenerationDisabled_NoPasswordsSaved_ManualFallbackAddedWithImportPasswordsOption) {
  UpdateSyncStatus(/*sync_enabled=*/false);
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(),
              OnlyPasswordsFallbackAdded(
                  /*has_passwords_saved=*/false,
                  /*is_password_generation_enabled_for_current_field=*/false,
                  /*is_passkey_from_another_device_in_context_menu=*/true));
}

IN_PROC_BROWSER_TEST_F(
    PasswordsFallbackTest,
    PasswordGenerationEnabled_NonPasswordField_NoPasswordsSaved_ManualFallbackAddedWithImportPasswordsOptionAndWithoutGeneratePasswordOption) {
  UpdateSyncStatus(/*sync_enabled=*/true);

  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id(),
                              blink::mojom::FormControlType::kInputText));

  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(),
              OnlyPasswordsFallbackAdded(
                  /*has_passwords_saved=*/false,
                  /*is_password_generation_enabled_for_current_field=*/false,
                  /*is_passkey_from_another_device_in_context_menu=*/true));
}

IN_PROC_BROWSER_TEST_F(PasswordsFallbackTest,
                       SelectPasswordTriggersSuggestions) {
  // Faking the pref value so that the context menu believes the user has
  // passwords saved.
  password_manager_client()->GetPrefs()->SetBoolean(
      password_manager::prefs::
          kAutofillableCredentialsProfileStoreLoginDatabase,
      true);
  autofill_context_menu_manager()->AppendItems();

  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form().fields()[0].renderer_id()},
          AutofillSuggestionTriggerSource::kManualFallbackPasswords));
  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD);
}

IN_PROC_BROWSER_TEST_F(
    PasswordsFallbackTest,
    ImportPasswordsTriggersOpeningPaswordManagerTabAndRecordsMetrics) {
  base::HistogramTester histogram_tester;
  ASSERT_NE(web_contents()->GetLastCommittedURL(),
            "chrome://password-manager/");

  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return web_contents()->GetLastCommittedURL() ==
           "chrome://password-manager/";
  }));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kPasswordContextMenu,
      /*expected_bucket_count=*/1);
}

class PasswordsFallbackWithUIInteractionsTest
    : public BaseAutofillContextMenuManagerTest {
  void SetUpOnMainThread() override {
    // Note that the `SetUpOnMainThread()` of the parent class is intentionally
    // not called, while `TearDownOnMainThread()` is intentionally let to be
    // called.
    //
    // Load an HTML with password forms so that the test can execute JS on the
    // forms.
    ASSERT_TRUE(embedded_test_server()->Start());
    PasswordsNavigationObserver observer(web_contents());
    const GURL url =
        embedded_test_server()->GetURL("/password/password_form.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(observer.Wait());

    // The next lines perform the same set up as the parent class
    // `BaseAutofillContextMenuManagerTest()`, with the exception that a
    // password form is created and attached.
    personal_data_ =
        PersonalDataManagerFactory::GetForBrowserContext(profile());
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    render_view_context_menu_ = std::make_unique<TestRenderViewContextMenu>(
        *main_rfh(), content::ContextMenuParams());
    render_view_context_menu_->Init();
    autofill_context_menu_manager_ =
        std::make_unique<AutofillContextMenuManager>(
            personal_data_, render_view_context_menu_.get(), menu_model_.get());

    FormData form = CreateAndAttachPasswordForm();
    autofill_context_menu_manager()->set_params_for_testing(
        CreateContextMenuParams(form.renderer_id(),
                                form.fields()[0].renderer_id(),
                                blink::mojom::FormControlType::kInputPassword));
  }

 private:
  base::test::ScopedFeatureList feature_{
      password_manager::features::kPasswordManualFallbackAvailable};
};

IN_PROC_BROWSER_TEST_F(
    PasswordsFallbackWithUIInteractionsTest,
    SuggestPasswordTriggersPasswordGenerationAndRecordsMetrics) {
  base::HistogramTester histogram_tester;

  // Focus on a password field so that the agent can allow password generation.
  // It is not relevant (and also no in the scope of the test) whether the
  // password field looks the same as the one provided to
  // `AutofillContextMenuManager`. The agent just needs to know that a password
  // field has focus in order to allow password generation.
  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('password_field').focus();"));
  TestGenerationPopupObserver generation_popup_observer;
  ChromePasswordManagerClient::FromWebContents(web_contents())
      ->SetTestObserver(&generation_popup_observer);
  ASSERT_FALSE(generation_popup_observer.popup_showing());

  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD);
  generation_popup_observer.WaitForStatus(
      TestGenerationPopupObserver::GenerationPopup::kShown);
  EXPECT_TRUE(generation_popup_observer.popup_showing());
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.Event",
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_PRESSED,
      /*expected_bucket_count=*/1);

  // Hide the password generation popup to avoid the test crashing.
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents());
  client->SetCurrentTargetFrameForTesting(
      web_contents()->GetPrimaryMainFrame());
  client->PasswordGenerationRejectedByTyping();
  client->SetCurrentTargetFrameForTesting(nullptr);
}

enum class PasswordDatabaseEntryType {
  kNormal,
  kBlocklisted,
  kFederated,
  kUsernameOnly,
};

// Not all password database entries are autofillable. This tests fixture goes
// through all relevant categories of password database entries: normal
// credentials, blocklisted entries, federated credentials and username-only
// credentials. Only the first category is autofillable.
// The tests in this fixture test that the "Select password" entry is displayed
// if and only if they have at least one normal credential in the password
// database.
class PasswordsFallbackWithPasswordDatabaseEntriesTest
    : public PasswordsFallbackTest,
      public testing::WithParamInterface<
          std::tuple<bool, PasswordDatabaseEntryType>> {
 public:
  void AddPasswordToStore() {
    password_manager::PasswordStoreInterface* password_store =
        use_profile_store()
            ? ProfilePasswordStoreFactory::GetForProfile(
                  browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                  .get()
            : AccountPasswordStoreFactory::GetForProfile(
                  browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                  .get();

    password_manager::PasswordForm password_form;
    password_form.signon_realm = "http://test.com";
    password_form.url = GURL("http://test.com");
    switch (password_database_entry_type()) {
      case PasswordDatabaseEntryType::kNormal:
        break;
      case PasswordDatabaseEntryType::kBlocklisted:
        password_form.blocked_by_user = true;
        break;
      case PasswordDatabaseEntryType::kFederated:
        password_form.federation_origin =
            url::SchemeHostPort(GURL("http://test.com"));
        break;
      case PasswordDatabaseEntryType::kUsernameOnly:
        password_form.scheme =
            password_manager::PasswordForm::Scheme::kUsernameOnly;
        break;
    }

    const std::string pref =
        use_profile_store()
            ? password_manager::prefs::
                  kAutofillableCredentialsProfileStoreLoginDatabase
            : password_manager::prefs::
                  kAutofillableCredentialsAccountStoreLoginDatabase;

    if (!has_autofillable_credentials()) {
      // `base::test::RunUntil()` can detect whether a change in the prefs
      // occur, but cannot detect anything if the prefs don't change. The pref
      // is set to `true`, because it is expected to turn to `false` when
      // `PasswordStoreInterface::AddLogin()` is called.
      password_manager_client()->GetPrefs()->SetBoolean(pref, true);
    }

    password_store->AddLogin(password_form);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return password_manager_client()->GetPrefs()->GetBoolean(pref) ==
             has_autofillable_credentials();
    })) << "Adding the login timed out.";
  }

  // If false, then use account store.
  bool use_profile_store() { return std::get<0>(GetParam()); }

  PasswordDatabaseEntryType password_database_entry_type() {
    return std::get<1>(GetParam());
  }

  bool has_autofillable_credentials() {
    return password_database_entry_type() == PasswordDatabaseEntryType::kNormal;
  }

 private:
  base::test::ScopedFeatureList feature_{
      password_manager::features::kPasswordManualFallbackAvailable};
};

IN_PROC_BROWSER_TEST_P(
    PasswordsFallbackWithPasswordDatabaseEntriesTest,
    PasswordGenerationEnabled_HasPasswordDatabaseEntries_ManualFallbackAddedWithGeneratePasswordOption) {
  UpdateSyncStatus(/*sync_enabled=*/true);
  AddPasswordToStore();

  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(),
              OnlyPasswordsFallbackAdded(
                  /*has_passwords_saved=*/has_autofillable_credentials(),
                  /*is_password_generation_enabled_for_current_field=*/true,
                  /*is_passkey_from_another_device_in_context_menu=*/true));
}

IN_PROC_BROWSER_TEST_P(
    PasswordsFallbackWithPasswordDatabaseEntriesTest,
    PasswordGenerationDisabled_HasPasswordDatabaseEntries_ManualFallbackAddedWithoutGeneratePasswordOption) {
  UpdateSyncStatus(/*sync_enabled=*/false);
  AddPasswordToStore();

  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(),
              OnlyPasswordsFallbackAdded(
                  /*has_passwords_saved=*/has_autofillable_credentials(),
                  /*is_password_generation_enabled_for_current_field=*/false,
                  /*is_passkey_from_another_device_in_context_menu=*/true));
}

IN_PROC_BROWSER_TEST_P(
    PasswordsFallbackWithPasswordDatabaseEntriesTest,
    PasswordGenerationEnabled_NonPasswordField_HasPasswordDatabaseEntries_ManualFallbackAddedWithoutGeneratePasswordOption) {
  UpdateSyncStatus(/*sync_enabled=*/true);
  AddPasswordToStore();

  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id(),
                              blink::mojom::FormControlType::kInputText));

  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(),
              OnlyPasswordsFallbackAdded(
                  /*has_passwords_saved=*/has_autofillable_credentials(),
                  /*is_password_generation_enabled_for_current_field=*/false,
                  /*is_passkey_from_another_device_in_context_menu=*/true));
}

INSTANTIATE_TEST_SUITE_P(
    PasswordsFallbackTest,
    PasswordsFallbackWithPasswordDatabaseEntriesTest,
    testing::Combine(
        testing::Bool(),
        testing::Values(PasswordDatabaseEntryType::kNormal,
                        PasswordDatabaseEntryType::kBlocklisted,
                        PasswordDatabaseEntryType::kFederated,
                        PasswordDatabaseEntryType::kUsernameOnly)));

class PasswordsFallbackWithGuestProfileTest : public PasswordsFallbackTest {
 public:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    user_manager::kGuestUserName);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }
#else
  void SetUpOnMainThread() override {
    guest_browser_ = CreateGuestBrowser();
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(guest_browser_, GURL("http://test.com")));
    PasswordsFallbackTest::SetUpOnMainThread();
  }

  content::WebContents* web_contents() const override {
    return guest_browser_->tab_strip_model()->GetActiveWebContents();
  }

  Profile* profile() override { return guest_browser_->profile(); }

  void TearDownOnMainThread() override {
    // Release raw_ptr's so they don't become dangling.
    guest_browser_ = nullptr;
    PasswordsFallbackTest::TearDownOnMainThread();
  }
#endif

 private:
  base::test::ScopedFeatureList feature_{
      password_manager::features::kPasswordManualFallbackAvailable};
  raw_ptr<Browser> guest_browser_ = nullptr;
};

// When filling is disabled (for example in guest profiles), manual fallback
// should not be offered.
IN_PROC_BROWSER_TEST_F(PasswordsFallbackWithGuestProfileTest,
                       NoManualFallback) {
  autofill_context_menu_manager()->AppendItems();
  EXPECT_EQ(menu_model()->GetItemCount(), 0u);
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
  ManualFallbackMetricsTest() {
    features_.InitWithFeatures(
        {features::kAutofillForUnclassifiedFieldsAvailable,
         password_manager::features::kPasswordManualFallbackAvailable},
        {});
  }
  void SetUpOnMainThread() override {
    BaseAutofillContextMenuManagerTest::SetUpOnMainThread();
    // When not testing addresses, make sure address fallback is not shown.
    // This makes this test simpler since we will not have to handle the
    // metrics also being emitted when the address manual fallback is shown,
    // therefore also making the test more self contained.
    // Address fallbacks are not shown when no profile exists and the user
    // is in incognito mode.
    if (GetParam().manual_fallback_option !=
        AutofillSuggestionTriggerSource::kManualFallbackAddress) {
      autofill_client()->set_is_off_the_record(true);
    }

    switch (GetParam().manual_fallback_option) {
      case AutofillSuggestionTriggerSource::kManualFallbackAddress:
        AddAutofillProfile(test::GetFullProfile());
        break;
      case AutofillSuggestionTriggerSource::kManualFallbackPayments:
        AddCreditCard(test::GetCreditCard());
        break;
      case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
        // Faking the pref value so that the context menu believes the user has
        // passwords saved.
        password_manager_client()->GetPrefs()->SetBoolean(
            password_manager::prefs::
                kAutofillableCredentialsProfileStoreLoginDatabase,
            true);
        break;
      default:
        NOTREACHED();
    }
  }

  // Returns the expected metric that should be emitted depending on the
  // option displayed in the context menu and whether the user accepted it.
  std::string GetExplicitlyTriggeredMetricName() const {
    const ManualFallbackMetricsTestParams& params = GetParam();
    std::string classified_or_unclassified_field_metric_name_substr =
        [&]() -> std::string {
      if (params.is_field_unclassified) {
        return "NotClassifiedAsTargetFilling";
      }
      // The field is classified as target filling.
      switch (params.manual_fallback_option) {
        // For addresses, the field is classified as target filling only if the
        // field has unrecognized autocomplete.
        case AutofillSuggestionTriggerSource::kManualFallbackAddress:
          return "ClassifiedFieldAutocompleteUnrecognized";
        case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
          return "ClassifiedAsTargetFilling";
        default:
          NOTREACHED();
      }
    }();

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
    // Only addresses have a "Total" variant of the metric when the field is
    // classified as target filling.
    CHECK_EQ(params.manual_fallback_option,
             AutofillSuggestionTriggerSource::kManualFallbackAddress);
    return "Autofill.ManualFallback.ExplicitlyTriggered.Total.Address";
  }

  int CommandToExecute() const {
    switch (GetParam().manual_fallback_option) {
      case AutofillSuggestionTriggerSource::kManualFallbackAddress:
        return IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS;
      case AutofillSuggestionTriggerSource::kManualFallbackPayments:
        return IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS;
      case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
        return IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD;
      default:
        NOTREACHED();
    }
  }

  FormData CreateAndAttachForm() {
    if (GetParam().is_field_unclassified) {
      return CreateAndAttachUnclassifiedForm();
    }
    // The field is classified as target filling.
    switch (GetParam().manual_fallback_option) {
      // For addresses, the field is classified as target filling only if the
      // field has unrecognized autocomplete.
      case AutofillSuggestionTriggerSource::kManualFallbackAddress:
        return CreateAndAttachAutocompleteUnrecognizedForm();
      case AutofillSuggestionTriggerSource::kManualFallbackPasswords: {
        // Create a password form manager for this form, to simulate that its
        // fields are classified as password form fields.
        FormData form = CreateAndAttachPasswordForm();
        password_manager::PasswordFormManager::
            set_wait_for_server_predictions_for_filling(false);
        password_manager::PasswordManager* password_manager =
            static_cast<password_manager::PasswordManager*>(
                password_manager_driver()->GetPasswordManager());
        password_manager->OnPasswordFormsParsed(password_manager_driver(),
                                                {form});
        // Wait until `form` gets parsed.
        EXPECT_TRUE(base::test::RunUntil([&]() {
          return password_manager->GetPasswordFormCache()->GetPasswordForm(
              password_manager_driver(), form.renderer_id());
        }));
        return form;
      }
      default:
        NOTREACHED();
    }
  }

 private:
  // Returns the expected histogram variant (Address, CreditCard or Password)
  // depending on the fallback option being tested.
  std::string GetFillingProductBucketName() const {
    switch (GetParam().manual_fallback_option) {
      case AutofillSuggestionTriggerSource::kManualFallbackAddress:
        return ".Address";
      case AutofillSuggestionTriggerSource::kManualFallbackPayments:
        return ".CreditCard";
      case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
        return ".Password";
      default:
        NOTREACHED();
    }
  }

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(ManualFallbackMetricsTest,
                       EmitExplicitlyTriggeredMetric) {
  const ManualFallbackMetricsTestParams& params = GetParam();
  FormData form = CreateAndAttachForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  if (params.option_accepted) {
    autofill_context_menu_manager()->ExecuteCommand(CommandToExecute());
  }

  base::HistogramTester histogram_tester;
  // Trigger navigation so that metrics are emitted. On navigation, the
  // `AutofillManager` destroys the autofill metrics recorders, and the
  // `PasswordAutofillManager` destroys the passwords metrics recorder. The
  // destructors of the metrics recorders emit metrics.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("http://navigation.com")));

  histogram_tester.ExpectUniqueSample(GetExplicitlyTriggeredMetricName(),
                                      params.option_accepted, 1);
  // Only classified password fields don't emit a "Total" variant of the metric,
  // because only passwords record the "ClassifiedAsTargetFilling" metric
  // variant. I.e. The other filling products (addresses and credit cards), do
  // not record the "ClassifiedAsTargetFilling" metric variant. This is because
  // classified address fields fall into the autocomplete
  // recognized/unrecognized metrics, while classified credit card fields do not
  // trigger any different behaviour and work the same as regular left click
  // (therefore, no specific metric is emitted for them). On the other hand,
  // password manual fallback always triggers a different behavior on
  // right-click (suggestions have a search bar).
  if (params.manual_fallback_option !=
          AutofillSuggestionTriggerSource::kManualFallbackPasswords ||
      params.is_field_unclassified) {
    histogram_tester.ExpectUniqueSample(GetExpectedTotalMetricName(),
                                        params.option_accepted, 1);
  }
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
         },
         {
             .manual_fallback_option =
                 AutofillSuggestionTriggerSource::kManualFallbackPasswords,
             .option_accepted = true,
             .is_field_unclassified = true,
             .test_name = "UnclassifiedField_Passwords_Accepted",
         },
         {
             .manual_fallback_option =
                 AutofillSuggestionTriggerSource::kManualFallbackPasswords,
             .option_accepted = false,
             .is_field_unclassified = true,
             .test_name = "UnclassifiedField_Passwords_NotAccepted",
         },
         {
             .manual_fallback_option =
                 AutofillSuggestionTriggerSource::kManualFallbackPasswords,
             .option_accepted = true,
             .is_field_unclassified = false,
             .test_name = "ClassifiedField_Passwords_Accepted",
         },
         {
             .manual_fallback_option =
                 AutofillSuggestionTriggerSource::kManualFallbackPasswords,
             .option_accepted = false,
             .is_field_unclassified = false,
             .test_name = "ClassifiedField_Passwords_NotAccepted",
         }})),
    [](const ::testing::TestParamInfo<ManualFallbackMetricsTest::ParamType>&
           info) { return info.param.test_name; });

class PlusAddressContextMenuManagerTest
    : public SigninBrowserTestBaseT<BaseAutofillContextMenuManagerTest> {
 public:
  static constexpr char kExcludedDomainRegex[] = "muh\\.mah$";
  static constexpr char kExcludedDomainUrl[] = "https://muh.mah";

  PlusAddressContextMenuManagerTest() {
    // TODO(crbug.com/327562692): Create and use a `PlusAddressTestEnvironment`.
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{plus_addresses::features::kPlusAddressesEnabled,
          {{plus_addresses::features::kEnterprisePlusAddressServerUrl.name,
            "https://foo.bar"}}},
         {plus_addresses::features::kPlusAddressFallbackFromContextMenu, {}},
         {plus_addresses::features::kPlusAddressBlocklistEnabled, {}}},
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
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), PlusAddressFallbackAdded());
}

// Tests that Plus Address fallbacks are added to classified forms.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest, ClassifiedForm) {
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), PlusAddressFallbackAdded());
}

// Tests that no Plus Address fallbacks are shown on password fields.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest, PasswordForm) {
  FormData form = CreateAndAttachPasswordForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id(),
                              blink::mojom::FormControlType::kInputPassword));
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), Not(ContainsAnyPlusAddressFallbackEntries()));
}

// Tests that Plus Address fallbacks are not added in incognito mode if the user
// does not have a Plus Address for the domain.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest,
                       IncognitoModeWithoutPlusAddress) {
  autofill_client()->set_is_off_the_record(true);
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), Not(ContainsAnyPlusAddressFallbackEntries()));
}

// Tests that Plus Address fallbacks are added in incognito mode if the user
// has a Plus Address for the domain.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest,
                       IncognitoModeWithPlusAddress) {
  const auto kUrl = GURL("https://foo.com");
  autofill_client()->set_is_off_the_record(true);
  autofill_client()->set_last_committed_primary_main_frame_url(kUrl);
  plus_address_service()->SavePlusProfile(
      plus_addresses::test::CreatePlusProfile());

  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), PlusAddressFallbackAdded());
}

// Tests that no Plus Address fallbacks are added on excluded domains.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest, ExcludedDomain) {
  plus_addresses::CompactPlusAddressBlockedFacets blocked_facets;
  blocked_facets.set_exclusion_pattern(kExcludedDomainRegex);
  plus_addresses::PlusAddressBlocklistData::GetInstance()
      .PopulateDataFromComponent(blocked_facets.SerializeAsString());

  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));

  // No entries are added on excluded domains.
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL(kExcludedDomainUrl));
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), Not(ContainsAnyPlusAddressFallbackEntries()));

  // That is also true for subdirectories on the domain.
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL(kExcludedDomainUrl).Resolve("sub/index.html"));
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), Not(ContainsAnyPlusAddressFallbackEntries()));
}

// Tests that Plus Address fallbacks are added on non-excluded domains.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest, NonExcludedDomain) {
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));

  // On non-excluded sites, the expected context menu entries are added.
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL("https://non-excluded-site.com"));
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), PlusAddressFallbackAdded());
}

// Tests that selecting the Plus Address manual fallback entry results in
// triggering suggestions with correct field global id and trigger source.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest,
                       ActionTriggersSuggestions) {
  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_CALL(
      *driver(),
      RendererShouldTriggerSuggestions(
          FieldGlobalId{LocalFrameToken(main_rfh()->GetFrameToken().value()),
                        form.fields()[0].renderer_id()},
          AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses));

  autofill_context_menu_manager()->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS);
}

}  // namespace
}  // namespace autofill
