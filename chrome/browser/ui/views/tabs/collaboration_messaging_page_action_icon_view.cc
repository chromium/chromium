// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/collaboration_messaging_page_action_icon_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
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
  auto* tab_data = GetCollaborationTabData();
  bool should_show_page_action = tab_data && tab_data->HasMessage();

  if (should_show_page_action) {
    UpdateContent(tab_data);
  }

  if (tab_data) {
    message_changed_callback_ =
        tab_data->RegisterMessageChangedCallback(base::BindRepeating(
            &CollaborationMessagingPageActionIconView::UpdateImpl,
            base::Unretained(this)));
  } else {
    message_changed_callback_ = {};
  }

  SetVisible(should_show_page_action);
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

  // Label is always visible.
  SetLabel(label_text);
  label()->SetVisible(true);

  avatar_image_ = collaboration_messaging_tab_data->page_action_avatar();
  UpdateIconImage();
}

std::vector<ActivityLogItem>
CollaborationMessagingPageActionIconView::GetActivityLog() {
  auto* tab_group_sync_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile_);
  CHECK(tab_group_sync_service);
  auto* messaging_service =
      collaboration::messaging::MessagingBackendServiceFactory::GetForProfile(
          profile_);
  CHECK(messaging_service);

  auto* tab = tabs::TabInterface::GetFromContents(GetWebContents());
  auto group = tab->GetGroup();
  CHECK(group.has_value());

  auto saved_tab_group = tab_group_sync_service->GetGroup(group.value());
  CHECK(saved_tab_group);
  auto collaboration_id = saved_tab_group->collaboration_id();

  // A message should never be delivered to a tab that is not shared.
  CHECK(collaboration_id.has_value());

  ActivityLogQueryParams activity_log_params;
  activity_log_params.result_length =
      RecentActivityBubbleDialogView::kMaxNumberRows;
  activity_log_params.collaboration_id =
      data_sharing::GroupId(collaboration_id.value().value());

  return messaging_service->GetActivityLog(activity_log_params);
}

void CollaborationMessagingPageActionIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  bubble_coordinator_.Show(this, GetWebContents(), GetActivityLog(), profile_);
}

views::BubbleDialogDelegate*
CollaborationMessagingPageActionIconView::GetBubble() const {
  return bubble_coordinator_.GetBubble();
}

const gfx::VectorIcon& CollaborationMessagingPageActionIconView::GetVectorIcon()
    const {
  return kPersonFilledPaddedSmallIcon;
}

ui::ImageModel CollaborationMessagingPageActionIconView::GetSizedIconImage(
    int icon_size) const {
  return avatar_image_;
}

BEGIN_METADATA(CollaborationMessagingPageActionIconView)
END_METADATA
