// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_FIND_BAR_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_FIND_BAR_ICON_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

class Browser;

// The find icon to show when the find bar is visible.
class FindBarIcon : public PageActionIconView {
 public:
  FindBarIcon(Browser* browser,
              IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
              PageActionIconView::Delegate* page_action_icon_delegate);
  ~FindBarIcon() override;

  void SetActive(bool activate, bool should_animate);

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(ExecuteSource execute_source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FindBarIcon);

  Browser* browser_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_FIND_BAR_ICON_H_
