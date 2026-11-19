// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog_coordinator.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_utils.h"

IncognitoClearBrowsingDataDialogCoordinator::
    ~IncognitoClearBrowsingDataDialogCoordinator() {
  // Forcefully close the Widget if it hasn't been closed by the time the
  // browser is torn down to avoid dangling references.
  if (IsShowing()) {
    bubble_tracker_.view()->GetWidget()->CloseNow();
  }
}

void IncognitoClearBrowsingDataDialogCoordinator::Show(
    IncognitoClearBrowsingDataDialog::Type type,
    AvatarToolbarButtonInterface* avatar_button) {
  // Cancel any pending request to show a bubble before starting a new one.
  weak_ptr_factory_.InvalidateWeakPtrs();

  views::Widget* const existing_widget =
      bubble_tracker_.view() ? bubble_tracker_.view()->GetWidget() : nullptr;
  if (existing_widget) {
    // Untrack and close the previous bubble before creating and showing a new
    // one, while keeping the old bubble's DeletingCallback alive until
    // RunWhenVisible() acquires a new lock.
    bubble_tracker_.SetView(nullptr);
    existing_widget->Close();
  }

  if (avatar_button) {
    avatar_button->RunWhenVisible(
        base::BindOnce(&IncognitoClearBrowsingDataDialogCoordinator::ShowBubble,
                       weak_ptr_factory_.GetWeakPtr(), type));
  }

  if (existing_widget && !bubble_tracker_.view()) {
    bubble_tracker_.SetIsDeletingCallback(base::OnceClosure());
  }
}

bool IncognitoClearBrowsingDataDialogCoordinator::IsShowing() const {
  return bubble_tracker_.view() != nullptr;
}

IncognitoClearBrowsingDataDialog* IncognitoClearBrowsingDataDialogCoordinator::
    GetIncognitoClearBrowsingDataDialogForTesting() {
  return IsShowing() ? views::AsViewClass<IncognitoClearBrowsingDataDialog>(
                           bubble_tracker_.view())
                     : nullptr;
}

void IncognitoClearBrowsingDataDialogCoordinator::ShowBubble(
    IncognitoClearBrowsingDataDialog::Type type,
    views::BubbleAnchor anchor,
    base::ScopedClosureRunner keep_visible) {
  if (anchor.IsNull()) {
    return;
  }

  auto bubble = std::make_unique<IncognitoClearBrowsingDataDialog>(
      anchor, profile_, type);
  bubble_tracker_.SetView(bubble.get());
  bubble_tracker_.SetIsDeletingCallback(
      base::DoNothingWithBoundArgs(std::move(keep_visible)));

  auto* widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  widget->Show();
}

DEFINE_USER_DATA(IncognitoClearBrowsingDataDialogCoordinator);

// static
IncognitoClearBrowsingDataDialogCoordinator*
IncognitoClearBrowsingDataDialogCoordinator::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

IncognitoClearBrowsingDataDialogCoordinator::
    IncognitoClearBrowsingDataDialogCoordinator(Profile* profile,
                                                ui::UnownedUserDataHost& host)
    : scoped_unowned_user_data_(host, *this), profile_(profile) {}
