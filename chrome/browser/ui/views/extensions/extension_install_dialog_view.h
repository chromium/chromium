// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/view.h"

class Profile;

namespace content {
class PageNavigator;
}

// Modal dialog that shows when the user attempts to install an extension. Also
// shown if the extension is already installed but needs additional permissions.
// Not a normal "bubble" despite being a subclass of BubbleDialogDelegateView.
class ExtensionInstallDialogView
    : public views::BubbleDialogDelegateView,
      public extensions::ExtensionRegistryObserver {
 public:
  // The views::View::id of the ratings section in the dialog.
  static const int kRatingsViewId = 1;

  ExtensionInstallDialogView(
      Profile* profile,
      content::PageNavigator* navigator,
      const ExtensionInstallPrompt::DoneCallback& done_callback,
      std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt);
  ~ExtensionInstallDialogView() override;

  // Returns the interior ScrollView of the dialog. This allows us to inspect
  // the contents of the DialogView.
  const views::ScrollView* scroll_view() const { return scroll_view_; }

  static void SetInstallButtonDelayForTesting(int timeout_in_ms);

  // Changes the widget size to accommodate the contents' preferred size.
  void ResizeWidget();

  // views::BubbleDialogDelegate:
  gfx::Size CalculatePreferredSize() const override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void AddedToWidget() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;

 private:
  void CloseDialog();

  // extensions::ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // views::WidgetDelegate:
  ax::mojom::Role GetAccessibleWindowRole() override;
  base::string16 GetAccessibleWindowTitle() const override;
  ui::ModalType GetModalType() const override;

  void LinkClicked();
  void OnDialogCanceled();
  void OnDialogAccepted();

  // Creates the contents area that contains permissions and other extension
  // info.
  void CreateContents();

  // Enables the install button and updates the dialog buttons.
  void EnableInstallButton();

  bool is_external_install() const {
    return prompt_->type() == ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT;
  }

  // Updates the histogram that holds installation accepted/aborted data.
  void UpdateInstallResultHistogram(bool accepted) const;

  Profile* profile_;
  content::PageNavigator* navigator_;
  ExtensionInstallPrompt::DoneCallback done_callback_;
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt_;
  base::string16 title_;
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  // The scroll view containing all the details for the dialog (including all
  // collapsible/expandable sections).
  views::ScrollView* scroll_view_;

  // Used to record time between dialog creation and acceptance, cancellation,
  // or dismissal.
  base::Optional<base::ElapsedTimer> install_result_timer_;

  // Used to delay the activation of the install button.
  base::OneShotTimer enable_install_timer_;

  // Used to determine whether the install button should be enabled.
  bool install_button_enabled_;

  // Checkbox used to indicate if permissions should be withheld on install.
  views::Checkbox* withhold_permissions_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_
