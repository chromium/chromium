// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"

#include <memory>

#include "base/feature_list.h"
#include "extensions/common/extension_features.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

ExtensionsMenuCoordinator::ExtensionsMenuCoordinator() = default;

ExtensionsMenuCoordinator::~ExtensionsMenuCoordinator() {
  Hide();
}

void ExtensionsMenuCoordinator::Show(views::View* anchor_view) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      anchor_view, views::BubbleBorder::TOP_RIGHT);
  bubble_delegate->set_margins(gfx::Insets(0));
  bubble_delegate->set_fixed_width(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  // Let anchor view's MenuButtonController handle the highlight.
  bubble_delegate->set_highlight_button_when_shown(false);
  bubble_delegate->SetButtons(ui::DIALOG_BUTTON_NONE);
  bubble_delegate->SetShowCloseButton(true);
  bubble_delegate->SetEnableArrowKeyTraversal(true);

  // TODO(crbug.com/1390952): Use "extensions menu base view" once it's created.
  auto* contents_view =
      bubble_delegate->SetContentsView(std::make_unique<views::View>());
  extensions_menu_bubble_view_tracker_.SetView(contents_view);

  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate))->Show();
}

void ExtensionsMenuCoordinator::Hide() {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));
  views::Widget* const menu = GetExtensionsMenuWidget();
  if (menu) {
    menu->Close();
    // Immediately stop tracking the view. Widget will be destroyed
    // asynchronously.
    extensions_menu_bubble_view_tracker_.SetView(nullptr);
  }
}

bool ExtensionsMenuCoordinator::IsShowing() const {
  return !!extensions_menu_bubble_view_tracker_.view();
}

views::Widget* ExtensionsMenuCoordinator::GetExtensionsMenuWidget() {
  return IsShowing() ? extensions_menu_bubble_view_tracker_.view()->GetWidget()
                     : nullptr;
}
