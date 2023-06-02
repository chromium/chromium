// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_CHIP_VIEW_H_

#include <string>
#include "base/scoped_observation.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_chip_tab_helper.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

// Represents the high efficiency page action chip that appears on previously
// discarded tabs.
class HighEfficiencyChipView : public PageActionIconView,
                               public HighEfficiencyBubbleObserver,
                               public performance_manager::user_tuning::
                                   UserPerformanceTuningManager::Observer {
 public:
  METADATA_HEADER(HighEfficiencyChipView);
  // The number of times a user should see the expanded chip.
  static constexpr int kChipAnimationCount = 3;

  HighEfficiencyChipView(
      CommandUpdater* command_updater,
      Browser* browser,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  HighEfficiencyChipView(const HighEfficiencyChipView&) = delete;
  HighEfficiencyChipView& operator=(const HighEfficiencyChipView&) = delete;
  ~HighEfficiencyChipView() override;

  // HighEfficiencyBubbleObserver:
  void OnBubbleShown() override;
  void OnBubbleHidden() override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  // performance_manager::user_tuning::UserPerformanceTuningManager::Observer:
  // Checks whether high efficiency mode is currently enabled.
  void OnHighEfficiencyModeChanged() override;

  bool ShouldHighlightMemorySavingsWithExpandedChip(
      HighEfficiencyChipTabHelper* high_efficiency_tab_helper,
      PrefService* pref_service);

  const raw_ptr<Browser> browser_;
  const std::u16string chip_accessible_label_;
  base::OneShotTimer timer_;
  raw_ptr<views::BubbleDialogModelHost> bubble_ = nullptr;
  base::ScopedObservation<
      performance_manager::user_tuning::UserPerformanceTuningManager,
      performance_manager::user_tuning::UserPerformanceTuningManager::Observer>
      user_performance_tuning_manager_observation_{this};
  bool is_high_efficiency_mode_enabled_ = false;
  base::WeakPtrFactory<HighEfficiencyChipView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_CHIP_VIEW_H_
