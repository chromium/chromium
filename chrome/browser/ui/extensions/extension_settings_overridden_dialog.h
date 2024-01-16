// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace gfx {
struct VectorIcon;
}

// The controller for a settings overridden dialog that manages settings
// overridden by an extension. The user has the option to acknowledge or
// disable the extension.
class ExtensionSettingsOverriddenDialog
    : public SettingsOverriddenDialogController {
 public:
  struct Params {
    // Chromium style requires an explicit ctor - which means we need more than
    // one : (
    Params(extensions::ExtensionId controlling_extension_id,
           const char* extension_acknowledged_preference_name,
           const char* dialog_result_histogram_name,
           std::u16string dialog_title,
           std::u16string dialog_message,
           const gfx::VectorIcon* icon);
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

    std::u16string dialog_title;
    std::u16string dialog_message;

    // The icon to display in the dialog, if any.
    // RAW_PTR_EXCLUSION: Seems to always point to nullptr (other VectorIncon*
    // typically point to a global).
    RAW_PTR_EXCLUSION const gfx::VectorIcon* icon = nullptr;
  };

  ExtensionSettingsOverriddenDialog(Params params, Profile* profile);
  ExtensionSettingsOverriddenDialog(const ExtensionSettingsOverriddenDialog&) =
      delete;
  ExtensionSettingsOverriddenDialog& operator=(
      const ExtensionSettingsOverriddenDialog&) = delete;
  ~ExtensionSettingsOverriddenDialog() override;

  // SettingsOverriddenDialogController:
  bool ShouldShow() override;
  ShowParams GetShowParams() override;
  void OnDialogShown() override;
  void HandleDialogResult(DialogResult result) override;

 private:
  // Disables the extension that controls the setting.
  void DisableControllingExtension();

  // Acknowledges the extension controlling the setting, preventing future
  // prompting.
  void AcknowledgeControllingExtension();

  // Returns true if the extension with the given |id| has already been
  // acknowledged.
  bool HasAcknowledgedExtension(const extensions::ExtensionId& id);

  const Params params_;

  // The profile associated with the controller.
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_H_
