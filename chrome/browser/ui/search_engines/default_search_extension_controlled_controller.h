// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_DEFAULT_SEARCH_EXTENSION_CONTROLLED_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_DEFAULT_SEARCH_EXTENSION_CONTROLLED_CONTROLLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Profile;
class GURL;
class BrowserWindowInterface;

namespace content {
class WebContents;
}

// Manages the confirmation UI logic for extension-controlled default search
// engines. Used by Omnibox interception to show a dialog before navigation.
class DefaultSearchExtensionControlledController {
 public:
  DECLARE_USER_DATA(DefaultSearchExtensionControlledController);

  static DefaultSearchExtensionControlledController* From(
      BrowserWindowInterface* browser);

  // Run when the confirmation request completes. `std::nullopt` means no
  // dialog was ever shown. The confirmation turned out to be unnecessary, or
  // the extension state changed before the dialog could be built. Callers that
  // deferred work pending confirmation must resume it in that case; a dialog
  // the user never saw must never be treated as a decision the user made.
  using ConfirmationCallback = base::OnceCallback<void(
      std::optional<SettingsOverriddenDialogController::DialogResult> result)>;

  explicit DefaultSearchExtensionControlledController(
      BrowserWindowInterface& browser_window_interface,
      Profile& profile);
  ~DefaultSearchExtensionControlledController();

  DefaultSearchExtensionControlledController(
      const DefaultSearchExtensionControlledController&) = delete;
  DefaultSearchExtensionControlledController& operator=(
      const DefaultSearchExtensionControlledController&) = delete;

  // Checks if confirmation is required (e.g., checks if the extension is
  // already acknowledged).
  bool ShouldRequestConfirmationForExtensionDse(const GURL& url) const;

  // Shows the confirmation dialog. The callback is run when the request
  // completes -- see ConfirmationCallback for the no-dialog-shown case.
  void ShowConfirmationDialog(content::WebContents& web_contents,
                              ConfirmationCallback callback);

  void DialogResolved(
      std::optional<SettingsOverriddenDialogController::DialogResult>
          dialog_result);

 private:
  void OnParamsLoaded(
      ConfirmationCallback callback,
      std::unique_ptr<ExtensionSettingsOverriddenDialog::Params> params);

  // Tracks the active controller instance currently showing (or preparing to
  // show) the confirmation dialog. Used to enforce that only one dialog is
  // active at any time across all browser windows, suppressing duplicate prompt
  // prompts if multiple search navigations occur concurrently.
  static base::WeakPtr<DefaultSearchExtensionControlledController>
  GetDialogCurrentlyShowing();

  static void SetDialogCurrentlyShowing(
      base::WeakPtr<DefaultSearchExtensionControlledController> controller);

  ConfirmationCallback confirmation_callback_;

  // Keeps the controller attached to the Browser's UnownedUserData.
  ui::ScopedUnownedUserData<DefaultSearchExtensionControlledController>
      scoped_unowned_user_data_;
  const raw_ref<BrowserWindowInterface> browser_window_interface_;
  const raw_ref<Profile> profile_;

  base::WeakPtrFactory<DefaultSearchExtensionControlledController>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_DEFAULT_SEARCH_EXTENSION_CONTROLLED_CONTROLLER_H_
