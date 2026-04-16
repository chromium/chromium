// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_GLIC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_GLIC_HANDLER_H_

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permissions_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui.h"

class Profile;
namespace settings {

class GlicHandler : public SettingsPageUIHandler,
                    public actor_login::ActorLoginPermissionsManager::Observer {
 public:
  GlicHandler();

  GlicHandler(const GlicHandler&) = delete;
  GlicHandler& operator=(const GlicHandler&) = delete;

  ~GlicHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void SetWebUIForTesting(content::WebUI* web_ui);

  // Returns whether the web actuation toggle should be shown for `profile`.
  static bool ShouldShowWebActuationToggle(Profile* profile);

 private:
  FRIEND_TEST_ALL_PREFIXES(GlicHandlerBrowserTest, UpdateShortcutSuspension);
  FRIEND_TEST_ALL_PREFIXES(GlicHandlerBrowserTest, UpdateGlicShortcut);
  FRIEND_TEST_ALL_PREFIXES(GlicHandlerBrowserTest, GetActorLoginPermissions);
  FRIEND_TEST_ALL_PREFIXES(GlicHandlerBrowserTest,
                           RevokeActorLoginPermissionSucceeded);
  FRIEND_TEST_ALL_PREFIXES(GlicHandlerBrowserTest,
                           RevokeActorLoginPermissionFailed);
  FRIEND_TEST_ALL_PREFIXES(GlicHandlerConsentBrowserTest,
                           GetWebActuationToggleVisibility_ConsentAccepted);
  FRIEND_TEST_ALL_PREFIXES(GlicHandlerConsentBrowserTest,
                           GetWebActuationToggleVisibility_ConsentNotAccepted);
  FRIEND_TEST_ALL_PREFIXES(
      GlicHandlerConsentBrowserTest,
      FireWebActuationToggleVisibilityChanged_ConsentAccepted);
  FRIEND_TEST_ALL_PREFIXES(
      GlicHandlerSubscriptionTierBrowserTest,
      GetWebActuationToggleVisibility_SubscriptionTierIneligible);
  FRIEND_TEST_ALL_PREFIXES(
      GlicHandlerSubscriptionTierBrowserTest,
      GetWebActuationToggleVisibility_SubscriptionTierEligible);
  FRIEND_TEST_ALL_PREFIXES(
      GlicHandlerSubscriptionTierBrowserTest,
      FireWebActuationToggleVisibilityChanged_SubscriptionTierBecomesEligible);
  // ActorLoginPermissionsManager::Observer:
  void OnPermissionsChanged() override;

  // Updates settings based on the OS launcher enabled state.
  void HandleSetGlicOsLauncherEnabled(const base::ListValue& args);

  // Sends to the settings page the last saved shortcut.
  void HandleGetGlicShortcut(const base::ListValue& args);

  // Updates the registered glic hotkey with the one provided in `args`.
  void HandleSetGlicShortcut(const base::ListValue& args);

  // Updates the GlobalAcceleratorListener to suspend/unsuspend listening for
  // accelerator input based on `args`.
  void HandleSetShortcutSuspensionState(const base::ListValue& args);

  // Sends the last saved glic focus toggle shortcut to the settings page.
  void HandleGetGlicFocusToggleShortcut(const base::ListValue& args);

  // Updates the glic focus toggle hotkey with the one provided in
  // `args`.
  void HandleSetGlicFocusToggleShortcut(const base::ListValue& args);

  // Sends the client whether glic is disallowed by the admin or not.
  void HandleGetGlicDisallowedByAdmin(const base::ListValue& args);

  // Handles requests for actor login permissions for display in settings.
  void HandleGetActorLoginPermissions(const base::ListValue& args);

  // Handles requests to revoke an actor login permission.
  void HandleRevokeActorLoginPermission(const base::ListValue& args);

  // Resolves the async request to revoke an actor login permission.
  void OnRevokeActorLoginPermission(std::string callback_id_str, bool success);

  // Sends to the settings page the last saved shortcut.
  void HandleGetGlicSelectionShortcut(const base::ListValue& args);

  // Updates the registered glic selection hotkey with the one provided in
  // `args`.
  void HandleSetGlicSelectionShortcut(const base::ListValue& args);

  // Notifies the client whether glic is disallowed by their administrator,
  // either on request or because it changed.
  void FireOnGlicDisallowedByAdminChanged();

  // Sends the client whether the web actuation toggle should be visible.
  void HandleGetWebActuationToggleVisibility(const base::ListValue& args);

  // Notifies the client that the web actuation toggle visibility has changed.
  void FireWebActuationToggleVisibilityChanged();

  // Callback for when the web actuation preference changes.
  void OnWebActuationPrefChanged();

  // Callback for when the ActorKeyedService notifies of a capability change.
  void OnWebActuationCapabilityChanged(bool can_act_on_web);

  // Requests a list of the actor login permissions asynchronously.
  void RequestPermissionsList(
      base::OnceCallback<void(base::ListValue)> callback);

  // Called when actor login permissions change.
  void NotifyPermissionsChanged(base::ListValue permissions_list);

  // Called to resolve the JavaScript callback for getActorLoginPermissions.
  void OnGetActorLoginPermissions(std::string callback_id_str,
                                  base::ListValue permissions_list);

  // Used to listen to changes in web actuation capability status.
  base::CallbackListSubscription web_actuation_subscription_;

  // Used to listen to changes in glic enabling status.
  base::CallbackListSubscription glic_enabling_subscription_;

  std::unique_ptr<actor_login::ActorLoginPermissionsManager>
      actor_login_permissions_manager_;

  base::ScopedObservation<actor_login::ActorLoginPermissionsManager,
                          actor_login::ActorLoginPermissionsManager::Observer>
      observation_{this};
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<GlicHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_GLIC_HANDLER_H_
