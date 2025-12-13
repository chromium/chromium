// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/password_manager/password_manager_uitest_util.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/plus_addresses/core/browser/blocked_facets.pb.h"
#include "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#include "components/plus_addresses/core/browser/plus_address_blocklist_data.h"
#include "components/plus_addresses/core/browser/plus_address_service.h"
#include "components/plus_addresses/core/browser/plus_address_test_utils.h"
#include "components/plus_addresses/core/browser/plus_address_types.h"
#include "components/plus_addresses/core/common/features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;

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

// Checks if the context menu model contains the plus address manual fallback
// entries with correct UI strings. `arg` must be of type `ui::SimpleMenuModel`.
MATCHER(PlusAddressFallbackAdded, "") {
  // There can be more than 2 entries, if other manual fallbacks are present
  // too.
  EXPECT_GE(arg->GetItemCount(), 2u);
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

// Checks if the context menu model contains the passwords manual fallback
// entries with correct UI strings. `arg` must be of type `ui::SimpleMenuModel`,
// `has_passwords_saved`, `is_password_generation_enabled_for_current_field`,
// `is_passkey_from_another_device_available` must be bool.
//
// `has_passwords_saved` is true if the user has any account or
// profile passwords stored.
//
// `is_password_generation_enabled_for_current_field` is true if the password
// generation feature is enabled for this user (note that some non-syncing users
// can also generate passwords, in special conditions) and for the current
// field.
//
// `is_passkey_from_another_device_available` is true iff the focused field
// supports WebAuthn conditional UI.
MATCHER_P3(OnlyPasswordsFallbackAdded,
           has_passwords_saved,
           is_password_generation_enabled_for_current_field,
           is_passkey_from_another_device_available,
           "") {
  const bool add_select_password_option = has_passwords_saved;
  const bool add_import_passwords_option = !has_passwords_saved;

  size_t current_context_menu_position = 0;
  if (add_select_password_option) {
    EXPECT_EQ(
        arg->GetLabelAt(current_context_menu_position),
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD));
    ++current_context_menu_position;
  }
  if (is_password_generation_enabled_for_current_field) {
    EXPECT_EQ(
        arg->GetLabelAt(current_context_menu_position),
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD));
    ++current_context_menu_position;
  }
  if (is_passkey_from_another_device_available) {
    EXPECT_EQ(
        arg->GetLabelAt(current_context_menu_position),
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE));
    ++current_context_menu_position;
  }
  if (add_import_passwords_option) {
    EXPECT_EQ(
        arg->GetLabelAt(current_context_menu_position),
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS));
    ++current_context_menu_position;
  }

  EXPECT_EQ(arg->GetTypeAt(current_context_menu_position),
            ui::MenuModel::ItemType::TYPE_SEPARATOR);
  ++current_context_menu_position;
  return arg->GetItemCount() == current_context_menu_position;
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

    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    render_view_context_menu_ = std::make_unique<TestRenderViewContextMenu>(
        *main_rfh(), content::ContextMenuParams());
    render_view_context_menu_->Init();
    autofill_context_menu_manager_ =
        std::make_unique<AutofillContextMenuManager>(
            render_view_context_menu_.get(), menu_model_.get());
    autofill_context_menu_manager()->set_params_for_testing(
        CreateContextMenuParams());
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
  FormData CreateAndAttachPasswordForm(bool is_webauthn = false) {
    FormData form;
    form.set_renderer_id(test::MakeFormRendererId());
    form.set_name(u"MyForm");
    form.set_url(GURL("https://myform.com/"));
    form.set_action(GURL("https://myform.com/submit.html"));
    form.set_fields({test::CreateTestFormField(
        /*label=*/"Password", /*name=*/"password", /*value=*/"",
        /*type=*/FormControlType::kInputPassword,
        is_webauthn ? /*autocomplete=*/"webauthn" : "")});
    password_manager::PasswordFormManager::
        set_wait_for_server_predictions_for_filling(false);
    OverrideLastCommittedOrigin(main_rfh(), url::Origin::Create(form.url()));
    AttachForm(form);
    password_manager::PasswordManagerInterface* password_manager =
        password_manager_driver()->GetPasswordManager();
    password_manager->OnPasswordFormsParsed(password_manager_driver(), {form});
    // First parsing is done for filling case. Password forms are only parsed
    // when filling is enabled.
    if (password_manager_client()->IsFillingEnabled(GURL(form.url()))) {
      // Wait until `form` gets parsed.
      EXPECT_TRUE(base::test::RunUntil([&]() {
        return password_manager->GetPasswordFormCache()->GetPasswordForm(
            password_manager_driver(), form.renderer_id());
      }));
    }

    return form;
  }

 protected:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<MockAutofillDriver> autofill_driver_injector_;
  std::unique_ptr<TestRenderViewContextMenu> render_view_context_menu_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<AutofillContextMenuManager> autofill_context_menu_manager_;
};

