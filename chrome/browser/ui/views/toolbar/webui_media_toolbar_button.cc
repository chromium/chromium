// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_media_toolbar_button.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

WebUIMediaToolbarButton::WebUIMediaToolbarButton(
    WebUIToolbarControlDelegate* delegate)
    : delegate_(delegate) {}

WebUIMediaToolbarButton::~WebUIMediaToolbarButton() = default;

void WebUIMediaToolbarButton::Init() {
  service_ = MediaNotificationServiceFactory::GetForProfile(
      delegate_->GetBrowser()->GetProfile());
  if (service_) {
    controller_ = std::make_unique<MediaToolbarButtonController>(
        this, service_->media_item_manager());
  }
}

void WebUIMediaToolbarButton::OnClicked(bool is_mouse_interaction) {
  if (!service_) {
    return;
  }

  const bool suppress =
      reopen_suppressor_.ShouldSuppressBubbleShow(is_mouse_interaction);

  if (MediaDialogView::IsShowing()) {
    MediaDialogView::HideDialog();
  } else if (!suppress) {
    reopen_suppressor_.Observe(MediaDialogView::ShowDialogFromToolbar(
        GetBubbleAnchor(), service_, delegate_->GetBrowser()->GetProfile()));
    ClosePromoBubble(/*engaged=*/true);
  }
}

void WebUIMediaToolbarButton::OnMousePressed() {
  reopen_suppressor_.OnMousePressed();
}

void WebUIMediaToolbarButton::HandleContextMenu(
    const gfx::Rect& screen_rect,
    ui::mojom::MenuSourceType source) {
  if (!context_menu_) {
    context_menu_ = std::make_unique<MediaToolbarButtonContextualMenu>(
        delegate_->GetBrowser()->GetProfile());
  }
  if (menu_runner_ && menu_runner_->IsRunning()) {
    menu_runner_->Cancel();
  }
  menu_model_ = context_menu_->CreateMenuModel();
  if (!menu_model_) {
    return;
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS,
      base::BindRepeating(&WebUIMediaToolbarButton::UpdateState,
                          base::Unretained(this)));
  menu_runner_->RunMenuAt(delegate_->GetView()->GetWidget(), nullptr,
                          screen_rect, views::MenuAnchorPosition::kTopLeft,
                          source);
  UpdateState();
}

void WebUIMediaToolbarButton::Show() {
  should_be_shown_ = true;
  UpdateState();
}

void WebUIMediaToolbarButton::Hide() {
  should_be_shown_ = false;
  UpdateState();
}

void WebUIMediaToolbarButton::Enable() {
  enabled_ = true;
  UpdateState();
}

void WebUIMediaToolbarButton::Disable() {
  enabled_ = false;
  ClosePromoBubble(/*engaged=*/false);
  UpdateState();
}

void WebUIMediaToolbarButton::MaybeShowLocalMediaCastingPromo() {
  if (service_ && service_->should_show_cast_local_media_iph()) {
    BrowserUserEducationInterface::From(delegate_->GetBrowser())
        ->MaybeShowFeaturePromo(
            feature_engagement::kIPHGMCLocalMediaCastingFeature);
  }
}

void WebUIMediaToolbarButton::MaybeShowStopCastingPromo() {
  if (service_ && service_->HasLocalCastNotifications()) {
    BrowserUserEducationInterface::From(delegate_->GetBrowser())
        ->MaybeShowFeaturePromo(
            feature_engagement::kIPHGMCCastStartStopFeature);
  }
}

views::BubbleAnchor WebUIMediaToolbarButton::GetBubbleAnchor() {
  BrowserElements* elements = BrowserElements::From(delegate_->GetBrowser());
  ui::TrackedElement* button =
      elements ? elements->GetElement(kToolbarMediaButtonElementId) : nullptr;
  return button ? views::BubbleAnchor(button)
                : views::BubbleAnchor(delegate_->GetView());
}

MediaToolbarButtonController* WebUIMediaToolbarButton::GetController() {
  return controller_.get();
}

void WebUIMediaToolbarButton::ClosePromoBubble(bool engaged) {
  if (auto* const user_education =
          BrowserUserEducationInterface::From(delegate_->GetBrowser())) {
    if (engaged) {
      user_education->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHGMCCastStartStopFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    } else {
      user_education->AbortFeaturePromo(
          feature_engagement::kIPHGMCCastStartStopFeature);
    }
  }
}

void WebUIMediaToolbarButton::UpdateState() {
  auto state = toolbar_ui_api::mojom::MediaControlState::New();
  state->enabled = enabled_;
  state->should_be_shown = should_be_shown_;
  state->is_context_menu_visible = menu_runner_ && menu_runner_->IsRunning();
  delegate_->OnMediaControlStateChanged(std::move(state));
}
