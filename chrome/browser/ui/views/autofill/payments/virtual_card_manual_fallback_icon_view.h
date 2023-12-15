// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class CommandUpdater;

namespace autofill {

class VirtualCardManualFallbackBubbleController;

// The icon to show the virtual card manual fallback bubble after the user has
// selected the virtual card to use and the information has been sent to Chrome.
class VirtualCardManualFallbackIconView : public PageActionIconView {
  METADATA_HEADER(VirtualCardManualFallbackIconView, PageActionIconView)

 public:
  VirtualCardManualFallbackIconView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* delegate);
  ~VirtualCardManualFallbackIconView() override;
  VirtualCardManualFallbackIconView(const VirtualCardManualFallbackIconView&) =
      delete;
  VirtualCardManualFallbackIconView& operator=(
      const VirtualCardManualFallbackIconView&) = delete;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  VirtualCardManualFallbackBubbleController* GetController() const;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_ICON_VIEW_H_
