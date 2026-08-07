// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/default_search_extension_controlled_controller.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
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

namespace {

base::WeakPtr<DefaultSearchExtensionControlledController>&
GetDialogCurrentlyShowingRef() {
  static base::NoDestructor<
      base::WeakPtr<DefaultSearchExtensionControlledController>>
      g_currently_showing;
  return *g_currently_showing;
}

}  // namespace

DEFINE_USER_DATA(DefaultSearchExtensionControlledController);

// static
base::WeakPtr<DefaultSearchExtensionControlledController>
DefaultSearchExtensionControlledController::GetDialogCurrentlyShowing() {
  return GetDialogCurrentlyShowingRef();
}

// static
void DefaultSearchExtensionControlledController::SetDialogCurrentlyShowing(
    base::WeakPtr<DefaultSearchExtensionControlledController> controller) {
  GetDialogCurrentlyShowingRef() = std::move(controller);
}

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
    ~DefaultSearchExtensionControlledController() {
  if (GetDialogCurrentlyShowing().get() == this) {
    SetDialogCurrentlyShowing(nullptr);
  }
}

// static
DefaultSearchExtensionControlledController*
DefaultSearchExtensionControlledController::From(
    BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

bool DefaultSearchExtensionControlledController::
    ShouldRequestConfirmationForExtensionDse(const GURL& url) const {
  if (GetDialogCurrentlyShowing()) {
    return false;
  }

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

  // 7) Don't show if the extension set the same search engine the user was
  // already using. Nothing was actually overridden, so there is nothing to
  // confirm. See https://crbug.com/540532980.
  if (settings_overridden_params::ExtensionSearchOverrideMatchesExistingEngine(
          base::to_address(profile_))) {
    return false;
  }

  // TODO(crbug.com/463712739): Remove this check to show the Dialog for all
  // extensions.
  //
  // 8) Don't show for "simple override" extensions.
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
  SetDialogCurrentlyShowing(weak_factory_.GetWeakPtr());

  settings_overridden_params::GetSearchOverriddenParamsThenRun(
      &web_contents,
      base::BindOnce(
          &DefaultSearchExtensionControlledController::OnParamsLoaded,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DefaultSearchExtensionControlledController::OnParamsLoaded(
    ConfirmationCallback callback,
    std::unique_ptr<ExtensionSettingsOverriddenDialog::Params> params) {
  confirmation_callback_ = std::move(callback);
  if (!params) {
    // Confirmation turned out to be unnecessary, or the extension state
    // changed while the parameters were loading. No dialog was shown. See
    // https://crbug.com/540532980.
    DialogResolved(std::nullopt);
    return;
  }

  auto dialog = std::make_unique<ExtensionSettingsOverriddenDialog>(
      std::move(*params), *profile_);
  CHECK(dialog->ShouldShow());

  // A dialog is being shown, so every outcome from here on is a user decision.
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
    std::optional<SettingsOverriddenDialogController::DialogResult>
        dialog_result) {
  if (GetDialogCurrentlyShowing().get() == this) {
    SetDialogCurrentlyShowing(nullptr);
  }
  CHECK(confirmation_callback_);
  std::move(confirmation_callback_).Run(dialog_result);
}
