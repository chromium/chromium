// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view_class_properties.h"

namespace {
// The delay before we close the app menu if this was opened for a drop so that
// the user can see a browser action if one was moved.
// This can be changed for tests.
base::TimeDelta g_close_menu_delay = base::TimeDelta::FromMilliseconds(300);
}

ExtensionToolbarMenuView::ExtensionToolbarMenuView(
    Browser* browser,
    views::MenuItemView* menu_item)
    : browser_(browser), menu_item_(menu_item) {
  CHECK(!base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  auto* toolbar_button_provider = browser_view->toolbar_button_provider();
  auto* app_menu_button = toolbar_button_provider->GetAppMenuButton();
  app_menu_ = app_menu_button->app_menu();

  // Use a transparent background so that the menu's background shows through.
  // None of the children use layers, so this should be ok.
  SetBackgroundColor(SK_ColorTRANSPARENT);
  BrowserActionsContainer* main =
      toolbar_button_provider->GetBrowserActionsContainer();
  auto container = std::make_unique<BrowserActionsContainer>(browser_, main,
                                                             main->delegate());
  container_ = SetContents(std::move(container));

  // Listen for the drop to finish so we can close the app menu, if necessary.
  toolbar_actions_bar_observer_.Add(main->toolbar_actions_bar());

  // Observe app menu so we know when RunMenu() is called.
  app_menu_button_observer_.Add(app_menu_button);

  // In *very* extreme cases, it's possible that there are so many overflowed
  // actions, we won't be able to show them all. Cap the height so that the
  // overflow won't be excessively tall (at 8 icons per row, this allows for
  // 104 total extensions).
  constexpr int kMaxOverflowRows = 13;
  max_height_ = container_->GetToolbarActionSize().height() * kMaxOverflowRows;
  ClipHeightTo(0, max_height_);
}

ExtensionToolbarMenuView::~ExtensionToolbarMenuView() {
}

gfx::Size ExtensionToolbarMenuView::CalculatePreferredSize() const {
  gfx::Size s = views::ScrollView::CalculatePreferredSize();
  // views::ScrollView::CalculatePreferredSize() includes the contents' size,
  // but not the scrollbar width. Add it in if necessary.
  if (container_->GetPreferredSize().height() > max_height_)
    s.Enlarge(GetScrollBarLayoutWidth(), 0);
  return s;
}

void ExtensionToolbarMenuView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  menu_item_->GetParentMenuItem()->ChildrenChanged();
}

void ExtensionToolbarMenuView::set_close_menu_delay_for_testing(
    base::TimeDelta delay) {
  g_close_menu_delay = delay;
}

void ExtensionToolbarMenuView::OnToolbarActionsBarDestroyed() {
  toolbar_actions_bar_observer_.RemoveAll();
}

void ExtensionToolbarMenuView::OnToolbarActionDragDone() {
  // In the case of a drag-and-drop, the bounds of the container may have
  // changed (in the case of removing an icon that was the last in a row).
  UpdateMargins();
  PreferredSizeChanged();

  // We need to close the app menu if it was just opened for the drag and drop,
  // or if there are no more extensions in the overflow menu after a drag and
  // drop.
  if (app_menu_->for_drop() ||
      container_->toolbar_actions_bar()->GetIconCount() == 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExtensionToolbarMenuView::CloseAppMenu,
                       weak_factory_.GetWeakPtr()),
        g_close_menu_delay);
  }
}

void ExtensionToolbarMenuView::AppMenuShown() {
  // Set the margins and flag for re-layout. This must be done here since
  // GetStartPadding() depends on views::MenuItemView::label_start() which is
  // initialized upon menu running.
  //
  // TODO(crbug.com/918741): fix MenuItemView so MenuItemView::label_start()
  // returns valid data before menu run time.
  UpdateMargins();
}

void ExtensionToolbarMenuView::CloseAppMenu() {
  app_menu_->CloseMenu();
}

void ExtensionToolbarMenuView::UpdateMargins() {
  SetProperty(views::kMarginsKey,
              gfx::Insets(0, GetStartPadding(), 0, GetEndPadding()));
  menu_item_->Layout();
}

int ExtensionToolbarMenuView::GetStartPadding() const {
  // We pad enough on the left so that the first icon starts at the same point
  // as the labels. We subtract kItemSpacing because there needs to be padding
  // so we can see the drop indicator.
  return views::MenuItemView::label_start() -
      container_->toolbar_actions_bar()->platform_settings().item_spacing;
}

int ExtensionToolbarMenuView::GetEndPadding() const {
  const views::MenuConfig& menu_config = views::MenuConfig::instance();
  // |menu_config.arrow_to_edge_padding| represents the typical trailing space
  // at the end of menu items. Use this, but subtract the item spacing to give
  // room for the drop indicator.
  return menu_config.arrow_to_edge_padding -
         container_->toolbar_actions_bar()->platform_settings().item_spacing;
}
