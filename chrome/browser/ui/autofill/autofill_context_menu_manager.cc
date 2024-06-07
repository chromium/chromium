// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_feedback_data.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/address_save_metrics.h"
#include "components/autofill/core/browser/metrics/fallback_autocomplete_unrecognized_metrics.h"
#include "components/autofill/core/browser/metrics/manual_fallback_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/service/variations_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"

namespace autofill {

using FillingProductSet = DenseSet<FillingProduct>;

namespace {

using ::password_manager::ContentPasswordManagerDriver;

constexpr char kFeedbackPlaceholder[] =
    "What steps did you just take?\n"
    "(1)\n"
    "(2)\n"
    "(3)\n"
    "\n"
    "What was the expected result?\n"
    "\n"
    "What happened instead? (Please include the screenshot below)";

// Constant determining the icon size in the context menu.
constexpr int kContextMenuIconSize = 16;

bool ShouldShowAutofillContextMenu(const content::ContextMenuParams& params) {
  if (!params.form_control_type) {
    return false;
  }
  // Return true (only) on text fields.
  //
  // Note that this switch is over `blink::mojom::FormControlType`, not
  // `autofill::FormControlType`. Therefore, it does not handle
  // `autofill::FormControlType::kContentEditable`, which is covered by the
  // above if-condition `!params.form_control_type`.
  //
  // TODO(crbug.com/40285492): Unify with functions from form_autofill_util.cc.
  switch (*params.form_control_type) {
    case blink::mojom::FormControlType::kInputEmail:
    case blink::mojom::FormControlType::kInputMonth:
    case blink::mojom::FormControlType::kInputNumber:
    case blink::mojom::FormControlType::kInputPassword:
    case blink::mojom::FormControlType::kInputSearch:
    case blink::mojom::FormControlType::kInputTelephone:
    case blink::mojom::FormControlType::kInputText:
    case blink::mojom::FormControlType::kInputUrl:
    case blink::mojom::FormControlType::kTextArea:
      return true;
    case blink::mojom::FormControlType::kButtonButton:
    case blink::mojom::FormControlType::kButtonSubmit:
    case blink::mojom::FormControlType::kButtonReset:
    case blink::mojom::FormControlType::kButtonPopover:
    case blink::mojom::FormControlType::kButtonSelectList:
    case blink::mojom::FormControlType::kFieldset:
    case blink::mojom::FormControlType::kInputButton:
    case blink::mojom::FormControlType::kInputCheckbox:
    case blink::mojom::FormControlType::kInputColor:
    case blink::mojom::FormControlType::kInputDate:
    case blink::mojom::FormControlType::kInputDatetimeLocal:
    case blink::mojom::FormControlType::kInputFile:
    case blink::mojom::FormControlType::kInputHidden:
    case blink::mojom::FormControlType::kInputImage:
    case blink::mojom::FormControlType::kInputRadio:
    case blink::mojom::FormControlType::kInputRange:
    case blink::mojom::FormControlType::kInputReset:
    case blink::mojom::FormControlType::kInputSubmit:
    case blink::mojom::FormControlType::kInputTime:
    case blink::mojom::FormControlType::kInputWeek:
    case blink::mojom::FormControlType::kOutput:
    case blink::mojom::FormControlType::kSelectOne:
    case blink::mojom::FormControlType::kSelectMultiple:
    case blink::mojom::FormControlType::kSelectList:
      return false;
  }
  NOTREACHED_NORETURN();
}

// Returns true if the given id is one generated for autofill context menu.
bool IsAutofillCustomCommandId(
    AutofillContextMenuManager::CommandId command_id) {
  static constexpr auto kAutofillCommands = base::MakeFixedFlatSet<int>(
      {IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS,
       IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS,
       IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS,
       IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
       IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS,
       IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD,
       IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS,
       IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD});
  return kAutofillCommands.contains(command_id.value());
}

bool IsLikelyDogfoodClient() {
  auto* variations_service = g_browser_process->variations_service();
  if (!variations_service) {
    return false;
  }
  return variations_service->IsLikelyDogfoodClient();
}

// Returns true if the field is a username or password field.
bool IsPasswordFormField(ContentPasswordManagerDriver* password_manager_driver,
                         const content::ContextMenuParams& params) {
  const autofill::FieldRendererId current_field_renderer_id(
      params.field_renderer_id);
  // `PasswordManager::GetParsedObservedForm` returns true iff the current field
  // is part of a password form.
  return password_manager_driver &&
         password_manager_driver->GetPasswordManager()->GetParsedObservedForm(
             password_manager_driver, current_field_renderer_id);
}

// Returns true if the user has autofillable passwords saved.
bool UserHasPasswordsSaved(
    ContentPasswordManagerDriver& password_manager_driver) {
  password_manager::PasswordManagerClient* client =
      password_manager_driver.GetPasswordManager()->GetClient();
  return client->GetPrefs()->GetBoolean(
             password_manager::prefs::
                 kAutofillableCredentialsProfileStoreLoginDatabase) ||
         client->GetPrefs()->GetBoolean(
             password_manager::prefs::
                 kAutofillableCredentialsAccountStoreLoginDatabase);
}

base::Value::Dict LoadTriggerFormAndFieldLogs(
    AutofillManager& manager,
    const LocalFrameToken& frame_token,
    const content::ContextMenuParams& params) {
  if (!ShouldShowAutofillContextMenu(params)) {
    return base::Value::Dict();
  }

  FormGlobalId form_global_id = {frame_token,
                                 FormRendererId(params.form_renderer_id)};

  base::Value::Dict trigger_form_logs;
  if (FormStructure* form = manager.FindCachedFormById(form_global_id)) {
    trigger_form_logs.Set("triggerFormSignature", form->FormSignatureAsStr());

    if (params.form_control_type) {
      FieldGlobalId field_global_id = {
          frame_token, FieldRendererId(params.field_renderer_id)};
      auto field =
          base::ranges::find_if(*form, [&field_global_id](const auto& field) {
            return field->global_id() == field_global_id;
          });
      if (field != form->end()) {
        trigger_form_logs.Set("triggerFieldSignature",
                              (*field)->FieldSignatureAsStr());
      }
    }
  }
  return trigger_form_logs;
}

}  // namespace

AutofillContextMenuManager::AutofillContextMenuManager(
    PersonalDataManager* personal_data_manager,
    RenderViewContextMenuBase* delegate,
    ui::SimpleMenuModel* menu_model)
    : personal_data_manager_(personal_data_manager),
      menu_model_(menu_model),
      delegate_(delegate),
      passwords_submenu_model_(delegate) {
  DCHECK(delegate_);
  params_ = delegate_->params();
}

AutofillContextMenuManager::~AutofillContextMenuManager() = default;

void AutofillContextMenuManager::AppendItems() {
  MaybeAddAutofillFeedbackItem();
  MaybeAddAutofillManualFallbackItems();
}

bool AutofillContextMenuManager::IsCommandIdSupported(int command_id) {
  return IsAutofillCustomCommandId(CommandId(command_id));
}

bool AutofillContextMenuManager::IsCommandIdEnabled(int command_id) {
  return true;
}

void AutofillContextMenuManager::ExecuteCommand(int command_id) {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh) {
    return;
  }
  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!autofill_driver) {
    return;
  }
  CHECK(IsAutofillCustomCommandId(CommandId(command_id)));

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK) {
    ExecuteAutofillFeedbackCommand(autofill_driver->GetFrameToken(),
                                   autofill_driver->GetAutofillManager());
    return;
  }

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS) {
    ExecuteFallbackForAddressesCommand(*autofill_driver);
    return;
  }

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS) {
    ExecuteFallbackForPaymentsCommand(*autofill_driver);
    return;
  }

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS) {
    ExecuteFallbackForPlusAddressesCommand(*autofill_driver);
    return;
  }

  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD) {
    ExecuteFallbackForPasswordsCommand(*autofill_driver);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS) {
    // This function also records metrics.
    NavigateToManagePasswordsPage(
        chrome::FindBrowserWithTab(web_contents),
        password_manager::ManagePasswordsReferrer::kPasswordContextMenu);
    return;
  }

  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD) {
    // This function also records metrics.
    password_manager_util::UserTriggeredManualGenerationFromContextMenu(
        ChromePasswordManagerClient::FromWebContents(web_contents),
        autofill::ContentAutofillClient::FromWebContents(web_contents));
    return;
  }
}

