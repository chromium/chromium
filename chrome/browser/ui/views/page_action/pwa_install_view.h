// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

namespace webapps {
class AppBannerManager;
}  // namespace webapps

// A plus icon to surface whether a site has passed PWA (progressive web app)
// installability checks and can be installed.
class PwaInstallView : public PageActionIconView {
 public:
  explicit PwaInstallView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  ~PwaInstallView() override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;
  const char* GetClassName() const override;

 private:
  // Called when IPH is closed.
  void OnIphClosed();

  // Track whether IPH is closed because of install icon being clicked.
  bool install_icon_clicked_after_iph_shown_ = false;

  // Decide whether IPH promo should be shown based on previous interactions.
  bool ShouldShowIph(content::WebContents* web_contents,
                     webapps::AppBannerManager* manager);

  base::WeakPtrFactory<PwaInstallView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PwaInstallView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_VIEW_H_
