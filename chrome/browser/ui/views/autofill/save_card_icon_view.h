// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

class Browser;
class CommandUpdater;

namespace autofill {

class SaveCardBubbleControllerImpl;

// The location bar icon to show the Save Credit Card bubble where the user can
// choose to save the credit card info to use again later without re-entering
// it.
class SaveCardIconView : public PageActionIconView {
 public:
  SaveCardIconView(CommandUpdater* command_updater,
                   Browser* browser,
                   PageActionIconView::Delegate* delegate,
                   const gfx::FontList& font_list);
  ~SaveCardIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegateView* GetBubble() const override;
  bool Update() override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;
  bool ShouldShowSeparator() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  friend class SaveCardBubbleViewsBrowserTestBase;

  SaveCardBubbleControllerImpl* GetController() const;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;

  // May be nullptr.
  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardIconView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_ICON_VIEW_H_