void AutofillContextMenuManager::MaybeAddAutofillFeedbackItem() {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh) {
    return;
  }

  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  // Do not show autofill context menu options for input fields that cannot be
  // filled by the driver. See crbug.com/1367547.
  if (!autofill_driver || !autofill_driver->CanShowAutofillUi()) {
    return;
  }

  // Includes the option of submitting feedback on Autofill.
  if (static_cast<BrowserAutofillManager&>(
          autofill_driver->GetAutofillManager())
          .IsAutofillEnabled() &&
      IsLikelyDogfoodClient()) {
    menu_model_->AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
        IDS_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
        ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon));

    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

void AutofillContextMenuManager::MaybeAddAutofillManualFallbackItems() {
  if (!ShouldShowAutofillContextMenu(params_)) {
    // Autofill entries are only available in input or text area fields
    return;
  }

  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh) {
    return;
  }

  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  ContentPasswordManagerDriver* password_manager_driver =
      ContentPasswordManagerDriver::GetForRenderFrameHost(rfh);

  bool add_plus_address_fallback = false;
  bool add_address_fallback = false;
  bool add_payments_fallback = false;
  bool add_passwords_fallback = false;

  // Do not show autofill context menu options for input fields that cannot be
  // filled by the driver. See crbug.com/1367547.
  if (autofill_driver && autofill_driver->CanShowAutofillUi()) {
    add_plus_address_fallback =
        ShouldAddPlusAddressManualFallbackItem(*autofill_driver);
    add_address_fallback = ShouldAddAddressManualFallbackItem(*autofill_driver);
    add_payments_fallback =
        personal_data_manager_->payments_data_manager()
            .IsAutofillPaymentMethodsEnabled() &&
        !personal_data_manager_->payments_data_manager()
             .GetCreditCardsToSuggest()
             .empty() &&
        base::FeatureList::IsEnabled(
            features::kAutofillForUnclassifiedFieldsAvailable);
  }

  // Do not show password manager context menu options for input fields that
  // cannot be filled by the driver. See crbug.com/1367547.
  if (password_manager_driver && password_manager_driver->CanShowAutofillUi()) {
    add_passwords_fallback =
        ShouldAddPasswordsManualFallbackItem(*password_manager_driver);
  }

  if (!add_plus_address_fallback && !add_address_fallback &&
      !add_payments_fallback && !add_passwords_fallback) {
    return;
  }
  menu_model_->AddTitle(
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_TITLE));

  if (add_plus_address_fallback) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS,
        IDS_PLUS_ADDRESS_FALLBACK_LABEL_CONTEXT_MENU);
  }
  if (add_address_fallback) {
    menu_model_->AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS,
        ui::ImageModel::FromVectorIcon(
            vector_icons::kLocationOnChromeRefreshIcon, ui::kColorIcon,
            kContextMenuIconSize));
    menu_model_->SetIsNewFeatureAt(
        menu_model_->GetItemCount() - 1,
        UserEducationService::MaybeShowNewBadge(
            delegate_->GetBrowserContext(),
            features::kAutofillForUnclassifiedFieldsAvailable));
  }
  if (add_payments_fallback) {
    menu_model_->AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS,
        ui::ImageModel::FromVectorIcon(kCreditCardChromeRefreshIcon,
                                       ui::kColorIcon, kContextMenuIconSize));
    menu_model_->SetIsNewFeatureAt(
        menu_model_->GetItemCount() - 1,
        UserEducationService::MaybeShowNewBadge(
            delegate_->GetBrowserContext(),
            features::kAutofillForUnclassifiedFieldsAvailable));
  }
  if (add_passwords_fallback) {
    AddPasswordsManualFallbackItems(*password_manager_driver);
  }

  const bool select_passwords_option_shown =
      add_passwords_fallback && UserHasPasswordsSaved(*password_manager_driver);
  // TODO(b/327566698): Log metrics for plus address fallbacks, too.
  LogManualFallbackContextMenuEntryShown(
      autofill_driver, password_manager_driver, add_address_fallback,
      add_payments_fallback, select_passwords_option_shown);
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
}

