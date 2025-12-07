// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_page_action_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"

namespace {

const std::u16string GetLabelText(
    collaboration::messaging::CollaborationEvent event) {
  switch (event) {
    case collaboration::messaging::CollaborationEvent::TAB_ADDED:
      return l10n_util::GetStringUTF16(
          IDS_DATA_SHARING_PAGE_ACTION_ADDED_NEW_TAB);
    case collaboration::messaging::CollaborationEvent::TAB_UPDATED:
      return l10n_util::GetStringUTF16(
          IDS_DATA_SHARING_PAGE_ACTION_CHANGED_TAB);
    default:
      // Chip messages should only be one of these 2 types.
      NOTREACHED();
  }
}

}  // namespace

DEFINE_USER_DATA(CollaborationMessagingPageActionController);

CollaborationMessagingPageActionController::
    CollaborationMessagingPageActionController(
        tabs::TabInterface& tab,
        page_actions::PageActionController& page_action_controller,
        tab_groups::CollaborationMessagingTabData&
            collaboration_messaging_tab_data)
    : page_actions_controller_(page_action_controller),
      collaboration_messaging_tab_data_(collaboration_messaging_tab_data),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  CHECK(IsPageActionMigrated(PageActionIconType::kCollaborationMessaging));

  tab_activated_subscription_ = tab.RegisterDidActivate(base::BindRepeating(
      &CollaborationMessagingPageActionController::HandleUpdate,
      base::Unretained(this)));

  tab_data_updated_subscription_ =
      collaboration_messaging_tab_data.RegisterMessageChangedCallback(
          base::BindRepeating(
              &CollaborationMessagingPageActionController::HandleUpdate,
              base::Unretained(this), base::Unretained(&tab)));
}

CollaborationMessagingPageActionController::
    ~CollaborationMessagingPageActionController() = default;

// static
CollaborationMessagingPageActionController*
CollaborationMessagingPageActionController::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

void CollaborationMessagingPageActionController::HandleUpdate(
    tabs::TabInterface* tab) {
  // Don't try to show page action on an inactive tab - this will be handled by
  // the tab activated callback.
  if (!tab->IsActivated()) {
    return;
  }

  const bool should_show_page_action =
      collaboration_messaging_tab_data_->HasMessage();

  if (!should_show_page_action) {
    Hide();
    return;
  }

  content::WebContents* web_contents = tab->GetContents();
  CHECK(web_contents);

  const ui::ColorProvider& color_provider = web_contents->GetColorProvider();

  content::RenderWidgetHostView* render_widget_host_view =
      web_contents->GetRenderWidgetHostView();
  const float scale_factor =
      render_widget_host_view ? render_widget_host_view->GetDeviceScaleFactor()
                              : 1.0f;

  const ui::ImageModel& image =
      collaboration_messaging_tab_data_->GetPageActionImage(scale_factor,
                                                            &color_provider);
  collaboration::messaging::CollaborationEvent event =
      collaboration_messaging_tab_data_->collaboration_event();
  const std::u16string& label_text = GetLabelText(event);

  Show(label_text, image);
}

void CollaborationMessagingPageActionController::Hide() {
  page_actions_controller_->HideSuggestionChip(
      kActionShowCollaborationRecentActivity);
  page_actions_controller_->Hide(kActionShowCollaborationRecentActivity);

  page_actions_controller_->ClearOverrideText(
      kActionShowCollaborationRecentActivity);
  page_actions_controller_->ClearOverrideTooltip(
      kActionShowCollaborationRecentActivity);

  page_actions_controller_->ClearOverrideImage(
      kActionShowCollaborationRecentActivity);
}

void CollaborationMessagingPageActionController::Show(
    const std::u16string& label_text,
    const ui::ImageModel& avatar) {
  page_actions_controller_->OverrideImage(
      kActionShowCollaborationRecentActivity, avatar);

  page_actions_controller_->OverrideText(kActionShowCollaborationRecentActivity,
                                         label_text);
  page_actions_controller_->OverrideTooltip(
      kActionShowCollaborationRecentActivity, label_text);

  page_actions_controller_->Show(kActionShowCollaborationRecentActivity);
  page_actions_controller_->ShowSuggestionChip(
      kActionShowCollaborationRecentActivity);
}
