// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog_coordinator.h"

#include <memory>

#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog.h"
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
    IncognitoClearBrowsingDataDialogInterface::Type type,
    views::View* anchor_view) {
  auto bubble = std::make_unique<IncognitoClearBrowsingDataDialog>(
      anchor_view, profile_, type);
  DCHECK_EQ(nullptr, bubble_tracker_.view());
  bubble_tracker_.SetView(bubble.get());

  auto* widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  widget->Show();
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

IncognitoClearBrowsingDataDialogCoordinator::
    IncognitoClearBrowsingDataDialogCoordinator(Profile* profile)
    : profile_(profile) {}
