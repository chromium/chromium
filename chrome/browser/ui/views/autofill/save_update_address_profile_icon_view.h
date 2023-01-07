// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

class CommandUpdater;

namespace autofill {

class SaveUpdateAddressProfileIconController;

// The location bar icon to show the Save Address Profile bubble.
class SaveUpdateAddressProfileIconView : public PageActionIconView {
 public:
  SaveUpdateAddressProfileIconView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  SaveUpdateAddressProfileIconView(const SaveUpdateAddressProfileIconView&) =
      delete;
  SaveUpdateAddressProfileIconView& operator=(
      const SaveUpdateAddressProfileIconView&) = delete;
  ~SaveUpdateAddressProfileIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  SaveUpdateAddressProfileIconController* GetController() const;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_ICON_VIEW_H_