class PasswordsFallbackTestBase : public BaseAutofillContextMenuManagerTest {
 public:
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

 protected:
  FormData form_;

 private:
  base::CallbackListSubscription subscription_;
};

class PasswordManualFallbackTest : public PasswordsFallbackTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  PasswordManualFallbackTest() {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {password_manager::features::kPasswordManualFallbackAvailable,
           password_manager::features::
               kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {password_manager::features::kPasswordManualFallbackAvailable},
          {password_manager::features::
               kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu});
    }
  }

  void SetUpOnMainThread() override {
    PasswordsFallbackTestBase::SetUpOnMainThread();

    form_ = CreateAndAttachPasswordForm(/*is_webauthn=*/GetParam());
    autofill_context_menu_manager()->set_params_for_testing(
        CreateContextMenuParams(form_.renderer_id(),
                                form_.fields()[0].renderer_id(),
                                blink::mojom::FormControlType::kInputPassword));

    webauthn_delegate()->OnCredentialsReceived(
        {}, ChromeWebAuthnCredentialsDelegate::SecurityKeyOrHybridFlowAvailable(
                true));
  }

  ChromeWebAuthnCredentialsDelegate* webauthn_delegate() {
    return ChromeWebAuthnCredentialsDelegateFactory::GetFactory(
               content::WebContents::FromRenderFrameHost(main_rfh()))
        ->GetDelegateForFrame(main_rfh());
    ;
  }

 private:
  raw_ptr<ChromeWebAuthnCredentialsDelegate> webauthn_delegate_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    PasswordManualFallbackTest,
    PasswordGenerationEnabled_NoPasswordsSaved_ManualFallbackAddedWithGeneratePasswordOptionAndImportPasswordsOption) {
  UpdateSyncStatus(/*sync_enabled=*/true);
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(),
              OnlyPasswordsFallbackAdded(false, true, GetParam()));
}

IN_PROC_BROWSER_TEST_P(
    PasswordManualFallbackTest,
    PasswordGenerationDisabled_NoPasswordsSaved_ManualFallbackAddedWithImportPasswordsOption) {
  UpdateSyncStatus(/*sync_enabled=*/false);
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(),
              OnlyPasswordsFallbackAdded(false, false, GetParam()));
}

IN_PROC_BROWSER_TEST_P(
    PasswordManualFallbackTest,
    PasswordGenerationDisabled_NoPasswordsSaved_SecurityKeyOrHybridFlowNotAvailable_ManualFallbackDoesntHavePasskeyEntry) {
  UpdateSyncStatus(/*sync_enabled=*/false);
  webauthn_delegate()->OnCredentialsReceived(
      {}, ChromeWebAuthnCredentialsDelegate::SecurityKeyOrHybridFlowAvailable(
              false));
  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(),
              OnlyPasswordsFallbackAdded(
                  /*has_passwords_saved=*/false,
                  /*is_password_generation_enabled_for_current_field=*/false,
                  /*is_passkey_from_another_device_available=*/false));
}

IN_PROC_BROWSER_TEST_P(
    PasswordManualFallbackTest,
    PasswordGenerationEnabled_NonPasswordField_NoPasswordsSaved_ManualFallbackAddedWithImportPasswordsOptionAndWithoutGeneratePasswordOption) {
  UpdateSyncStatus(/*sync_enabled=*/true);

  FormData form = CreateAndAttachUnclassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id(),
                              blink::mojom::FormControlType::kInputText));

  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), OnlyPasswordsFallbackAdded(false, false, false));
}

IN_PROC_BROWSER_TEST_P(PasswordManualFallbackTest,
                       SelectPasswordTriggersSuggestions) {
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordStoreWaiter add_waiter(password_store);
  password_manager::PasswordForm existing_form;
  existing_form.username_value = u"username";
  existing_form.password_value = u"password";
  existing_form.signon_realm = "http://test.com";
  existing_form.url = GURL(existing_form.signon_realm);
  password_store->AddLogin(existing_form);
  add_waiter.WaitOrReturn();

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

IN_PROC_BROWSER_TEST_P(
    PasswordManualFallbackTest,
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

INSTANTIATE_TEST_SUITE_P(PasswordsManualFallbackTest,
                         PasswordManualFallbackTest,
                         testing::Bool());

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
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    render_view_context_menu_ = std::make_unique<TestRenderViewContextMenu>(
        *main_rfh(), content::ContextMenuParams());
    render_view_context_menu_->Init();
    autofill_context_menu_manager_ =
        std::make_unique<AutofillContextMenuManager>(
            render_view_context_menu_.get(), menu_model_.get());
    autofill_client()
        ->GetPersonalDataManager()
        .test_address_data_manager()
        .SetAutofillProfileEnabled(false);

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
    : public PasswordsFallbackTestBase,
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

    password_manager::PasswordStoreWaiter add_waiter(password_store);
    password_store->AddLogin(password_form);
    add_waiter.WaitOrReturn();
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
    PasswordGenerationEnabled_HasPasswordDatabaseEntries_TriggeredOnContenteditable_NoEntriesAdded) {
  UpdateSyncStatus(/*sync_enabled=*/true);
  AddPasswordToStore();

  FormData form = CreateAndAttachPasswordForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id(),
                              blink::mojom::FormControlType::kTextArea));

  autofill_context_menu_manager()->AppendItems();
  // Password manual fallback entry should not be added if the context menu was
  // triggered on a text area.
  EXPECT_THAT(menu_model()->GetItemCount(), ::testing::Eq(0));
}

