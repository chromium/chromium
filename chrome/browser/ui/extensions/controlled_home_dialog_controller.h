// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_DIALOG_CONTROLLER_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller_interface.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}  // namespace extensions

// A bubble shown for an extension overriding the user's home page (different
// than the NTP).
class ControlledHomeDialogController
    : public ControlledHomeDialogControllerInterface {
 public:
  explicit ControlledHomeDialogController(Profile* profile,
                                          content::WebContents* web_contents);

  ControlledHomeDialogController(const ControlledHomeDialogController&) =
      delete;
  ControlledHomeDialogController& operator=(
      const ControlledHomeDialogController&) = delete;

  ~ControlledHomeDialogController() override;

  // The key in the extensions preferences to indicate if an extension has been
  // acknowledged.
  static constexpr char kAcknowledgedPreference[] = "ack_settings_bubble";

  // Called when the bubble is set to show (but hasn't quite shown yet).
  void PendingShow();

  // ControlledHomeDialogControllerInterface:
  bool ShouldShow() override;
  std::u16string GetHeadingText() override;
  std::u16string GetBodyText() override;
  std::u16string GetActionButtonText() override;
  std::u16string GetDismissButtonText() override;
  bool IsPolicyIndicationNeeded() const override;
  std::string GetAnchorActionId() override;
  void OnBubbleShown() override;
  void OnBubbleClosed(CloseAction action) override;

  const extensions::Extension* extension_for_testing() {
    return extension_.get();
  }
  // Don't try to navigate when "learn more" is clicked.
  static base::AutoReset<bool> IgnoreLearnMoreForTesting();
  // Clear the set of shown profiles.
  static void ClearProfileSetForTesting();

 private:
  // Checks whether `extension` corresponds to this bubble's extension and,
  // if so, closes the bubble.
  void HandleExtensionUnloadOrUninstall(const extensions::Extension* extension);

  // The corresponding `Profile`.
  raw_ptr<Profile> const profile_;
  // The original web contents that triggered the dialog.
  base::WeakPtr<content::WebContents> web_contents_;
  // The action taken when the bubble closed, if any.
  std::optional<CloseAction> close_action_;
  // The extension controlling the home page, if any. This is null'd out when
  // the extension is uninstalled.
  scoped_refptr<const extensions::Extension> extension_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_DIALOG_CONTROLLER_H_