bool AutofillContextMenuManager::ShouldAddPlusAddressManualFallbackItem(
    ContentAutofillDriver& autofill_driver) {
  if (params_.form_control_type &&
      params_.form_control_type.value() ==
          blink::mojom::FormControlType::kInputPassword) {
    return false;
  }

  auto* web_contents = content::WebContents::FromRenderFrameHost(
      autofill_driver.render_frame_host());
  const plus_addresses::PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  AutofillClient& client = autofill_driver.GetAutofillManager().client();
  return plus_address_service &&
         plus_address_service->SupportsPlusAddresses(
             client.GetLastCommittedPrimaryMainFrameOrigin(),
             client.IsOffTheRecord()) &&
         base::FeatureList::IsEnabled(
             plus_addresses::features::kPlusAddressFallbackFromContextMenu);
}

bool AutofillContextMenuManager::ShouldAddAddressManualFallbackItem(
    ContentAutofillDriver& autofill_driver) {
  if (!personal_data_manager_->address_data_manager()
           .IsAutofillProfileEnabled()) {
    return false;
  }

  // If the field is of address type and there is information in the profile to
  // fill it, we always show the fallback option.
  // TODO(crbug.com/40285811): Remove the following code block once feature is
  // cleaned up. At that point, we can only check whether a profile exists or if
  // the user is not in incognito mode. Whether the field can be filled will be
  // irrelevant.
  AutofillField* field = GetAutofillField(autofill_driver.GetAutofillManager(),
                                          autofill_driver.GetFrameToken());
  if (field && FieldTypeGroupToFormType(field->Type().group()) ==
                   FormType::kAddressForm) {
    // Show the context menu entry for address fields, which can be filled
    // with at least one of the user's profiles.
    CHECK(personal_data_manager_);
    if (base::ranges::any_of(
            personal_data_manager_->address_data_manager().GetProfiles(),
            [field](const AutofillProfile* profile) {
              return profile->HasInfo(field->Type().GetStorableType());
            })) {
      return true;
    }
  }

  // Also add the manual fallback option if:
  // 1. The user has a profile stored, or
  // 2. The user does not have a profile stored and is not in incognito mode.
  // This is done so that users can be prompted to create an address profile.
  const bool has_profile =
      !personal_data_manager_->address_data_manager().GetProfiles().empty();
  const bool is_incognito =
      autofill_driver.GetAutofillManager().client().IsOffTheRecord();
  return (has_profile || !is_incognito) &&
         base::FeatureList::IsEnabled(
             features::kAutofillForUnclassifiedFieldsAvailable);
}

