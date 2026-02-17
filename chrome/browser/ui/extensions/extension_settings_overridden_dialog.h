// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {
class Extension;
}  // namespace extensions

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// The controller for a settings overridden dialog that manages settings
// overridden by an extension. The user has the option to acknowledge or
// disable the extension.
class ExtensionSettingsOverriddenDialog
    : public SettingsOverriddenDialogController {
 public:
  // Preference key to store the timestamp when the simple override enforcement
  // began. Used to grandfather in existing installations.
  static constexpr char kSimpleOverrideBeginConfirmationTimestamp[] =
      "extensions.simple_override_begin_confirmation_timestamp";

  struct Params {
    // Chromium style requires an explicit ctor - which means we need more than
    // one : (
    Params(extensions::ExtensionId controlling_extension_id,
           const char* extension_acknowledged_preference_name,
           const char* dialog_result_histogram_name,
           ShowParams show_params);
    Params(Params&& params);
    Params(const Params& params) = delete;
    ~Params();

    // The ID of the extension controlling the associated setting.
    extensions::ExtensionId controlling_extension_id;
    // The name of the preference to use to mark an extension as
    // acknowledged by the user.
    std::string extension_acknowledged_preference_name;
    // The name of the histogram to use when recording the result of the
    // dialog.
    std::string dialog_result_histogram_name;

    // The text and similar content required for the dialog.
    ShowParams content;
  };

  ExtensionSettingsOverriddenDialog(Params params, Profile& profile);
  ExtensionSettingsOverriddenDialog(const ExtensionSettingsOverriddenDialog&) =
      delete;
  ExtensionSettingsOverriddenDialog& operator=(
      const ExtensionSettingsOverriddenDialog&) = delete;
  ~ExtensionSettingsOverriddenDialog() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns true if the extension with the given `id` has already had settings
  // overridden dialog shown for.
  static bool HasShownFor(Profile& profile, const extensions::ExtensionId& id);

  // Returns true if the extension with the given `id` has already been
  // acknowledged.
  static bool HasAcknowledgedExtension(
      Profile& profile,
      const extensions::ExtensionId& id,
      const std::string& extension_acknowledged_preference_name);

  // Returns true if a simple overridden extension should get a dialog shown.
  static bool ShouldShowForSimpleOverrideExtension(
      Profile& profile,
      const extensions::Extension& extension);

  // SettingsOverriddenDialogController:
  bool ShouldShow() override;
  ShowParams GetShowParams() override;
  void OnDialogShown() override;
  void HandleDialogResult(DialogResult result) override;

  // Sets a callback to be invoked when the dialog result is handled.
  using DialogResultCallback = base::OnceCallback<void(DialogResult)>;
  void SetDialogResultCallback(DialogResultCallback callback);

 private:
  // Disables the extension that controls the setting.
  void DisableControllingExtension();

  // Acknowledges the extension controlling the setting, preventing future
  // prompting.
  void AcknowledgeControllingExtension();

  const Params params_;

  // The profile associated with the controller.
  const raw_ref<Profile> profile_;

  DialogResultCallback dialog_result_callback_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_H_
