// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_utils.h"

namespace {
ExtensionsTabbedMenuView* g_extensions_dialog = nullptr;

// Adds a new tab in `tabbed_pane` at `index` with the given `title` and
// `contents`.
void CreateTab(raw_ptr<views::TabbedPane> tabbed_pane,
               size_t index,
               int title_string_id,
               std::unique_ptr<views::View> contents) {
  // This is set so that the extensions menu doesn't fall outside the monitor in
  // a maximized window in 1024x768. See https://crbug.com/1096630.
  // TODO(pbos): Consider making this dynamic and handled by views. Ideally we
  // wouldn't ever pop up so that they pop outside the screen.
  constexpr int kMaxExtensionButtonsHeightDp = 448;
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->ClipHeightTo(0, kMaxExtensionButtonsHeightDp);
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetContents(std::move(contents));

  tabbed_pane->AddTabAtIndex(index, l10n_util::GetStringUTF16(title_string_id),
                             std::move(scroll_view));
}

// A helper method to convert to an ExtensionsMenuItemView. This cannot be used
// to *determine* if a view is an ExtensionsMenuItemView (it should only be used
// when the view is known to be one). It is only used as an extra measure to
// prevent bad static casts.
ExtensionsMenuItemView* GetAsMenuItemView(views::View* view) {
  DCHECK(views::IsViewClass<ExtensionsMenuItemView>(view));
  return static_cast<ExtensionsMenuItemView*>(view);
}

}  // namespace

ExtensionsTabbedMenuView::ExtensionsTabbedMenuView(
    views::View* anchor_view,
    Browser* browser,
    ExtensionsContainer* extensions_container,
    ExtensionsToolbarButton::ButtonType button_type,
    bool allow_pinning)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      browser_(browser),
      extensions_container_(extensions_container),
      toolbar_model_(ToolbarActionsModel::Get(browser_->profile())),
      allow_pinning_(allow_pinning) {
  // Ensure layer masking is used for the extensions menu to ensure buttons with
  // layer effects sitting flush with the bottom of the bubble are clipped
  // appropriately.
  SetPaintClientToLayer(true);

  set_margins(gfx::Insets(0));

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowCloseButton(true);
  SetTitle(IDS_EXTENSIONS_MENU_TITLE);
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());

  SetEnableArrowKeyTraversal(true);

  // Let anchor view's MenuButtonController handle the highlight.
  set_highlight_button_when_shown(false);

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  Populate(button_type);
}

ExtensionsTabbedMenuView::~ExtensionsTabbedMenuView() {
  g_extensions_dialog = nullptr;
}

// static
views::Widget* ExtensionsTabbedMenuView::ShowBubble(
    views::View* anchor_view,
    Browser* browser,
    ExtensionsContainer* extensions_container_,
    ExtensionsToolbarButton::ButtonType button_type,
    bool allow_pining) {
  DCHECK(!g_extensions_dialog);
  DCHECK(base::FeatureList::IsEnabled(features::kExtensionsMenuAccessControl));
  g_extensions_dialog = new ExtensionsTabbedMenuView(
      anchor_view, browser, extensions_container_, button_type, allow_pining);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(g_extensions_dialog);
  widget->Show();
  return widget;
}

// static
bool ExtensionsTabbedMenuView::IsShowing() {
  return g_extensions_dialog != nullptr;
}

// static
void ExtensionsTabbedMenuView::Hide() {
  DCHECK(base::FeatureList::IsEnabled(features::kExtensionsMenuAccessControl));
  if (IsShowing()) {
    g_extensions_dialog->GetWidget()->Close();
    // Set the dialog to nullptr since `GetWidget->Close()` is not synchronous.
    g_extensions_dialog = nullptr;
  }
}

// static
ExtensionsTabbedMenuView*
ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting() {
  return g_extensions_dialog;
}

std::vector<ExtensionsMenuItemView*>
ExtensionsTabbedMenuView::GetInstalledItemsForTesting() const {
  std::vector<ExtensionsMenuItemView*> menu_item_views;
  if (IsShowing()) {
    for (views::View* view : installed_items_->children())
      menu_item_views.push_back(GetAsMenuItemView(view));
  }
  return menu_item_views;
}

size_t ExtensionsTabbedMenuView::GetSelectedTabIndex() const {
  return tabbed_pane_->GetSelectedTabIndex();
}

std::u16string ExtensionsTabbedMenuView::GetAccessibleWindowTitle() const {
  // The title is already spoken via the call to SetTitle().
  return std::u16string();
}

void ExtensionsTabbedMenuView::Populate(
    ExtensionsToolbarButton::ButtonType button_type) {
  // The actions for the profile haven't been initialized yet. We'll call in
  // again once they have.
  if (!toolbar_model_->actions_initialized())
    return;

  DCHECK(children().empty()) << "Populate() can only be called once!";

  tabbed_pane_ = AddChildView(std::make_unique<views::TabbedPane>());
  tabbed_pane_->SetFocusBehavior(views::View::FocusBehavior::NEVER);

  // TODO(crbug.com/1263310): Populate site access tab.
  CreateTab(tabbed_pane_, 0, IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_TITLE,
            std::make_unique<views::View>());

  auto installed_items = std::make_unique<views::View>();
  installed_items->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  installed_items_ = installed_items.get();
  CreateTab(tabbed_pane_, 1, IDS_EXTENSIONS_MENU_EXTENSIONS_TAB_TITLE,
            std::move(installed_items));

  // Sort action ids based on their extension name.
  auto sort_by_name = [this](const ToolbarActionsModel::ActionId a,
                             const ToolbarActionsModel::ActionId b) {
    return base::i18n::ToLower(
               base::UTF8ToUTF16(toolbar_model_->GetExtensionName(a))) <
           base::i18n::ToLower(
               base::UTF8ToUTF16(toolbar_model_->GetExtensionName(b)));
  };
  std::vector<std::string> sorted_ids(toolbar_model_->action_ids().begin(),
                                      toolbar_model_->action_ids().end());
  std::sort(sorted_ids.begin(), sorted_ids.end(), sort_by_name);

  for (size_t i = 0; i < sorted_ids.size(); ++i)
    CreateAndInsertInstalledExtension(sorted_ids[i], i);

  // Tabs left to right order is 'site access tab' | 'extensions tab'.
  tabbed_pane_->SelectTabAt(button_type ==
                            ExtensionsToolbarButton::ButtonType::kExtensions);
}

void ExtensionsTabbedMenuView::CreateAndInsertInstalledExtension(
    const ToolbarActionsModel::ActionId& id,
    int index) {
  std::unique_ptr<ExtensionActionViewController> controller =
      ExtensionActionViewController::Create(id, browser_,
                                            extensions_container_);
  auto item = std::make_unique<ExtensionsMenuItemView>(
      ExtensionsMenuItemView::MenuItemType::kExtensions, browser_,
      std::move(controller), allow_pinning_);
  installed_items_->AddChildViewAt(std::move(item), index);
}

BEGIN_METADATA(ExtensionsTabbedMenuView, views::BubbleDialogDelegateView)
END_METADATA
