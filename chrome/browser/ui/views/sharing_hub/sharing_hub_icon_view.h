// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class CommandUpdater;

namespace sharing_hub {

class SharingHubBubbleController;

// The location bar icon to show the Sharing Hub bubble, where the user can
// choose to share the current page to a sharing target or save the page using
// first party actions.
class SharingHubIconView : public PageActionIconView {
  METADATA_HEADER(SharingHubIconView, PageActionIconView)

 public:
  SharingHubIconView(CommandUpdater* command_updater,
                     IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                     PageActionIconView::Delegate* page_action_icon_delegate);
  SharingHubIconView(const SharingHubIconView&) = delete;
  SharingHubIconView& operator=(const SharingHubIconView&) = delete;
  ~SharingHubIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  SharingHubBubbleController* GetController() const;
  // Shows a "Sending..." animation if a device was selected in the send tab to
  // self dialog.
  void MaybeAnimateSendingToast();
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_ICON_VIEW_H_
