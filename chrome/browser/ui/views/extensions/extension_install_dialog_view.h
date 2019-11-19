// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"

class Profile;

namespace content {
class PageNavigator;
}

namespace views {
class Link;
}

// Modal dialog that shows when the user attempts to install an extension. Also
// shown if the extension is already installed but needs additional permissions.
// Not a normal "bubble" despite being a subclass of BubbleDialogDelegateView.
class ExtensionInstallDialogView : public views::BubbleDialogDelegateView,
                                   public views::LinkListener {
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

 private:
  // views::BubbleDialogDelegate:
  gfx::Size CalculatePreferredSize() const override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void AddedToWidget() override;
  bool Cancel() override;
  bool Accept() override;
  int GetDialogButtons() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  bool ShouldShowCloseButton() const override;

  // views::WidgetDelegate:
  ax::mojom::Role GetAccessibleWindowRole() override;
  base::string16 GetAccessibleWindowTitle() const override;
  ui::ModalType GetModalType() const override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

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

  // The scroll view containing all the details for the dialog (including all
  // collapsible/expandable sections).
  views::ScrollView* scroll_view_;

  // Set to true once the user's selection has been received and the callback
  // has been run.
  bool handled_result_;

  // Used to record time between dialog creation and acceptance, cancellation,
  // or dismissal.
  base::Optional<base::ElapsedTimer> install_result_timer_;

  // Used to delay the activation of the install button.
  base::OneShotTimer enable_install_timer_;

  // Used to determine whether the install button should be enabled.
  bool install_button_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogView);
};

// A view that displays a list of details, along with a link that expands and
// collapses those details.
class ExpandableContainerView : public views::View, public views::LinkListener {
 public:
  ExpandableContainerView(const std::vector<base::string16>& details,
                          int available_width);
  ~ExpandableContainerView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

 private:
  // Helper class representing the list of details, that can hide itself.
  class DetailsView : public views::View {
   public:
    explicit DetailsView(const std::vector<base::string16>& details);
    ~DetailsView() override {}

    // views::View:
    gfx::Size CalculatePreferredSize() const override;

    // Expands or collapses this view.
    void ToggleExpanded();

    bool expanded() { return expanded_; }

   private:
    // Whether this details section is expanded.
    bool expanded_ = false;

    DISALLOW_COPY_AND_ASSIGN(DetailsView);
  };

  // Expands or collapses |details_view_|.
  void ToggleDetailLevel();

  // The view that expands or collapses when |details_link_| is clicked.
  DetailsView* details_view_;

  // The 'Show Details' link, which changes to 'Hide Details' when the details
  // section is expanded.
  views::Link* details_link_;

  DISALLOW_COPY_AND_ASSIGN(ExpandableContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_H_