bool AutofillContextMenuManager::ShouldAddPasswordsManualFallbackItem(
    ContentPasswordManagerDriver& password_manager_driver) {
  return password_manager_driver.GetPasswordManager()
             ->GetClient()
             ->IsFillingEnabled(
                 password_manager_driver.GetLastCommittedURL()) &&
         base::FeatureList::IsEnabled(
             password_manager::features::kPasswordManualFallbackAvailable);
}

void AutofillContextMenuManager::AddPasswordsManualFallbackItems(
    ContentPasswordManagerDriver& password_manager_driver) {
  int regular_password_entry_command_id;
  int regular_password_entry_string_id;
  const bool user_is_syncing =
      password_manager_util::ManualPasswordGenerationEnabled(
          &password_manager_driver);

  // TODO(b/321678141): Update strings once we have UX decision.
  if (UserHasPasswordsSaved(password_manager_driver)) {
    regular_password_entry_command_id =
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD;
    regular_password_entry_string_id =
        user_is_syncing
            ? IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD
            : IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS;
  } else {
    // If the user doesn't have passwords saved, display "Import passwords"
    // option.
    regular_password_entry_command_id =
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS;
    regular_password_entry_string_id =
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS;
  }

  if (user_is_syncing) {
    // If the user is syncing, create a passwords submenu. The submenu
    // contains the regular passwords manual fallback entry, plus an extra
    // entry for generating passwords.
    passwords_submenu_model_.AddItemWithStringId(
        regular_password_entry_command_id, regular_password_entry_string_id);

    passwords_submenu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD);

    menu_model_->AddSubMenuWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS,
        &passwords_submenu_model_);
  } else {
    // If the user is not syncing, add the regular passwords manual fallback
    // passwords entry.
    menu_model_->AddItemWithStringId(regular_password_entry_command_id,
                                     regular_password_entry_string_id);
  }
}

