// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/default_search_extension_controlled_controller.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"
#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/extensions/extensions_overrides/simple_overrides.h"
#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"

DEFINE_USER_DATA(DefaultSearchExtensionControlledController);

DefaultSearchExtensionControlledController::
    DefaultSearchExtensionControlledController(
        BrowserWindowInterface& browser_window_interface,
        Profile& profile)
    : scoped_unowned_user_data_(
          browser_window_interface.GetUnownedUserDataHost(),
          *this),
      browser_window_interface_(browser_window_interface),
      profile_(profile) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kSearchEngineExplicitChoiceDialog));
}

DefaultSearchExtensionControlledController::
    ~DefaultSearchExtensionControlledController() = default;

// static
DefaultSearchExtensionControlledController*
DefaultSearchExtensionControlledController::From(
    BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

bool DefaultSearchExtensionControlledController::
    ShouldRequestConfirmationForExtensionDse(const GURL& url) const {
  // 1) DSE must be extension-controlled.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(base::to_address(profile_));
  if (!template_url_service) {
    return false;
  }

  const TemplateURL* dse = template_url_service->GetDefaultSearchProvider();
  if (!dse || dse->type() != TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) {
    return false;
  }

  // 2) The navigation URL must actually be a search URL for this DSE.
  if (!dse->IsSearchURL(url, template_url_service->search_terms_data())) {
    return false;
  }

  // 3) Look up the controlling extension.
  const std::string extension_id = dse->GetExtensionId();
  if (extension_id.empty()) {
    return false;
  }

  auto* registry =
      extensions::ExtensionRegistry::Get(base::to_address(profile_));
  if (!registry) {
    return false;
  }

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    return false;
  }

  // 4) Don't show for force-installed extensions that must remain enabled.
  auto* extension_system =
      extensions::ExtensionSystem::Get(base::to_address(profile_));
  if (!extension_system) {
    return false;
  }

  if (extension_system->management_policy()->MustRemainEnabled(
          extension, /*error=*/nullptr)) {
    return false;
  }

  // 5) Don't show if the user has already seen or acknowledged the dialog.
  if (ExtensionSettingsOverriddenDialog::HasShownFor(*profile_,
                                                     extension->id())) {
    return false;
  }

  // 6) Don't show if the user has already acknowledged the dialog.
  if (ExtensionSettingsOverriddenDialog::HasAcknowledgedExtension(
          *profile_, extension->id(),
          ControlledHomeDialogController::kAcknowledgedPreference)) {
    return false;
  }

  // TODO(crbug.com/463712739): Remove this check to show the Dialog for all
  // extensions.
  //
  // 7) Don't show for "simple override" extensions.
  if (simple_overrides::IsSimpleOverrideExtension(*extension)) {
    return ExtensionSettingsOverriddenDialog::
        ShouldShowForSimpleOverrideExtension(*profile_, *extension);
  }

  // If we reach here, we should show the confirmation.
  return true;
}

void DefaultSearchExtensionControlledController::ShowConfirmationDialog(
    content::WebContents& web_contents,
    ConfirmationCallback callback) {
  confirmation_callback_ = std::move(callback);

  settings_overridden_params::GetSearchOverriddenParamsThenRun(
      &web_contents,
      base::BindOnce(
          &DefaultSearchExtensionControlledController::OnParamsLoaded,
          weak_factory_.GetWeakPtr()));
}

void DefaultSearchExtensionControlledController::OnParamsLoaded(
    std::unique_ptr<ExtensionSettingsOverriddenDialog::Params> params) {
  if (!params) {
    DialogResolved(SettingsOverriddenDialogController::DialogResult::
                       kDialogClosedWithoutUserAction);
    return;
  }

  auto dialog = std::make_unique<ExtensionSettingsOverriddenDialog>(
      std::move(*params), *profile_);
  CHECK(dialog->ShouldShow());

  dialog->SetDialogResultCallback(base::BindOnce(
      &DefaultSearchExtensionControlledController::DialogResolved,
      weak_factory_.GetWeakPtr()));

  content::WebContents* web_contents =
      browser_window_interface_->GetActiveTabInterface()->GetContents();
  CHECK(web_contents);

  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();
  extensions::ShowSettingsOverriddenDialog(std::move(dialog), parent_window);
}

void DefaultSearchExtensionControlledController::DialogResolved(
    SettingsOverriddenDialogController::DialogResult dialog_result) {
  CHECK(confirmation_callback_);
  std::move(confirmation_callback_).Run(dialog_result);
}
