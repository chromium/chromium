// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_intervention_bubble_delegate.h"

#include "base/check.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_bubble_observer.h"

PerformanceInterventionBubbleDelegate::PerformanceInterventionBubbleDelegate(
    Browser* browser,
    PerformanceInterventionBubbleObserver* observer)
    : browser_(browser), observer_(observer) {
  CHECK(browser);
}

PerformanceInterventionBubbleDelegate::
    ~PerformanceInterventionBubbleDelegate() = default;

void PerformanceInterventionBubbleDelegate::OnBubbleClosed() {
  // TODO(crbug.com/341138308): Record metrics for when the dialog is
  // closed by not clicking the dismiss or deactivate buttons.

  observer_->OnBubbleHidden();
}

void PerformanceInterventionBubbleDelegate::OnDismissButtonClicked() {
  // TODO(crbug.com/341138308): Record metrics for when the dismiss button is
  // clicked.

  observer_->OnBubbleHidden();
}

void PerformanceInterventionBubbleDelegate::OnDeactivateButtonClicked() {
  // TODO(crbug.com/341138308): Record metrics for when the deactivate button is
  // clicked.

  // TODO(crbug.com/338073040): Discard the selected tabs in the tab list.

  observer_->OnDeactivateButtonClicked();
}
