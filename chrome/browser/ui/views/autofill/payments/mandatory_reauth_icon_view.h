// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class CommandUpdater;

namespace autofill {

class MandatoryReauthBubbleController;

class MandatoryReauthIconView : public PageActionIconView {
  METADATA_HEADER(MandatoryReauthIconView, PageActionIconView)

 public:
  MandatoryReauthIconView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  MandatoryReauthIconView(const MandatoryReauthIconView&) = delete;
  MandatoryReauthIconView& operator=(const MandatoryReauthIconView&) = delete;
  ~MandatoryReauthIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  MandatoryReauthBubbleController* GetController() const;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_ICON_VIEW_H_
