// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include <algorithm>
#include <string>

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_context_menu_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webauthn/context_menu_helper.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_feedback_data.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_manual_fallback_metrics_recorder.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/variations/service/variations_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/menus/simple_menu_model.h"
#include "url/origin.h"

namespace autofill {

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

// Returns true if the given id is one generated for autofill context menu.
bool IsAutofillCustomCommandId(
    AutofillContextMenuManager::CommandId command_id) {
  static constexpr auto kAutofillCommands = base::MakeFixedFlatSet<int>({
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY,
      IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE,
  });
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
bool IsPasswordFormField(ContentPasswordManagerDriver& password_manager_driver,
                         const content::ContextMenuParams& params) {
  const FieldRendererId current_field_renderer_id(
      params.field_renderer_id.value());
  return password_manager_driver.GetPasswordManager()
      ->GetPasswordFormCache()
      ->GetPasswordForm(&password_manager_driver, current_field_renderer_id);
}

base::DictValue LoadTriggerFormAndFieldLogs(
    AutofillManager& manager,
    const LocalFrameToken& frame_token,
    const content::ContextMenuParams& params) {
  if (!ShouldShowAutofillContextMenu(params)) {
    return base::DictValue();
  }

  FormGlobalId form_global_id = {
      frame_token, FormRendererId(params.form_renderer_id.value())};

  base::DictValue trigger_form_logs;
  if (const FormStructure* form = manager.FindCachedFormById(form_global_id)) {
    trigger_form_logs.Set("triggerFormSignature", form->FormSignatureAsStr());

    if (params.form_control_type) {
      FieldGlobalId field_global_id = {
          frame_token, FieldRendererId(params.field_renderer_id.value())};
      if (const AutofillField* field = form->GetFieldById(field_global_id)) {
        trigger_form_logs.Set("triggerFieldSignature",
                              field->FieldSignatureAsStr());
      }
    }
  }
  return trigger_form_logs;
}

}  // namespace

AutofillContextMenuManager::AutofillContextMenuManager(
    RenderViewContextMenuBase* delegate,
    ui::SimpleMenuModel* menu_model)
    : menu_model_(menu_model), delegate_(delegate) {
  DCHECK(delegate_);
  params_ = delegate_->params();
}

AutofillContextMenuManager::~AutofillContextMenuManager() = default;

void AutofillContextMenuManager::AppendItems() {
  if (params_.is_content_editable_for_autofill) {
    if (MaybeAddAtMemoryItem()) {
      menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    }
    return;
  }

  MaybeAddAutofillManualFallbackItems();
  MaybeAddAutofillFeedbackItem();
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
  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE) {
    webauthn::OnPasskeyFromAnotherDeviceContextMenuItemSelected(rfh);
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

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY) {
    ExecuteAtMemoryContextMenuCommand(*rfh, params_);
    return;
  }

  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD) {
    ExecuteFallbackForSelectPasswordCommand(*autofill_driver);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS) {
    // This function also records metrics.
    NavigateToManagePasswordsPage(
        GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
            web_contents),
        password_manager::ManagePasswordsReferrer::kPasswordContextMenu);
    return;
  }

  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD) {
    // This function also records metrics.
    password_manager_util::UserTriggeredManualGenerationFromContextMenu(
        ChromePasswordManagerClient::FromWebContents(web_contents),
        ContentAutofillClient::FromWebContents(web_contents));
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
  // filled by the driver. See crbug.com/40061116.
  if (!autofill_driver || !autofill_driver->CanShowAutofillUi()) {
    return;
  }

  // Includes the option of submitting feedback on Autofill.
  if (autofill_driver->GetAutofillClient().IsAutofillEnabled() &&
      IsLikelyDogfoodClient()) {
    menu_model_->AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
        IDS_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
        ui::ImageModel::FromVectorIcon(::features::IsRoundedIconsEnabled()
                                           ? vector_icons::kPetsIcon
                                           : vector_icons::kDogfoodOldIcon,
                                       ui::kColorIcon, kContextMenuIconSize));

    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

bool AutofillContextMenuManager::MaybeAddAtMemoryItem() {
  content::RenderFrameHost* const rfh = delegate_->GetRenderFrameHost();
  if (!rfh || !ShouldShowAtMemoryContextMenuItem(*rfh, params_)) {
    return false;
  }

  menu_model_->AddItemWithStringId(
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY,
      IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY);
  return true;
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

  // Do not show password manager context menu options for input fields that
  // cannot be filled by the driver. See crbug.com/40061116.
  const bool add_passwords_fallback =
      ShouldAddPasswordsManualFallbackItem(*rfh, params_);

  if (add_passwords_fallback) {
    ContentPasswordManagerDriver& password_manager_driver =
        CHECK_DEREF(ContentPasswordManagerDriver::GetForRenderFrameHost(rfh));
    const bool select_passwords_option_shown =
        ShouldShowSelectPasswordContextMenuItem(*rfh, params_);
    AddPasswordsManualFallbackItems(password_manager_driver,
                                    select_passwords_option_shown);

    if (select_passwords_option_shown) {
      LogSelectPasswordManualFallbackContextMenuEntryShown(
          password_manager_driver);
    }
  }
  const bool add_at_memory_fallback = MaybeAddAtMemoryItem();

  if (add_passwords_fallback || add_at_memory_fallback) {
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

void AutofillContextMenuManager::AddPasswordsManualFallbackItems(
    ContentPasswordManagerDriver& password_manager_driver,
    bool add_select_password_option) {
  const bool add_password_generation_option =
      password_manager_util::ManualPasswordGenerationEnabled(
          &password_manager_driver) &&
      password_manager_driver.IsPasswordFieldForPasswordManager(
          FieldRendererId(params_.field_renderer_id.value()),
          params_.form_control_type);
  const bool add_passkey_from_another_device_option =
      webauthn::IsPasskeyFromAnotherDeviceContextMenuEnabled(
          delegate_->GetRenderFrameHost(), params_.form_renderer_id.value(),
          params_.field_renderer_id.value());
  const bool add_import_passwords_option = !add_select_password_option;

  if (add_select_password_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD);

    if (::features::IsMenuSimplificationEnabled()) {
      menu_model_->SetIconForCommandId(
          IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD,
          ui::ImageModel::FromVectorIcon(
              ::features::IsRoundedIconsEnabled()
                  ? vector_icons::kPasswordManagerIcon
                  : vector_icons::kPasswordManagerOldIcon,
              ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize));
    }
  }
  if (add_password_generation_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD);
  }
  if (add_passkey_from_another_device_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE);
  }
  if (add_import_passwords_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS);
  }
}

