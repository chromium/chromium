// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include <string>

#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/user_education/scoped_new_badge_tracker.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_feedback_data.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/fallback_autocomplete_unrecognized_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/variations/service/variations_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"

namespace autofill {

namespace {

constexpr char kFeedbackPlaceholder[] =
    "What steps did you just take?\n"
    "(1)\n"
    "(2)\n"
    "(3)\n"
    "\n"
    "What was the expected result?\n"
    "\n"
    "What happened instead? (Please include the screenshot below)";

bool ShouldShowAutofillContextMenu(const content::ContextMenuParams& params) {
  if (!params.form_control_type) {
    return false;
  }
  // Return true on text fields.
  // TODO(crbug.com/1492339): Unify with functions from form_autofill_util.cc.
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
    default:
      return false;
  }
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

bool IsLikelyDogfoodClient() {
  auto* variations_service = g_browser_process->variations_service();
  if (!variations_service) {
    return false;
  }
  return variations_service->IsLikelyDogfoodClient();
}

}  // namespace

// static
bool AutofillContextMenuManager::IsAutofillCustomCommandId(
    CommandId command_id) {
  const int id = command_id.value();
  return id == IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK ||
         id == IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS ||
         id == IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS;
}

AutofillContextMenuManager::AutofillContextMenuManager(
    PersonalDataManager* personal_data_manager,
    RenderViewContextMenuBase* delegate,
    ui::SimpleMenuModel* menu_model)
    : personal_data_manager_(personal_data_manager),
      menu_model_(menu_model),
      delegate_(delegate) {
  DCHECK(delegate_);
  params_ = delegate_->params();
}

AutofillContextMenuManager::~AutofillContextMenuManager() = default;

void AutofillContextMenuManager::AppendItems() {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh)
    return;

  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  // Do not show autofill context menu options for input fields that cannot be
  // filled by the driver. See crbug.com/1367547.
  if (!driver || !driver->CanShowAutofillUi())
    return;

  if (ShouldShowAutofillContextMenu(params_)) {
    const LocalFrameToken frame_token = driver->GetFrameToken();
    // Formless fields have default form renderer id.
    driver->OnContextMenuShownInField(
        {frame_token, FormRendererId(params_.form_renderer_id)},
        {frame_token, FieldRendererId(params_.field_renderer_id)});
  }

  // Includes the option of submitting feedback on Autofill.
  if (personal_data_manager_->IsAutofillEnabled() && IsLikelyDogfoodClient()) {
    menu_model_->AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
        IDS_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
        ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon));

    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  MaybeAddAutofillManualFallbackItems(*driver);
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
  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!driver) {
    return;
  }
  AutofillManager& manager = driver->GetAutofillManager();

  CHECK(IsAutofillCustomCommandId(CommandId(command_id)));

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK) {
    ExecuteAutofillFeedbackCommand(driver->GetFrameToken(), manager);
    return;
  }

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS) {
    ExecuteFallbackForAddressesCommand(manager);
    return;
  }

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS) {
    ExecuteFallbackForPaymentsCommand(manager);
    return;
  }
}

void AutofillContextMenuManager::ExecuteAutofillFeedbackCommand(
    const LocalFrameToken& frame_token,
    AutofillManager& manager) {
  // The cast is safe since the context menu is only available on Desktop.
  auto& client = static_cast<ContentAutofillClient&>(manager.client());
  Browser* browser = chrome::FindBrowserWithTab(&client.GetWebContents());
  chrome::ShowFeedbackPage(
      browser, chrome::kFeedbackSourceAutofillContextMenu,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/kFeedbackPlaceholder,
      /*category_tag=*/"dogfood_autofill_feedback",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/
      data_logs::FetchAutofillFeedbackData(
          &manager,
          LoadTriggerFormAndFieldLogs(manager, frame_token, params_)));
}

