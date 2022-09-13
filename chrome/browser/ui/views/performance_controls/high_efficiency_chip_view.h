// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_CHIP_VIEW_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

// Represents the high efficiency page action chip that appears on previously
// discarded tabs.
class HighEfficiencyChipView : public PageActionIconView,
                               public HighEfficiencyBubbleObserver {
 public:
  METADATA_HEADER(HighEfficiencyChipView);
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
  std::u16string GetTextForTooltipAndAccessibleName() const override;

 private:
  const raw_ptr<Browser> browser_;
  raw_ptr<views::BubbleDialogModelHost> bubble_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_CHIP_VIEW_H_
