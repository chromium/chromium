// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_COLLABORATION_MESSAGING_PAGE_ACTION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_COLLABORATION_MESSAGING_PAGE_ACTION_ICON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/saved_tab_groups/public/types.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;

namespace tab_groups {
class CollaborationMessagingTabData;
}  // namespace tab_groups

using tab_groups::CollaborationMessagingTabData;

class CollaborationMessagingPageActionIconView : public PageActionIconView {
  METADATA_HEADER(CollaborationMessagingPageActionIconView, PageActionIconView)

 public:
  CollaborationMessagingPageActionIconView(
      Browser* browser,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  CollaborationMessagingPageActionIconView(
      const CollaborationMessagingPageActionIconView&) = delete;
  CollaborationMessagingPageActionIconView& operator=(
      const CollaborationMessagingPageActionIconView&) = delete;
  ~CollaborationMessagingPageActionIconView() override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  ui::ImageModel GetSizedIconImage(int size) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      CollaborationMessagingPageActionIconViewInteractiveTest,
      ReactsToChangesInTabData);

  tab_groups::LocalTabGroupID GetGroupId();

  // Helper method to get the collaboration data for the current tab.
  CollaborationMessagingTabData* GetCollaborationTabData() const;

  // Populates the PageAction with content for the current message.
  void UpdateContent(
      CollaborationMessagingTabData* collaboration_messaging_tab_data);

  raw_ptr<Profile> profile_ = nullptr;
  base::WeakPtr<CollaborationMessagingTabData>
      collaboration_messaging_tab_data_ = nullptr;
  base::CallbackListSubscription message_changed_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_COLLABORATION_MESSAGING_PAGE_ACTION_ICON_VIEW_H_
