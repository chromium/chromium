// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_media_toolbar_button.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
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
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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

void WebUIMediaToolbarButton::OnClicked() {
  if (!service_) {
    return;
  }

  if (MediaDialogView::IsShowing()) {
    MediaDialogView::HideDialog();
  } else {
    ShowBubble();
  }
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
  if (should_be_shown_) {
    return;
  }
  should_be_shown_ = true;
  UpdateState();
  delegate_->OnPreferredSizeChanged();
}

void WebUIMediaToolbarButton::Hide() {
  CancelPendingShowBubble();
  if (!should_be_shown_) {
    return;
  }
  should_be_shown_ = false;
  UpdateState();
  delegate_->OnPreferredSizeChanged();
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

void WebUIMediaToolbarButton::OnWidgetDestroying(views::Widget* widget) {
  dialog_widget_observation_.Reset();
  // Let the WebUI know the button no longer needs to be displayed.
  UpdateState();
}

void WebUIMediaToolbarButton::ShowBubble() {
  // Consider this call to fulfill any pending attempt to show the dialog,
  // though if the button still isn't visible, it will subscribe again below.
  // Every path below calls UpdateState(), in case clearing
  // `pending_show_bubble_` changed `prevent_overflow`.
  button_shown_subscription_ = {};
  pending_show_bubble_ = false;

  BrowserElements* const elements =
      BrowserElements::From(delegate_->GetBrowser());
  // The BrowserElements class is owned by the Browser object, so should always
  // be non-null, as long as the Browser still exists. On browser window
  // deletion, this button should be torn down, too, so `elements` should never
  // be nullptr.
  CHECK(elements);
  ui::TrackedElement* element =
      elements->GetElement(kToolbarMediaButtonElementId);

  // If the button is hidden, have to set `pending_show_bubble_` to true and
  // subscribe to a notification for when it is visible.
  if (!element) {
    pending_show_bubble_ = true;
    button_shown_subscription_ =
        ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
            kToolbarMediaButtonElementId, elements->GetContext(),
            base::BindRepeating(
                &WebUIMediaToolbarButton::OnButtonShownWithPendingShowBubble,
                base::Unretained(this)));
    // Push the updated state, which now has `prevent_overflow` set. That will
    // result in the button being displayed, and then the dialog being shown.
    UpdateState();
    return;
  }

  // This shouldn't be necessary, since OnClicked() won't call this if there's
  // already a dialog open, but clear `dialog_widget_observation_`, just to be
  // safe.
  dialog_widget_observation_.Reset();

  views::Widget* const widget = MediaDialogView::ShowDialogFromToolbar(
      views::BubbleAnchor(element), service_,
      delegate_->GetBrowser()->GetProfile());
  dialog_widget_observation_.Observe(widget);

  // Note that this is deliberately not done above, when the button is still
  // hidden: this method is invoked a second time once the button becomes
  // visible, and notifying that the feature was used twice per click would
  // double-count it in the feature engagement backend.
  ClosePromoBubble(/*engaged=*/true);

  // Since `dialog_widget_observation_` has changed, need to call UpdateState()
  // to pass the updated value of `prevent_overflow` to the renderer.
  UpdateState();
}

void WebUIMediaToolbarButton::OnButtonShownWithPendingShowBubble(
    ui::TrackedElement* element) {
  // This call should have been cancelled if `pending_show_bubble_` became false
  // before it was invoked.
  CHECK(pending_show_bubble_);
  // ShowBubble() will clear `pending_show_bubble_`, and is guaranteed to find
  // `element` when it looks for it, since the ElementTracker registers elements
  // before notifying observers that they've been shown. So this can't get into
  // an infinite loop of re-subscribing.
  ShowBubble();
}

void WebUIMediaToolbarButton::CancelPendingShowBubble() {
  button_shown_subscription_ = {};
  if (std::exchange(pending_show_bubble_, false)) {
    UpdateState();
  }
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
  if (should_be_shown_) {
    // Note that this is set while the dialog is merely pending, to cause the
    // button to be displayed so the dialog can then be anchored to it.
    state->prevent_overflow =
        pending_show_bubble_ || dialog_widget_observation_.IsObserving();
  }
  delegate_->OnMediaControlStateChanged(std::move(state));
}
