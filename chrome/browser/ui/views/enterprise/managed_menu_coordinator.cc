// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/enterprise/managed_menu_coordinator.h"

#include "chrome/browser/ui/views/enterprise/managed_menu_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/management_toolbar_button.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_utils.h"

ManagedMenuCoordinator::~ManagedMenuCoordinator() {
  // Forcefully close the Widget if it hasn't been closed by the time the
  // browser is torn down to avoid dangling references.
  if (IsShowing()) {
    bubble_tracker_.view()->GetWidget()->CloseNow();
  }
}

void ManagedMenuCoordinator::Show() {
  auto* management_toolbar_button =
      BrowserView::GetBrowserViewForBrowser(&GetBrowser())
          ->toolbar_button_provider()
          ->GetManagementToolbarButton();

  // Do not show management bubble if there is no management menu button or the
  // bubble is already showing.
  if (!management_toolbar_button || IsShowing()) {
    return;
  }

  auto& browser = GetBrowser();

  std::unique_ptr<ManagedMenuView> bubble =
      std::make_unique<ManagedMenuView>(management_toolbar_button, &browser);

  auto* bubble_ptr = bubble.get();
  DCHECK_EQ(nullptr, bubble_tracker_.view());
  bubble_tracker_.SetView(bubble_ptr);

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  widget->Show();
}

bool ManagedMenuCoordinator::IsShowing() const {
  return bubble_tracker_.view() != nullptr;
}

ManagedMenuCoordinator::ManagedMenuCoordinator(Browser* browser)
    : BrowserUserData<ManagedMenuCoordinator>(*browser) {}

BROWSER_USER_DATA_KEY_IMPL(ManagedMenuCoordinator);