void AutofillContextMenuManager::LogManualFallbackContextMenuEntryShown(
    ContentAutofillDriver* autofill_driver,
    ContentPasswordManagerDriver* password_manager_driver,
    bool address_option_shown,
    bool payments_option_shown,
    bool select_passwords_option_shown) {
  if (!autofill_driver || (!address_option_shown && !payments_option_shown &&
                           !select_passwords_option_shown)) {
    return;
  }
  AutofillField* field = GetAutofillField(autofill_driver->GetAutofillManager(),
                                          autofill_driver->GetFrameToken());
  const bool address_option_shown_for_field_not_classified_as_address =
      address_option_shown &&
      !IsAddressType(field ? field->Type().GetStorableType() : UNKNOWN_TYPE);
  const bool payments_option_shown_for_field_not_classified_as_payments =
      payments_option_shown &&
      (!field || field->Type().group() != FieldTypeGroup::kCreditCard);
  CHECK(!select_passwords_option_shown || password_manager_driver)
      << "No password entries should be shown if there is no driver.";
  const bool
      select_passwords_option_shown_for_form_not_classified_as_password_form =
          select_passwords_option_shown &&
          !IsPasswordFormField(password_manager_driver, params_);

  if (address_option_shown &&
      !address_option_shown_for_field_not_classified_as_address) {
    // Only use AutocompleteUnrecognizedFallbackEventLogger if the address
    // option was shown on a field that WAS classified as an address.
    static_cast<BrowserAutofillManager&>(autofill_driver->GetAutofillManager())
        .GetAutocompleteUnrecognizedFallbackEventLogger()
        .ContextMenuEntryShown(
            /*address_field_has_ac_unrecognized=*/field
                ->ShouldSuppressSuggestionsAndFillingByDefault());
  }

  static_cast<BrowserAutofillManager&>(autofill_driver->GetAutofillManager())
      .GetManualFallbackEventLogger()
      .ContextMenuEntryShown(
          address_option_shown_for_field_not_classified_as_address,
          payments_option_shown_for_field_not_classified_as_payments,
          select_passwords_option_shown_for_form_not_classified_as_password_form);
}

