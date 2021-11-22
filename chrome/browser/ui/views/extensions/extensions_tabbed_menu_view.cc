// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {
ExtensionsTabbedMenuView* g_extensions_dialog = nullptr;
}  // namespace

ExtensionsTabbedMenuView::ExtensionsTabbedMenuView(
    views::View* anchor_view,
    ExtensionsToolbarButton::ButtonType button_type)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT) {
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

  CreateTabbedPane(button_type);
}

ExtensionsTabbedMenuView::~ExtensionsTabbedMenuView() {
  g_extensions_dialog = nullptr;
}

// static
views::Widget* ExtensionsTabbedMenuView::ShowBubble(
    views::View* anchor_view,
    ExtensionsToolbarButton::ButtonType button_type) {
  DCHECK(!g_extensions_dialog);
  DCHECK(base::FeatureList::IsEnabled(features::kExtensionsMenuAccessControl));
  g_extensions_dialog = new ExtensionsTabbedMenuView(anchor_view, button_type);
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

size_t ExtensionsTabbedMenuView::GetSelectedTabIndex() const {
  return tabbed_pane_->GetSelectedTabIndex();
}

std::u16string ExtensionsTabbedMenuView::GetAccessibleWindowTitle() const {
  // The title is already spoken via the call to SetTitle().
  return std::u16string();
}

void ExtensionsTabbedMenuView::CreateTabbedPane(
    ExtensionsToolbarButton::ButtonType button_type) {
  tabbed_pane_ = AddChildView(std::make_unique<views::TabbedPane>());
  tabbed_pane_->SetFocusBehavior(views::View::FocusBehavior::NEVER);

  // TODO(crbug.com/1263310): Populate site access tab.
  auto permissions_view = std::make_unique<views::View>();
  tabbed_pane_->AddTab(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_TITLE),
      std::move(permissions_view));

  // TODO(crbug.com/1263311): Populate extensions installed tab.
  auto installed_view = std::make_unique<views::View>();
  tabbed_pane_->AddTab(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_EXTENSIONS_TAB_TITLE),
      std::move(installed_view));

  // Tabs left to right order is 'site access tab' | 'extensions tab'.
  tabbed_pane_->SelectTabAt(button_type ==
                            ExtensionsToolbarButton::ButtonType::kExtensions);
}

BEGIN_METADATA(ExtensionsTabbedMenuView, views::BubbleDialogDelegateView)
END_METADATA
