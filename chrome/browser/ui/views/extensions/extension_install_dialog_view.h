// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

class Profile;

// Modal dialog that shows when the user attempts to install an extension. Also
// shown if the extension is already installed but needs additional permissions.
// Not a normal "bubble" despite being a subclass of BubbleDialogDelegateView.
class ExtensionInstallDialogView : public views::BubbleDialogDelegateView,
                                   public extensions::ExtensionRegistryObserver,
                                   public views::TextfieldController {
  METADATA_HEADER(ExtensionInstallDialogView, views::BubbleDialogDelegateView)

 public:
  // The views::View::id of the ratings section in the dialog.
  static const int kRatingsViewId = 1;

  ExtensionInstallDialogView(
      std::unique_ptr<ExtensionInstallPromptShowParams> show_params,
      ExtensionInstallPrompt::DoneCallback done_callback,
      std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt);
  ExtensionInstallDialogView(const ExtensionInstallDialogView&) = delete;
  ExtensionInstallDialogView& operator=(const ExtensionInstallDialogView&) =
      delete;
  ~ExtensionInstallDialogView() override;

  // Returns the interior ScrollView of the dialog. This allows us to inspect
  // the contents of the DialogView.
  const views::ScrollView* scroll_view() const { return scroll_view_; }

  static void SetInstallButtonDelayForTesting(int timeout_in_ms);

  // Changes the widget size to accommodate the contents' preferred size.
  void ResizeWidget();

  // views::BubbleDialogDelegateView:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void AddedToWidget() override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  std::u16string GetAccessibleWindowTitle() const override;

  ExtensionInstallPromptShowParams* GetShowParamsForTesting();
  void ClickLinkForTesting();
  bool IsJustificationFieldVisibleForTesting();
  void SetJustificationTextForTesting(const std::u16string& new_text);

 private:
  // Forward-declaration.
  class ExtensionJustificationView;

  void CloseDialog();

  // extensions::ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  void LinkClicked();
  void OnDialogCanceled();
  void OnDialogAccepted();

  // Creates the contents area that contains permissions and other extension
  // info.
  void CreateContents();

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // Enables the install button and updates the dialog buttons.
  void EnableInstallButton();

  raw_ptr<Profile> profile_;
  std::unique_ptr<ExtensionInstallPromptShowParams> show_params_;
  ExtensionInstallPrompt::DoneCallback done_callback_;
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt_;
  std::u16string title_;
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // The scroll view containing all the details for the dialog (including all
  // collapsible/expandable sections).
  raw_ptr<views::ScrollView> scroll_view_;

  // Used to record time between dialog creation and acceptance, cancellation,
  // or dismissal.
  std::optional<base::ElapsedTimer> install_result_timer_;

  // Used to delay the activation of the install button.
  base::OneShotTimer enable_install_timer_;

  // Used to determine whether the install button should be enabled.
  bool install_button_enabled_;

  // Along with install_button_enabled_, used to determine whether the extension
  // request button should be enabled. Its value is initialized to |true| so
  // that it has no effect unless the justification field is present and the
  // entered text length is larger than the defined limit.
  bool request_button_enabled_ = true;

  // Checkbox used to indicate if host permissions should be granted on install.
  // Should only be present when permissions are withheld on installation by
  // default.
  raw_ptr<views::Checkbox> grant_permissions_checkbox_;

  // The justification text field view where users enter their justification for
  // requesting an extension.
  raw_ptr<ExtensionJustificationView> justification_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_
