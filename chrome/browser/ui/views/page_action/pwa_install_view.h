// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/user_education/common/feature_promo_result.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;

namespace webapps {
struct WebAppBannerData;
}  // namespace webapps

// A plus icon to surface whether a site has passed PWA (progressive web app)
// installability checks and can be installed.
class PwaInstallView : public PageActionIconView, public TabStripModelObserver {
  METADATA_HEADER(PwaInstallView, PageActionIconView)

 public:
  PwaInstallView(CommandUpdater* command_updater,
                 IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                 PageActionIconView::Delegate* page_action_icon_delegate,
                 Browser* browser);
  PwaInstallView(const PwaInstallView&) = delete;
  PwaInstallView& operator=(const PwaInstallView&) = delete;
  ~PwaInstallView() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  // Called when the IPH is shown.
  void OnIphShown(user_education::FeaturePromoResult result);

  // Called when IPH is closed.
  void OnIphClosed(const webapps::WebAppBannerData& data);

  // Whether the IPH is trying to show.
  bool iph_pending_ = false;

  // Track whether IPH is closed because of install icon being clicked.
  bool install_icon_clicked_after_iph_shown_ = false;

  // Decide whether IPH promo should be shown based on previous interactions.
  bool ShouldShowIph(content::WebContents* web_contents,
                     const webapps::WebAppBannerData& data);

  raw_ptr<Browser> browser_ = nullptr;
  base::WeakPtrFactory<PwaInstallView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PWA_INSTALL_VIEW_H_
