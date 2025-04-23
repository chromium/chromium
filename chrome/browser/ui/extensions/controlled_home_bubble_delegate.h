// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_BUBBLE_DELEGATE_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/base/mojom/dialog_button.mojom.h"

class Browser;
class Profile;

namespace extensions {
class Extension;
}

// A bubble shown for an extension overriding the user's home page (different
// than the NTP).
// TODO(crbug.com/40946250): Have this class use the new dialog builders
// and remove ToolbarActionsBarBubbleDelegate.
class ControlledHomeBubbleDelegate
    : public ToolbarActionsBarBubbleDelegate,
      public extensions::ExtensionRegistryObserver {
 public:
  explicit ControlledHomeBubbleDelegate(Browser* browser);

  ControlledHomeBubbleDelegate(const ControlledHomeBubbleDelegate&) = delete;
  ControlledHomeBubbleDelegate& operator=(const ControlledHomeBubbleDelegate&) =
      delete;

  ~ControlledHomeBubbleDelegate() override;

  // The key in the extensions preferences to indicate if an extension has been
  // acknowledged.
  static constexpr char kAcknowledgedPreference[] = "ack_settings_bubble";

  // Called when the bubble is set to show (but hasn't quite shown yet).
  void PendingShow();

  // ToolbarActionsBarBubbleDelegate:
  bool ShouldShow() override;
  std::u16string GetHeadingText() override;
  std::u16string GetBodyText(bool anchored_to_action) override;
  std::u16string GetActionButtonText() override;
  std::u16string GetDismissButtonText() override;
  ui::mojom::DialogButton GetDefaultDialogButton() override;
  std::unique_ptr<ExtraViewInfo> GetExtraViewInfo() override;
  std::string GetAnchorActionId() override;
  void OnBubbleShown(base::OnceClosure close_bubble_callback) override;
  void OnBubbleClosed(CloseAction action) override;

  const extensions::Extension* extension_for_testing() {
    return extension_.get();
  }
  // Don't try to navigate when "learn more" is clicked.
  static base::AutoReset<bool> IgnoreLearnMoreForTesting();
  // Clear the set of shown profiles.
  static void ClearProfileSetForTesting();

 private:
  // Returns true if we should add the policy indicator to the bubble.
  bool IsPolicyIndicationNeeded() const;

  // ExtensionRegistryObserver:
  void OnShutdown(extensions::ExtensionRegistry* registry) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // Checks whether `extension` corresponds to this bubble's extension and,
  // if so, closes the bubble.
  void HandleExtensionUnloadOrUninstall(const extensions::Extension* extension);

  // The corresponding `Browser`.
  raw_ptr<Browser> const browser_;
  // The corresponding `Profile`.
  raw_ptr<Profile> const profile_;
  // The action taken when the bubble closed, if any.
  std::optional<CloseAction> close_action_;
  // The extension controlling the home page, if any. This is null'd out when
  // the extension is uninstalled.
  scoped_refptr<const extensions::Extension> extension_;
  // A closure to close the native view for the bubble. Populated in
  // `OnBubbleShown()`.
  base::OnceClosure close_bubble_callback_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_BUBBLE_DELEGATE_H_
