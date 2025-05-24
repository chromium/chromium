// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_MEMORY_SAVER_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_MEMORY_SAVER_CHIP_VIEW_H_

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

// Represents the memory saver page action chip that appears on previously
// discarded tabs.
class MemorySaverChipView : public PageActionIconView,
                            public MemorySaverBubbleObserver {
  METADATA_HEADER(MemorySaverChipView, PageActionIconView)

 public:
  // The number of times a user should see the expanded chip.
  static constexpr int kChipAnimationCount = 3;

  MemorySaverChipView(CommandUpdater* command_updater,
                      Browser* browser,
                      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                      PageActionIconView::Delegate* page_action_icon_delegate);
  MemorySaverChipView(const MemorySaverChipView&) = delete;
  MemorySaverChipView& operator=(const MemorySaverChipView&) = delete;
  ~MemorySaverChipView() override;

  // MemorySaverBubbleObserver:
  void OnBubbleShown() override;
  void OnBubbleHidden() override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  const raw_ptr<Browser> browser_;
  const std::u16string chip_accessible_label_;
  raw_ptr<views::BubbleDialogModelHost> bubble_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_MEMORY_SAVER_CHIP_VIEW_H_