IN_PROC_BROWSER_TEST_P(
    PasswordsFallbackWithPasswordDatabaseEntriesTest,
    PasswordGenerationEnabled_HasPasswordDatabaseEntries_ManualFallbackAddedWithGeneratePasswordOption) {
  UpdateSyncStatus(/*sync_enabled=*/true);
  AddPasswordToStore();

  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), OnlyPasswordsFallbackAdded(
                                has_autofillable_credentials(), true, false));
}

IN_PROC_BROWSER_TEST_P(
    PasswordsFallbackWithPasswordDatabaseEntriesTest,
    PasswordGenerationDisabled_HasPasswordDatabaseEntries_ManualFallbackAddedWithoutGeneratePasswordOption) {
  UpdateSyncStatus(/*sync_enabled=*/false);
  AddPasswordToStore();

  autofill_context_menu_manager()->AppendItems();
  EXPECT_THAT(menu_model(), OnlyPasswordsFallbackAdded(
                                has_autofillable_credentials(), false, false));
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
  EXPECT_THAT(menu_model(), OnlyPasswordsFallbackAdded(
                                has_autofillable_credentials(), false, false));
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

class PasswordsFallbackWithGuestProfileTest : public PasswordsFallbackTestBase {
 public:
#if BUILDFLAG(IS_CHROMEOS)
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
    PasswordsFallbackTestBase::SetUpOnMainThread();
  }

  content::WebContents* web_contents() const override {
    return guest_browser_->tab_strip_model()->GetActiveWebContents();
  }

  Profile* profile() override { return guest_browser_->profile(); }

  void TearDownOnMainThread() override {
    // Release raw_ptr's so they don't become dangling.
    guest_browser_ = nullptr;
    PasswordsFallbackTestBase::TearDownOnMainThread();
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

// Test parameter data for asserting metrics emission when triggering Passwords
// manual fallback.
struct SelectPasswordFallbackMetricsTestParams {
  // Whether the context menu option was accepted by the user.
  const bool option_accepted;
  // Whether the field where manual fallback was used is classified or not.
  const bool is_field_unclassified;
  const std::string test_name;
};

// Test fixture that covers metrics emitted when Passwords are triggered via the
// context menu.
class SelectPasswordFallbackMetricsTest
    : public BaseAutofillContextMenuManagerTest,
      public ::testing::WithParamInterface<
          SelectPasswordFallbackMetricsTestParams> {
 public:
  void SetUpOnMainThread() override {
    BaseAutofillContextMenuManagerTest::SetUpOnMainThread();
    // Add a saved password so the manual fallback option shows.
    password_manager::PasswordStoreInterface* password_store =
        ProfilePasswordStoreFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
            .get();
    password_manager::PasswordStoreWaiter add_waiter(password_store);
    password_manager::PasswordForm form;
    form.username_value = u"username";
    form.password_value = u"password";
    form.signon_realm = "http://example.com";
    form.url = GURL(form.signon_realm);
    password_store->AddLogin(form);
    add_waiter.WaitOrReturn();
  }

  // Returns the expected metric that should be emitted depending on the
  // field classification.
  std::string GetExplicitlyTriggeredMetricName() const {
    std::string_view classified_or_unclassified_field_metric_name_substr =
        GetParam().is_field_unclassified ? "NotClassifiedAsTargetFilling"
                                         : "ClassifiedAsTargetFilling";
    return base::StrCat({"Autofill.ManualFallback.ExplicitlyTriggered.",
                         classified_or_unclassified_field_metric_name_substr,
                         ".Password"});
  }

 private:
  base::test::ScopedFeatureList feature_{
      password_manager::features::kPasswordManualFallbackAvailable};
};

IN_PROC_BROWSER_TEST_P(SelectPasswordFallbackMetricsTest,
                       EmitExplicitlyTriggeredMetric) {
  const SelectPasswordFallbackMetricsTestParams& params = GetParam();
  FormData form = params.is_field_unclassified
                      ? CreateAndAttachUnclassifiedForm()
                      : CreateAndAttachPasswordForm();

  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  if (params.option_accepted) {
    autofill_context_menu_manager()->ExecuteCommand(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD);
  }

  base::HistogramTester histogram_tester;
  // Trigger navigation so that metrics are emitted. On navigation, the
  // `PasswordAutofillManager` destroys the passwords metrics recorder. The
  // destructors of the metrics recorder emit metrics.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("http://navigation.com")));

  histogram_tester.ExpectUniqueSample(GetExplicitlyTriggeredMetricName(),
                                      params.option_accepted, 1);
}

INSTANTIATE_TEST_SUITE_P(
    BaseAutofillContextMenuManagerTest,
    SelectPasswordFallbackMetricsTest,
    ::testing::ValuesIn(std::vector<SelectPasswordFallbackMetricsTestParams>(
        {{
             .option_accepted = true,
             .is_field_unclassified = true,
             .test_name = "UnclassifiedField_Passwords_Accepted",
         },
         {
             .option_accepted = false,
             .is_field_unclassified = true,
             .test_name = "UnclassifiedField_Passwords_NotAccepted",
         },
         {
             .option_accepted = true,
             .is_field_unclassified = false,
             .test_name = "ClassifiedField_Passwords_Accepted",
         },
         {
             .option_accepted = false,
             .is_field_unclassified = false,
             .test_name = "ClassifiedField_Passwords_NotAccepted",
         }})),
    [](const ::testing::TestParamInfo<
        SelectPasswordFallbackMetricsTest::ParamType>& info) {
      return info.param.test_name;
    });

class PlusAddressContextMenuManagerTest
    : public SigninBrowserTestBaseT<BaseAutofillContextMenuManagerTest> {
 public:
  static constexpr char kExcludedDomainRegex[] = "muh\\.mah$";
  static constexpr char kExcludedDomainUrl[] = "https://muh.mah";
  static constexpr char kUserActionPlusAddressesFallbackSelected[] =
      "PlusAddresses.ManualFallbackDesktopContextManualFallbackSelected";

  PlusAddressContextMenuManagerTest() {
    // TODO(crbug.com/327562692): Create and use a `PlusAddressTestEnvironment`.
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{plus_addresses::features::kPlusAddressesEnabled,
          {{plus_addresses::features::kEnterprisePlusAddressServerUrl.name,
            "https://foo.bar"}}},
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

  base::UserActionTester user_action_tester_;

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
  EXPECT_EQ(user_action_tester_.GetActionCount(
                kUserActionPlusAddressesFallbackSelected),
            0);
}

// Tests that Plus Address fallbacks are added to classified forms.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest, ClassifiedForm) {
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id()));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), PlusAddressFallbackAdded());
  EXPECT_EQ(user_action_tester_.GetActionCount(
                kUserActionPlusAddressesFallbackSelected),
            0);
}