void AutofillContextMenuManager::ExecuteFallbackForAddressesCommand(
    AutofillManager& manager) {
  auto& driver = static_cast<ContentAutofillDriver&>(manager.driver());
  if (!ShouldAddAddressManualFallbackForAutocompleteUnrecognized(driver)) {
    // Do nothing if the target field is not on address form field with
    // unrecognized autocomplete attribute fillable with available data.
    // TODO(crbug.com/1493361): Render suggestions for unclassified fields.
    return;
  }
  AutofillField* field = GetAutofillField(manager, driver.GetFrameToken());
  if (!field) {
    // The field should generally exist, since the fallback option is only shown
    // when the field can be retrieved. But if the website removed the field
    // before the entry was select, it might not be available anymore.
    return;
  }
  driver.browser_events().RendererShouldTriggerSuggestions(
      field->global_id(),
      AutofillSuggestionTriggerSource::kManualFallbackAddress);
  static_cast<BrowserAutofillManager&>(manager)
      .GetAutocompleteUnrecognizedFallbackEventLogger()
      .ContextMenuEntryAccepted(
          /*address_field_has_ac_unrecognized=*/field
              ->ShouldSuppressSuggestionsAndFillingByDefault());
}

void AutofillContextMenuManager::ExecuteFallbackForPaymentsCommand(
    AutofillManager& manager) {
  auto& driver = static_cast<ContentAutofillDriver&>(manager.driver());
  driver.browser_events().RendererShouldTriggerSuggestions(
      FieldGlobalId(driver.GetFrameToken(),
                    FieldRendererId(params_.field_renderer_id)),
      AutofillSuggestionTriggerSource::kManualFallbackPayments);
}

void AutofillContextMenuManager::MaybeAddAutofillManualFallbackItems(
    ContentAutofillDriver& driver) {
  if (!ShouldShowAutofillContextMenu(params_)) {
    // Autofill entries are only available in input or text area fields
    return;
  }
  const bool add_address_fallback = ShouldAddAddressManualFallbackItem(driver);
  const bool add_payments_fallback =
      !personal_data_manager_->GetCreditCardsToSuggest().empty() &&
      base::FeatureList::IsEnabled(
          features::kAutofillForUnclassifiedFieldsAvailable);

  if (!add_address_fallback && !add_payments_fallback) {
    return;
  }
  menu_model_->AddTitle(
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_TITLE));

  if (add_address_fallback) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS);
    LogManualFallbackContextMenuEntryShown(driver);
  }
  if (add_payments_fallback) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS);
  }
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
}

bool AutofillContextMenuManager::ShouldAddAddressManualFallbackItem(
    ContentAutofillDriver& driver) {
  if (personal_data_manager_->GetProfilesToSuggest().empty()) {
    return false;
  }

  return ShouldAddAddressManualFallbackForAutocompleteUnrecognized(driver) ||
         base::FeatureList::IsEnabled(
             features::kAutofillForUnclassifiedFieldsAvailable);
}

bool AutofillContextMenuManager::
    ShouldAddAddressManualFallbackForAutocompleteUnrecognized(
        ContentAutofillDriver& driver) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillFallbackForAutocompleteUnrecognized)) {
    return false;
  }

  AutofillField* field =
      GetAutofillField(driver.GetAutofillManager(), driver.GetFrameToken());

  if (!field || FieldTypeGroupToFormType(field->Type().group()) !=
                    FormType::kAddressForm) {
    return false;
  }

  // Only show the context menu entry for address fields, which can be filled
  // with at least one of the user's profiles.
  CHECK(personal_data_manager_);
  if (base::ranges::none_of(personal_data_manager_->GetProfiles(),
                            [field](AutofillProfile* profile) {
                              return profile->HasInfo(
                                  field->Type().GetStorableType());
                            })) {
    return false;
  }

  // Depending on the Finch parameter, only show the context menu entry for
  // ac=unrecognized fields.
  return field->ShouldSuppressSuggestionsAndFillingByDefault() ||
         features::kAutofillFallForAutocompleteUnrecognizedOnAllAddressField
             .Get();
}

void AutofillContextMenuManager::LogManualFallbackContextMenuEntryShown(
    ContentAutofillDriver& driver) {
  AutofillField* field =
      GetAutofillField(driver.GetAutofillManager(), driver.GetFrameToken());
  if (!field) {
    // `field` can be null when the user clicks on the correct input form field,
    // which is not extracted by the BrowserAutofillManager.
    return;
  }

  static_cast<BrowserAutofillManager&>(driver.GetAutofillManager())
      .GetAutocompleteUnrecognizedFallbackEventLogger()
      .ContextMenuEntryShown(
          /*address_field_has_ac_unrecognized=*/field
              ->ShouldSuppressSuggestionsAndFillingByDefault());
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
