// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_ICON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;

// A search icon that appears when the side search feature is available. Opens
// the side panel to the available SRP.
class SideSearchIconView : public PageActionIconView,
                           public TemplateURLServiceObserver {
 public:
  METADATA_HEADER(SideSearchIconView);
  explicit SideSearchIconView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate,
      Browser* browser);
  SideSearchIconView(const SideSearchIconView&) = delete;
  SideSearchIconView& operator=(const SideSearchIconView&) = delete;
  ~SideSearchIconView() override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  ui::ImageModel GetSizedIconImage(int size) const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

 private:
  // Callback used for when `GetSizedIconImage()` does not return the icon image
  // immediately but instead fetches the image asynchronously.
  void OnIconFetched(const gfx::Image& icon);

  raw_ptr<Browser> browser_ = nullptr;

  // The ID of the current default TemplateURL instance. Keep track of this so
  // we update the page action's favicon only when the default instance changes.
  TemplateURLID default_template_url_id_ = kInvalidTemplateURLID;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};

  base::WeakPtrFactory<SideSearchIconView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_ICON_VIEW_H_