// Tests that Plus Address fallbacks are added when the context menu is
// triggered on a text area.
IN_PROC_BROWSER_TEST_F(PlusAddressContextMenuManagerTest,
                       TriggeredOnTextArea_ClassifiedForm) {
  FormData form = CreateAndAttachClassifiedForm();
  autofill_context_menu_manager()->set_params_for_testing(
      CreateContextMenuParams(form.renderer_id(),
                              form.fields()[0].renderer_id(),
                              blink::mojom::FormControlType::kTextArea));
  autofill_context_menu_manager()->AppendItems();

  EXPECT_THAT(menu_model(), PlusAddressFallbackAdded());
  EXPECT_EQ(user_action_tester_.GetActionCount(
                kUserActionPlusAddressesFallbackSelected),
            0);
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
  EXPECT_EQ(user_action_tester_.GetActionCount(
                kUserActionPlusAddressesFallbackSelected),
            0);
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
  EXPECT_EQ(user_action_tester_.GetActionCount(
                kUserActionPlusAddressesFallbackSelected),
            0);
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
  EXPECT_EQ(user_action_tester_.GetActionCount(
                kUserActionPlusAddressesFallbackSelected),
            0);
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
  EXPECT_EQ(user_action_tester_.GetActionCount(
                kUserActionPlusAddressesFallbackSelected),
            0);
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
  EXPECT_EQ(user_action_tester_.GetActionCount(
                kUserActionPlusAddressesFallbackSelected),
            0);
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
  EXPECT_EQ(user_action_tester_.GetActionCount(
                kUserActionPlusAddressesFallbackSelected),
            1);
}

}  // namespace
}  // namespace autofill