void AutofillContextMenuManager::LogManualFallbackContextMenuEntryAccepted(
    AutofillDriver& autofill_driver,
    FillingProduct filling_product) {
  BrowserAutofillManager& manager = static_cast<BrowserAutofillManager&>(
      autofill_driver.GetAutofillManager());
  AutofillField* field =
      GetAutofillField(manager, autofill_driver.GetFrameToken());

  switch (filling_product) {
    case FillingProduct::kAddress: {
      const bool is_address_field =
          field && IsAddressType(field->Type().GetStorableType());
      if (is_address_field) {
        // Address manual fallback was triggered from a classified address
        // field.
        manager.GetAutocompleteUnrecognizedFallbackEventLogger()
            .ContextMenuEntryAccepted(
                /*address_field_has_ac_unrecognized=*/field
                    ->ShouldSuppressSuggestionsAndFillingByDefault());
      } else {
        manager.GetManualFallbackEventLogger().ContextMenuEntryAccepted(
            filling_product);
      }
      break;
    }
    case FillingProduct::kCreditCard:
    case FillingProduct::kStandaloneCvc:
      if (!field ||
          !FieldTypeGroupSet::is_one_of(
              field->Type().group(), {FieldTypeGroup::kCreditCard,
                                      FieldTypeGroup::kStandaloneCvcField})) {
        // Only log payments manual fallback when triggered from a field that is
        // not classified as payments.
        manager.GetManualFallbackEventLogger().ContextMenuEntryAccepted(
            filling_product);
      }
      break;
    case FillingProduct::kPassword: {
      content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
      ContentPasswordManagerDriver* password_manager_driver =
          rfh ? ContentPasswordManagerDriver::GetForRenderFrameHost(rfh)
              : nullptr;

      // TODO(b/321678141): Handle the "else" case of this if.
      if (!IsPasswordFormField(password_manager_driver, params_)) {
        manager.GetManualFallbackEventLogger().ContextMenuEntryAccepted(
            filling_product);
      }
      break;
    }
    // TODO(b/327566698): Add metrics for plus addresses.
    case FillingProduct::kPlusAddresses:
      NOTIMPLEMENTED();
      break;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kCompose:
      NOTREACHED_IN_MIGRATION();
  }
}

void AutofillContextMenuManager::ExecuteAutofillFeedbackCommand(
    const LocalFrameToken& frame_token,
    AutofillManager& manager) {
  // The cast is safe since the context menu is only available on Desktop.
  auto& client = static_cast<ContentAutofillClient&>(manager.client());
  Browser* browser = chrome::FindBrowserWithTab(&client.GetWebContents());
  chrome::ShowFeedbackPage(
      browser, feedback::kFeedbackSourceAutofillContextMenu,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/kFeedbackPlaceholder,
      /*category_tag=*/"dogfood_autofill_feedback",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/
      data_logs::FetchAutofillFeedbackData(
          &manager,
          LoadTriggerFormAndFieldLogs(manager, frame_token, params_)));
}

void AutofillContextMenuManager::ExecuteFallbackForPlusAddressesCommand(
    AutofillDriver& autofill_driver) {
  autofill_driver.RendererShouldTriggerSuggestions(
      /*field_id=*/{autofill_driver.GetFrameToken(),
                    FieldRendererId(params_.field_renderer_id)},
      AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses);
  LogManualFallbackContextMenuEntryAccepted(autofill_driver,
                                            FillingProduct::kPlusAddresses);
}

void AutofillContextMenuManager::ExecuteFallbackForPaymentsCommand(
    AutofillDriver& autofill_driver) {
  autofill_driver.RendererShouldTriggerSuggestions(
      /*field_id=*/{autofill_driver.GetFrameToken(),
                    FieldRendererId(params_.field_renderer_id)},
      AutofillSuggestionTriggerSource::kManualFallbackPayments);
  LogManualFallbackContextMenuEntryAccepted(autofill_driver,
                                            FillingProduct::kCreditCard);
  UserEducationService::MaybeNotifyPromoFeatureUsed(
      delegate_->GetBrowserContext(),
      features::kAutofillForUnclassifiedFieldsAvailable);
}

void AutofillContextMenuManager::ExecuteFallbackForPasswordsCommand(
    AutofillDriver& autofill_driver) {
  autofill_driver.RendererShouldTriggerSuggestions(
      /*field_id=*/{autofill_driver.GetFrameToken(),
                    FieldRendererId(params_.field_renderer_id)},
      AutofillSuggestionTriggerSource::kManualFallbackPasswords);
  LogManualFallbackContextMenuEntryAccepted(autofill_driver,
                                            FillingProduct::kPassword);
}

