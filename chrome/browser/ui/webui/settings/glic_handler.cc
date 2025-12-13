// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/glic_handler.h"

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace settings {

GlicHandler::GlicHandler() = default;
GlicHandler::~GlicHandler() = default;

void GlicHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setGlicOsLauncherEnabled",
      base::BindRepeating(&GlicHandler::HandleSetGlicOsLauncherEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getGlicShortcut",
      base::BindRepeating(&GlicHandler::HandleGetGlicShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setGlicShortcut",
      base::BindRepeating(&GlicHandler::HandleSetGlicShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getGlicFocusToggleShortcut",
      base::BindRepeating(&GlicHandler::HandleGetGlicFocusToggleShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setGlicFocusToggleShortcut",
      base::BindRepeating(&GlicHandler::HandleSetGlicFocusToggleShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setShortcutSuspensionState",
      base::BindRepeating(&GlicHandler::HandleSetShortcutSuspensionState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getGlicDisallowedByAdmin",
      base::BindRepeating(&GlicHandler::HandleGetGlicDisallowedByAdmin,
                          base::Unretained(this)));
}

void GlicHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (auto* service =
          glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile)) {
    // Unretained is safe here since our subscription will expire upon our
    // destruction.
    glic_enabling_subscription_ =
        std::make_unique<base::CallbackListSubscription>(
            service->enabling().RegisterAllowedChanged(base::BindRepeating(
                &GlicHandler::FireOnGlicDisallowedByAdminChanged,
                base::Unretained(this))));
  }

  if (auto* actor_service =
          actor::ActorKeyedServiceFactory::GetActorKeyedService(profile)) {
    web_actuation_subscription_ =
        actor_service->AddActOnWebCapabilityChangedCallback(
            base::BindRepeating(&GlicHandler::OnWebActuationCapabilityChanged,
                                base::Unretained(this)));
  }
}

void GlicHandler::OnJavascriptDisallowed() {
  glic_enabling_subscription_.reset();
  web_actuation_subscription_ = {};
}

void GlicHandler::SetWebUIForTesting(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

void GlicHandler::HandleSetGlicOsLauncherEnabled(
    const base::Value::List& args) {
  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      web_ui()->GetWebContents()->GetBrowserContext(), features::kGlic);
}

void GlicHandler::HandleGetGlicShortcut(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();
  ResolveJavascriptCallback(
      callback_id,
      base::UTF16ToUTF8(glic::GlicLauncherConfiguration::GetGlobalHotkey()
                            .GetShortcutText()));
}

void GlicHandler::HandleSetGlicShortcut(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string accelerator_string = args[1].GetString();
  g_browser_process->local_state()->SetString(glic::prefs::kGlicLauncherHotkey,
                                              accelerator_string);

  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      web_ui()->GetWebContents()->GetBrowserContext(),
      features::kGlicKeyboardShortcutNewBadge);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, base::Value());
}

void GlicHandler::HandleGetGlicFocusToggleShortcut(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();
  ResolveJavascriptCallback(
      callback_id,
      base::UTF16ToUTF8(glic::LocalHotkeyManager::GetConfigurableAccelerator(
                            glic::LocalHotkeyManager::Hotkey::kFocusToggle)
                            .GetShortcutText()));
}

void GlicHandler::HandleSetGlicFocusToggleShortcut(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string accelerator_string = args[1].GetString();
  g_browser_process->local_state()->SetString(
      glic::prefs::kGlicFocusToggleHotkey, accelerator_string);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, base::Value());
}

void GlicHandler::HandleSetShortcutSuspensionState(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const bool should_suspend = args[0].GetBool();
  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  // `global_accelerator_listener` may be null on Linux Wayland builds.
  if (global_accelerator_listener) {
    global_accelerator_listener->SetShortcutHandlingSuspended(should_suspend);
  }
}

void GlicHandler::HandleGetGlicDisallowedByAdmin(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();

  Profile* profile = Profile::FromWebUI(web_ui());
  const bool disallowed =
      glic::GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin();

  ResolveJavascriptCallback(callback_id, base::Value(disallowed));
}

void GlicHandler::FireOnGlicDisallowedByAdminChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
  const bool disallowed =
      glic::GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin();
  FireWebUIListener("glic-disallowed-by-admin-changed",
                    base::Value(disallowed));
}

void GlicHandler::OnWebActuationCapabilityChanged(bool can_act_on_web) {
  FireWebUIListener("glic-web-actuation-capability-changed",
                    base::Value(can_act_on_web));
}

}  // namespace settings