void AutofillContextMenuManager::
    LogSelectPasswordManualFallbackContextMenuEntryShown(
        ContentPasswordManagerDriver& password_manager_driver) {
  password_manager_driver.GetPasswordAutofillManager()
      ->GetPasswordManualFallbackMetricsRecorder()
      .ContextMenuEntryShown(
          /*classified_as_target_filling_password=*/
          IsPasswordFormField(password_manager_driver, params_));
}

void AutofillContextMenuManager::
    LogSelectPasswordManualFallbackContextMenuEntryAccepted() {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  ContentPasswordManagerDriver* password_manager_driver =
      rfh ? ContentPasswordManagerDriver::GetForRenderFrameHost(rfh) : nullptr;

  if (password_manager_driver) {
    password_manager_driver->GetPasswordAutofillManager()
        ->GetPasswordManualFallbackMetricsRecorder()
        .ContextMenuEntryAccepted(/*classified_as_target_filling_password=*/
                                  IsPasswordFormField(*password_manager_driver,
                                                      params_));
  }
}

void AutofillContextMenuManager::ExecuteAutofillFeedbackCommand(
    const LocalFrameToken& frame_token,
    AutofillManager& manager) {
  // The cast is safe since the context menu is only available on Desktop.
  auto& client = static_cast<ContentAutofillClient&>(manager.client());
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          &client.GetWebContents());
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

void AutofillContextMenuManager::ExecuteFallbackForSelectPasswordCommand(
    AutofillDriver& autofill_driver) {
  autofill_driver.RendererShouldTriggerSuggestions(
      /*field_id=*/{autofill_driver.GetFrameToken(),
                    FieldRendererId(params_.field_renderer_id.value())},
      AutofillSuggestionTriggerSource::kManualFallbackPasswords);

  LogSelectPasswordManualFallbackContextMenuEntryAccepted();
}

void AutofillContextMenuManager::MaybeMarkLastItemAsNewFeature(
    const base::Feature& feature) {
  menu_model_->SetIsNewFeatureAt(menu_model_->GetItemCount() - 1,
                                 UserEducationService::MaybeShowNewBadge(
                                     delegate_->GetBrowserContext(), feature));
}

}  // namespace autofill