void AutofillContextMenuManager::ExecuteFallbackForAddressesCommand(
    ContentAutofillDriver& autofill_driver) {
  AutofillManager& manager = autofill_driver.GetAutofillManager();
  AutofillField* field =
      GetAutofillField(manager, autofill_driver.GetFrameToken());
  if (!field && !base::FeatureList::IsEnabled(
                    features::kAutofillForUnclassifiedFieldsAvailable)) {
    // The field should generally exist, since the fallback option is only shown
    // when the field can be retrieved. But if the website removed the field
    // before the entry was select, it might not be available anymore.
    //
    // Note that, when `features::kAutofillForUnclassifiedFieldsAvailable` is
    // enabled Autofill is always available, regardless of whether
    // `AutofillField` exists or not.
    return;
  }

  if (personal_data_manager_->address_data_manager().GetProfiles().empty() &&
      base::FeatureList::IsEnabled(
          features::kAutofillForUnclassifiedFieldsAvailable)) {
    content::RenderFrameHost* rfh = autofill_driver.render_frame_host();
    auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
    AddressBubblesController::SetUpAndShowAddNewAddressBubble(
        web_contents,
        base::BindOnce(
            [](AddressDataManager* adm,
               content::GlobalRenderFrameHostId frame_id,
               uint64_t field_renderer_id,
               AutofillClient::AddressPromptUserDecision decision,
               base::optional_ref<const AutofillProfile> profile) {
              bool new_address_saved =
                  decision ==
                  AutofillClient::AddressPromptUserDecision::kEditAccepted;
              if (new_address_saved && profile.has_value()) {
                adm->AddChangeCallback(base::BindOnce(
                    [](content::GlobalRenderFrameHostId frame_id,
                       uint64_t field_renderer_id) {
                      content::RenderFrameHost* rfh =
                          content::RenderFrameHost::FromID(frame_id);
                      if (!rfh) {
                        return;
                      }
                      AutofillDriver* driver =
                          ContentAutofillDriver::GetForRenderFrameHost(rfh);
                      if (!driver) {
                        return;
                      }

                      driver->RendererShouldTriggerSuggestions(
                          /*field_id=*/{driver->GetFrameToken(),
                                        FieldRendererId(field_renderer_id)},
                          AutofillSuggestionTriggerSource::
                              kManualFallbackAddress);
                    },
                    frame_id, field_renderer_id));
                adm->AddProfile(*profile);
              }

              LogAddNewAddressPromptOutcome(
                  new_address_saved
                      ? autofill_metrics::AutofillAddNewAddressPromptOutcome::
                            kSaved
                      : autofill_metrics::AutofillAddNewAddressPromptOutcome::
                            kCanceled);

              if (new_address_saved) {
                autofill_metrics::LogManuallyAddedAddress(
                    autofill_metrics::AutofillManuallyAddedAddressSurface::
                        kContextMenuPrompt);
              }
            },
            // `PersonalDataManager`, as a keyed service, will always outlive
            // the bubble, which is bound to a tab.
            &personal_data_manager_->address_data_manager(), rfh->GetGlobalId(),
            params_.field_renderer_id));
  } else {
    autofill_driver.browser_events().RendererShouldTriggerSuggestions(
        /*field_id=*/{autofill_driver.GetFrameToken(),
                      FieldRendererId(params_.field_renderer_id)},
        AutofillSuggestionTriggerSource::kManualFallbackAddress);
  }
  LogManualFallbackContextMenuEntryAccepted(autofill_driver,
                                            FillingProduct::kAddress);
  UserEducationService::MaybeNotifyPromoFeatureUsed(
      delegate_->GetBrowserContext(),
      features::kAutofillForUnclassifiedFieldsAvailable);
}

AutofillField* AutofillContextMenuManager::GetAutofillField(
    AutofillManager& manager,
    const LocalFrameToken& frame_token) const {
  CHECK(ShouldShowAutofillContextMenu(params_));
  FormStructure* form = manager.FindCachedFormById(
      {frame_token, FormRendererId(params_.form_renderer_id)});
  return form ? form->GetFieldById(
                    {frame_token, FieldRendererId(params_.field_renderer_id)})
              : nullptr;
}

}  // namespace autofill
