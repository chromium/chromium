// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/send_tab_to_self/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/strings/grit/ui_strings.h"

namespace send_tab_to_self {

SendTabToSelfIconView::SendTabToSelfIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_SEND_TAB_TO_SELF,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "SendTabToSelf") {
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_ICON_SEND_TAB_TO_SELF));
  SetUpForInOutAnimation();
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(IDS_OMNIBOX_TOOLTIP_SEND_TAB_TO_SELF));
}

SendTabToSelfIconView::~SendTabToSelfIconView() {}

views::BubbleDialogDelegate* SendTabToSelfIconView::GetBubble() const {
  SendTabToSelfBubbleController* controller = GetController();
  if (!controller) {
    return nullptr;
  }

  return controller->send_tab_to_self_bubble_view();
}

void SendTabToSelfIconView::UpdateImpl() {
  content::WebContents* web_contents = GetWebContents();
  if (!send_tab_to_self::ShouldOfferOmniboxIcon(web_contents)) {
    return;
  }

  const OmniboxView* omnibox_view = delegate()->GetOmniboxView();
  if (!omnibox_view) {
    return;
  }

  // The bubble is anchored to the sharing hub icon when the sharing hub is
  // enabled, so this icon is no longer required.
  if (sharing_hub::SharingHubOmniboxEnabled(
          web_contents->GetBrowserContext())) {
    SetVisible(false);
    return;
  }

  SendTabToSelfBubbleController* controller = GetController();
  if (!is_animating_label() && !omnibox_view->model()->has_focus()) {
    sending_animation_state_ = AnimationState::kNotShown;
  }
  if (controller && controller->show_message()) {
    if (!GetVisible()) {
      SetVisible(true);
    }
    controller->set_show_message(false);
    if (initial_animation_state_ == AnimationState::kShowing &&
        label()->GetVisible()) {
      initial_animation_state_ = AnimationState::kShown;
      SetLabel(
          l10n_util::GetStringUTF16(IDS_BROWSER_SHARING_OMNIBOX_SENDING_LABEL));
    } else {
      AnimateIn(IDS_BROWSER_SHARING_OMNIBOX_SENDING_LABEL);
    }
    sending_animation_state_ = AnimationState::kShowing;
  }
  if (!GetVisible() && omnibox_view->model()->has_focus() &&
      !omnibox_view->model()->user_input_in_progress()) {
    // Shows the "Send" animation once per profile.
    if (controller && !controller->InitialSendAnimationShown() &&
        initial_animation_state_ == AnimationState::kNotShown) {
      // Set label ahead of time to avoid announcing a useless alert (i.e.
      // "alert Send") to screenreaders.
      SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_ICON_SEND_TAB_TO_SELF));
      AnimateIn(absl::nullopt);
      initial_animation_state_ = AnimationState::kShowing;
      controller->SetInitialSendAnimationShown(true);
    }
    if (sending_animation_state_ == AnimationState::kNotShown) {
      SetVisible(true);
    }
  }
}

void SendTabToSelfIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& SendTabToSelfIconView::GetVectorIcon() const {
  return OmniboxFieldTrial::IsChromeRefreshIconsEnabled()
             ? kDevicesChromeRefreshIcon
             : kDevicesIcon;
}

SendTabToSelfBubbleController* SendTabToSelfIconView::GetController() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  return SendTabToSelfBubbleController::CreateOrGetFromWebContents(
      web_contents);
}

void SendTabToSelfIconView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (sending_animation_state_ == AnimationState::kShowing) {
    UpdateOpacity();
  }
  return PageActionIconView::AnimationProgressed(animation);
}

void SendTabToSelfIconView::AnimationEnded(const gfx::Animation* animation) {
  PageActionIconView::AnimationEnded(animation);
  initial_animation_state_ = AnimationState::kShown;
  if (sending_animation_state_ == AnimationState::kShowing) {
    UpdateOpacity();
    SetVisible(false);
    sending_animation_state_ = AnimationState::kShown;
  }
}

void SendTabToSelfIconView::UpdateOpacity() {
  if (!GetVisible()) {
    ResetSlideAnimation(false);
  }
  if (!IsShrinking()) {
    DestroyLayer();
    SetTextSubpixelRenderingEnabled(true);
    return;
  }

  if (!layer()) {
    SetPaintToLayer();
    SetTextSubpixelRenderingEnabled(false);
    layer()->SetFillsBoundsOpaquely(false);
  }

  // Large enough number so that there is enough granularity (1/kLargeNumber) in
  // the opacity as the animation shrinks.
  int kLargeNumber = 100;
  layer()->SetOpacity(GetWidthBetween(0, kLargeNumber) /
                      static_cast<float>(kLargeNumber));
}

BEGIN_METADATA(SendTabToSelfIconView, PageActionIconView)
END_METADATA

}  // namespace send_tab_to_self
