// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

// A plus icon to surface whether a site has passed PWA (progressive web app)
// installability checks and can be installed.
class PwaInstallView : public PageActionIconView {
 public:
  explicit PwaInstallView(CommandUpdater* command_updater,
                          PageActionIconView::Delegate* delegate);
  ~PwaInstallView() override;

 protected:
  // PageActionIconView:
  bool Update() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  views::BubbleDialogDelegateView* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PwaInstallView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_VIEW_H_
