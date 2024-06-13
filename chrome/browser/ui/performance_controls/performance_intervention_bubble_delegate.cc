// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_intervention_bubble_delegate.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/tab_list_model.h"

PerformanceInterventionBubbleDelegate::PerformanceInterventionBubbleDelegate(
    Browser* browser,
    std::unique_ptr<TabListModel> tab_list_model,
    PerformanceInterventionBubbleObserver* observer)
    : browser_(browser),
      tab_list_model_(std::move(tab_list_model)),
      observer_(observer) {
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

  performance_manager::user_tuning::PerformanceDetectionManager* manager =
      performance_manager::user_tuning::PerformanceDetectionManager::
          GetInstance();
  CHECK(manager);
  manager->DiscardTabs(tab_list_model_->page_contexts());
  observer_->OnDeactivateButtonClicked();
}
