// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"

#include "base/i18n/case_conversion.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns sorted extension ids based on their extensions name.
std::vector<std::string> SortExtensionsByName(
    ToolbarActionsModel* toolbar_model) {
  auto sort_by_name = [toolbar_model](const ToolbarActionsModel::ActionId a,
                                      const ToolbarActionsModel::ActionId b) {
    return base::i18n::ToLower(toolbar_model->GetExtensionName(a)) <
           base::i18n::ToLower(toolbar_model->GetExtensionName(b));
  };
  std::vector<std::string> sorted_ids(toolbar_model->action_ids().begin(),
                                      toolbar_model->action_ids().end());
  std::sort(sorted_ids.begin(), sorted_ids.end(), sort_by_name);
  return sorted_ids;
}

}  // namespace

ExtensionsMenuViewController::ExtensionsMenuViewController(
    Browser* browser,
    ExtensionsContainer* extensions_container,
    views::View* bubble_contents,
    views::BubbleDialogDelegate* bubble_delegate)
    : browser_(browser),
      extensions_container_(extensions_container),
      bubble_contents_(bubble_contents),
      bubble_delegate_(bubble_delegate),
      toolbar_model_(ToolbarActionsModel::Get(browser_->profile())) {
  browser_->tab_strip_model()->AddObserver(this);
}

void ExtensionsMenuViewController::OpenMainPage() {
  auto main_page = std::make_unique<ExtensionsMenuMainPageView>(browser_, this);

  // Populate.
  bool allow_pinning = extensions_container_->CanShowActionsInToolbar();
  std::vector<std::string> sorted_ids = SortExtensionsByName(toolbar_model_);
  for (size_t i = 0; i < sorted_ids.size(); ++i) {
    // TODO(emiliapaz): Under MVC architecte, view should not own the view
    // controller. However, the current extensions structure depends on this
    // thus a major restructure is needed.
    std::unique_ptr<ExtensionActionViewController> action_controller =
        ExtensionActionViewController::Create(sorted_ids[i], browser_,
                                              extensions_container_);
    main_page->CreateAndInsertMenuItem(std::move(action_controller),
                                       allow_pinning, i);
  }

  SwitchToPage(std::move(main_page));
}

void ExtensionsMenuViewController::OpenSitePermissionsPage() {
  auto site_permissions_page =
      std::make_unique<ExtensionsMenuSitePermissionsPage>(this);
  SwitchToPage(std::move(site_permissions_page));
}

void ExtensionsMenuViewController::CloseBubble() {
  bubble_contents_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void ExtensionsMenuViewController::TabChangedAt(content::WebContents* contents,
                                                int index,
                                                TabChangeType change_type) {
  DCHECK(current_page_);
  current_page_->Update(contents);
}

void ExtensionsMenuViewController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  content::WebContents* web_contents = tab_strip_model->GetActiveWebContents();
  if (!selection.active_tab_changed() || !web_contents) {
    return;
  }

  current_page_->Update(browser_->tab_strip_model()->GetActiveWebContents());
}

// TODO(crbug.com/1390952): Listen for "toolbar pinned actions changed" to
// update the pin button. Currently pin button icon is not updated after
// clicking on it.

ExtensionsMenuMainPageView*
ExtensionsMenuViewController::GetMainPageViewForTesting() {
  DCHECK(current_page_);
  return views::AsViewClass<ExtensionsMenuMainPageView>(current_page_);
}

void ExtensionsMenuViewController::SwitchToPage(
    std::unique_ptr<ExtensionsMenuPageView> page) {
  if (current_page_) {
    bubble_contents_->RemoveChildViewT(current_page_.get());
  }
  current_page_ = bubble_contents_->AddChildView(std::move(page));

  // Only resize the menu if the bubble is created, since page could be added to
  // the menu beforehand and delegate wouldn't know the bubble bounds.
  if (bubble_delegate_->GetBubbleFrameView()) {
    bubble_delegate_->SizeToContents();
  }
}
