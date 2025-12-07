// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/collaboration_messaging_page_action_icon_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

using collaboration::messaging::ActivityLogItem;
using collaboration::messaging::ActivityLogQueryParams;
using collaboration::messaging::CollaborationEvent;

CollaborationMessagingPageActionIconView::
    CollaborationMessagingPageActionIconView(
        Browser* browser,
        IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
        PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "CollaborationMessaging"),
      profile_(browser->profile()) {
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(false);
  SetProperty(views::kElementIdentifierKey,
              kCollaborationMessagingPageActionIconElementId);
  SetUseTonalColorsWhenExpanded(true);
  SetBackgroundVisibility(BackgroundVisibility::kAlways);
}

CollaborationMessagingPageActionIconView::
    ~CollaborationMessagingPageActionIconView() = default;

void CollaborationMessagingPageActionIconView::UpdateImpl() {
  // Get the current tab data.
  auto* tab_data = GetCollaborationTabData();
  if (tab_data) {
    // Set a weak pointer to the current tab data. This will be used to get the
    // icon when the page action needs it.
    collaboration_messaging_tab_data_ = tab_data->GetWeakPtr();

    // If the message changes, call this function again to update the page
    // action.
    message_changed_callback_ =
        tab_data->RegisterMessageChangedCallback(base::BindRepeating(
            &CollaborationMessagingPageActionIconView::UpdateImpl,
            base::Unretained(this)));
  } else {
    // Reset the callback.
    message_changed_callback_ = {};
  }

  UpdateContent(tab_data);
}

CollaborationMessagingTabData*
CollaborationMessagingPageActionIconView::GetCollaborationTabData() const {
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  auto* tab = tabs::TabInterface::GetFromContents(web_contents);
  if (!tab || !tab->GetTabFeatures()) {
    return nullptr;
  }

  return tab->GetTabFeatures()->collaboration_messaging_tab_data();
}

void CollaborationMessagingPageActionIconView::UpdateContent(
    CollaborationMessagingTabData* collaboration_messaging_tab_data) {
  bool should_show_page_action = collaboration_messaging_tab_data &&
                                 collaboration_messaging_tab_data->HasMessage();
  if (!should_show_page_action) {
    SetVisible(false);
    return;
  }

  std::u16string label_text;
  switch (collaboration_messaging_tab_data->collaboration_event()) {
    case CollaborationEvent::TAB_ADDED:
      label_text =
          l10n_util::GetStringUTF16(IDS_DATA_SHARING_PAGE_ACTION_ADDED_NEW_TAB);
      break;
    case CollaborationEvent::TAB_UPDATED:
      label_text =
          l10n_util::GetStringUTF16(IDS_DATA_SHARING_PAGE_ACTION_CHANGED_TAB);
      break;
    default:
      // CHIP messages should only be one of these 2 types.
      NOTREACHED();
  }

  // If the label text is empty, there is nothing to show.
  if (label_text.empty()) {
    SetVisible(false);
    return;
  }

  SetVisible(true);
  SetLabel(label_text);
  label()->SetVisible(true);
  UpdateIconImage();
}

tab_groups::LocalTabGroupID
CollaborationMessagingPageActionIconView::GetGroupId() {
  auto* tab = tabs::TabInterface::GetFromContents(GetWebContents());
  auto group = tab->GetGroup();
  CHECK(group.has_value());
  return group.value();
}

void CollaborationMessagingPageActionIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  // Safe to assume TabInterface and BrowserWindowInterface are initialized by
  // the time user clicks on the page action chip.
  auto* tab = tabs::TabInterface::GetFromContents(GetWebContents());
  CHECK(tab);

  auto* bubble_coordinator =
      RecentActivityBubbleCoordinator::From(tab->GetBrowserWindowInterface());
  CHECK(bubble_coordinator);

  bubble_coordinator->ShowForCurrentTab(
      this, GetWebContents(),
      tab_groups::SavedTabGroupUtils::GetRecentActivity(
          profile_, GetGroupId(), tab->GetHandle().raw_value()),
      tab_groups::SavedTabGroupUtils::GetRecentActivity(profile_, GetGroupId()),
      profile_);
}

views::BubbleDialogDelegate*
CollaborationMessagingPageActionIconView::GetBubble() const {
  // This method gets called during Browser startup, before WebContents,
  // TabInterface and BrowserWindowInterface gets initialized.
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  auto* tab = tabs::TabInterface::GetFromContents(web_contents);
  if (!tab) {
    return nullptr;
  }

  auto* bubble_coordinator =
      RecentActivityBubbleCoordinator::From(tab->GetBrowserWindowInterface());
  if (!bubble_coordinator) {
    return nullptr;
  }

  return bubble_coordinator->GetBubble();
}

const gfx::VectorIcon& CollaborationMessagingPageActionIconView::GetVectorIcon()
    const {
  // In practice, this should never be used since we use a fallback icon
  // when the avatar is unavailable.
  return kPersonFilledPaddedSmallIcon;
}

ui::ImageModel CollaborationMessagingPageActionIconView::GetSizedIconImage(
    int icon_size) const {
  if (!collaboration_messaging_tab_data_) {
    return ui::ImageModel();
  }
  return collaboration_messaging_tab_data_->GetPageActionImage(GetWidget());
}

BEGIN_METADATA(CollaborationMessagingPageActionIconView)
END_METADATA
