// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_base_view.h"
#include "extensions/common/extension_features.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

ExtensionsMenuCoordinator::ExtensionsMenuCoordinator(Browser* browser)
    : browser_(browser) {}

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
  bubble_delegate->SetEnableArrowKeyTraversal(true);

  auto* contents_view = bubble_delegate->SetContentsView(
      std::make_unique<ExtensionsMenuBaseView>(browser_));
  bubble_tracker_.SetView(contents_view);

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
    bubble_tracker_.SetView(nullptr);
  }
}

bool ExtensionsMenuCoordinator::IsShowing() const {
  return bubble_tracker_.view() != nullptr;
}

views::Widget* ExtensionsMenuCoordinator::GetExtensionsMenuWidget() {
  return IsShowing() ? bubble_tracker_.view()->GetWidget() : nullptr;
}
