// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/overlay_base_controller.h"

#include "chrome/browser/ui/lens/lens_preselection_bubble.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "net/base/network_change_notifier.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

OverlayBaseController::OverlayBaseController(tabs::TabInterface* tab,
                                             PrefService* pref_service)
    : tab_(tab), pref_service_(pref_service) {}

OverlayBaseController::~OverlayBaseController() {
  state_ = State::kOff;
}

bool OverlayBaseController::IsOverlayShowing() const {
  return state_ == State::kStartingWebUI || state_ == State::kOverlay;
}

bool OverlayBaseController::IsOverlayActive() const {
  return IsOverlayShowing() || state_ == State::kHidden ||
         state_ == State::kHiding || state_ == State::kIsReshowing;
}

bool OverlayBaseController::IsOverlayInitializing() {
  return state_ == State::kStartingWebUI || state_ == State::kScreenshot ||
         state_ == State::kClosingOpenedSidePanel;
}

bool OverlayBaseController::IsOverlayClosing() {
  return state_ == State::kClosing;
}

void OverlayBaseController::OnViewBoundsChanged(views::View* observed_view) {
  CHECK(observed_view == overlay_view_);

  // Set our view to the same bounds as the contents web view so it always
  // covers the tab contents.
  if (lens_overlay_blur_layer_delegate_) {
    // Set the blur to have the same bounds as our view, but since it is in our
    // views local coordinate system, the blur should be positioned at (0,0).
    lens_overlay_blur_layer_delegate_->layer()->SetBounds(
        overlay_view_->GetLocalBounds());
  }
}

#if BUILDFLAG(IS_MAC)
void OverlayBaseController::OnWidgetActivationChanged(views::Widget* widget,
                                                      bool active) {
  if (active && preselection_widget_) {
    // On Mac, traversing out of the preselection widget into the browser causes
    // the browser to restore its focus to the wrong place. Thus, when entering
    // the preselection widget, make sure to clear out the browser's native
    // focus. This causes the preselection widget to lose activation, so
    // reactivate it manually.
    BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
        ->GetPrimaryWindowWidget()
        ->GetFocusManager()
        ->ClearNativeFocus();
    preselection_widget_->Activate();
  }
}
#endif

void OverlayBaseController::OnWidgetDestroying(views::Widget* widget) {
  preselection_widget_ = nullptr;
  preselection_widget_observer_.Reset();
}

void OverlayBaseController::OnImmersiveRevealStarted() {
  // The toolbar has began to reveal. If the overlay is showing, hide the
  // preselection bubble to ensure it doesn't cover with the toolbar UI.
  if (IsOverlayShowing()) {
    HidePreselectionBubble();
  }
}

void OverlayBaseController::OnImmersiveRevealEnded() {
  // The toolbar is no longer revealed. If the overlay is showing, reshow the
  // preselection bubble to ensure it doesn't cover with the toolbar UI.
  if (IsOverlayShowing()) {
    ShowPreselectionBubble();
  }
}

void OverlayBaseController::OnImmersiveFullscreenEntered() {
  // The browser entered immersive fullscreen. If the overlay is showing, call
  // close and reopen the preselection bubble to ensure it respositions
  // correctly.
  if (IsOverlayShowing()) {
    CloseAndReshowPreselectionBubble();
  }
}

void OverlayBaseController::OnImmersiveFullscreenExited() {
  // The browser exited immersive fullscreen. If the overlay is showing, call
  // close and reopen the preselection bubble to ensure it respositions
  // correctly.
  if (IsOverlayShowing()) {
    CloseAndReshowPreselectionBubble();
  }
}

void OverlayBaseController::ShowPreselectionBubble() {
  // Don't show the preselection bubble if the overlay is not being shown.
  if (IsResultsSidePanelShowing()) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  // On Mac, the kShowFullscreenToolbar pref is used to determine whether the
  // toolbar is always shown. This causes the toolbar to never unreveal, meaning
  // the preselection bubble will never be shown. Check for this case and show
  // the preselection bubble if needed.
  const bool always_show_toolbar =
      pref_service_->GetBoolean(prefs::kShowFullscreenToolbar);
#else
  const bool always_show_toolbar = false;
#endif  // BUILDFLAG(IS_MAC)

  if (!always_show_toolbar &&
      ImmersiveModeController::From(tab_->GetBrowserWindowInterface())
          ->IsRevealed()) {
    // If the immersive mode controller is revealing top chrome, do not show
    // the preselection bubble. The bubble will be shown once the reveal
    // finishes.
    return;
  }

  if (!preselection_widget_) {
    CHECK(preselection_widget_anchor_);
    // Setup the preselection widget.
    preselection_widget_ = views::BubbleDialogDelegateView::CreateBubble(
        std::make_unique<lens::LensPreselectionBubble>(
            tab_->GetHandle(), preselection_widget_anchor_,
            net::NetworkChangeNotifier::IsOffline(),
            /*exit_clicked_callback=*/
            base::BindRepeating(&OverlayBaseController::RequestSyncClose,
                                weak_factory_.GetWeakPtr(),
                                DismissalSource::kPreselectionToastExitButton),
            /*on_cancel_callback=*/
            base::BindOnce(&OverlayBaseController::RequestSyncClose,
                           weak_factory_.GetWeakPtr(),
                           DismissalSource::kPreselectionToastEscapeKeyPress)));
    preselection_widget_observer_.Observe(preselection_widget_);
    // Setting the parent allows focus traversal out of the preselection widget.
    preselection_widget_->SetFocusTraversableParent(
        preselection_widget_anchor_->GetWidget()->GetFocusTraversable());
    preselection_widget_->SetFocusTraversableParentView(
        preselection_widget_anchor_);
  }

  // When in fullscreen, top Chrome may cover this widget on Mac. Set the
  // z-order to floating UI element to ensure the widget is above the top
  // Chrome. Only do this if immersive mode is enabled to avoid issues with
  // the preselection widget covering other windows.
  if (ImmersiveModeController::From(tab_->GetBrowserWindowInterface())
          ->IsEnabled()) {
    preselection_widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);
  } else {
    preselection_widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  }

  auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
      preselection_widget_->widget_delegate());
  bubble_view->SetCanActivate(true);

  // The bubble position is dependent on if top chrome is showing. Resize the
  // bubble to ensure the correct position is used.
  bubble_view->SizeToContents();
  // Show inactive so that the overlay remains active.
  preselection_widget_->ShowInactive();
}

void OverlayBaseController::CloseAndReshowPreselectionBubble() {
  // If the preselection bubble is already closed, do not reshow it.
  if (!preselection_widget_) {
    return;
  }
  ClosePreselectionBubbleImpl();
  ShowPreselectionBubble();
}

void OverlayBaseController::HidePreselectionBubble() {
  if (preselection_widget_) {
    // The preselection bubble remains in the browser's focus order even when it
    // is hidden, for example, when another browser tab is active. This means it
    // remains possible for the bubble to be activated by keyboard input i.e.
    // tabbing into the bubble, which unhides the bubble even on a browser tab
    // where the overlay is not being shown. Prevent this by setting the bubble
    // to non-activatable while it is hidden.
    auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
        preselection_widget_->widget_delegate());
    bubble_view->SetCanActivate(false);

    preselection_widget_->Hide();
  }
}

void OverlayBaseController::ClosePreselectionBubbleImpl() {
  if (preselection_widget_) {
    preselection_widget_->Close();
    preselection_widget_ = nullptr;
    preselection_widget_observer_.Reset();
  }
}
