// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_location_bar_host.h"

#include <memory>

#include "base/functional/bind.h"

#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_content_config.h"
#include "content/public/browser/web_contents.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

OmniboxPopupLocationBarHost::OmniboxPopupLocationBarHost(
    LocationBarView& location_bar,
    OmniboxController& controller)
    : location_bar_view_(location_bar), controller_(controller) {
  content_ = location_bar_view_->AddChildView(
      std::make_unique<OmniboxFullPopupWebUIContent>(
          this, /*sizer=*/nullptr,
          OmniboxPopupContentConfig::ForFullInLocationBar(), &location_bar,
          &controller));
  content_->SetVisible(false);
  location_bar_observation_.Observe(&location_bar);
}

OmniboxPopupLocationBarHost::~OmniboxPopupLocationBarHost() {
  location_bar_observation_.Reset();
  if (OmniboxFullPopupWebUIContent* content = content_) {
    content_ = nullptr;
    location_bar_view_->RemoveChildViewT(content);
  }
}

void OmniboxPopupLocationBarHost::Show() {
  if (!content_) {
    return;
  }
  const bool was_shown = IsShown();
  content_->SetVisible(true);
  if (!was_shown) {
    // Reloads a crashed page and reattaches the WebContents detached on hide.
    content_->ShowUI();
    // As `OmniboxPopupPresenterBase::ShowWidget()` does, report the content as
    // presented only once the compositor shows it, so that a pending context
    // menu (see `OmniboxFullPopupWebUIContent::ShowPendingContextMenuIfReady()`)
    // doesn't open before the first frame.
    views::Widget* widget = location_bar_view_->GetWidget();
    if (ui::Compositor* compositor =
            widget ? widget->GetCompositor() : nullptr) {
      compositor->RequestPresentationTimeForNextFrame(
          base::BindOnce(&OmniboxPopupLocationBarHost::OnContentPresented,
                         presentation_weak_factory_.GetWeakPtr()));
    } else {
      content_->OnPopupWidgetPresentedChanged(true);
    }
  }
  content_->UpdateAimButtonVisibility();
  UpdateLayout();
}

void OmniboxPopupLocationBarHost::OnContentPresented(
    const gfx::PresentationFeedback& feedback) {
  if (IsShown()) {
    content_->OnPopupWidgetPresentedChanged(true);
  }
}

void OmniboxPopupLocationBarHost::Hide() {
  presentation_weak_factory_.InvalidateWeakPtrs();
  if (IsShown()) {
    content_->CancelPendingContextMenuRequest();
    content_->OnPopupWidgetPresentedChanged(false);
    ClearFocusFromContent();
    content_->SetVisible(false);
    content_->HideUI();
  }
}

void OmniboxPopupLocationBarHost::RequestFocus() {
  // Only a shown host has the page attached. While hidden, `content_` may have
  // detached its WebContents, and `GetWebContents()` would then create and
  // attach a new blank one, so use the wrapped WebContents.
  if (!IsShown()) {
    return;
  }
  content_->RequestFocus();
  // Focus the page as well: if `content_` already has views focus,
  // `RequestFocus()` does nothing.
  if (content::WebContents* web_contents = content_->GetWrappedWebContents()) {
    web_contents->Focus();
  }
}

bool OmniboxPopupLocationBarHost::IsShown() const {
  return content_ && content_->GetVisible();
}

OmniboxPopupWebUIBaseContent* OmniboxPopupLocationBarHost::GetWebUIContent() {
  return content_;
}

void OmniboxPopupLocationBarHost::SetPermissionPromptShowing(bool showing) {}

bool OmniboxPopupLocationBarHost::IsClosePrevented() const {
  return false;
}

bool OmniboxPopupLocationBarHost::CompletedFirstVisuallyNonEmptyPaint() {
  return content_ && content_->CompletedFirstVisuallyNonEmptyPaint();
}

void OmniboxPopupLocationBarHost::DeactivateAndKillFocus() {}

std::optional<OmniboxPopupHost::FocusStateToRestore>
OmniboxPopupLocationBarHost::TakeFocusStateToRestoreAfterModalDialog() {
  return std::nullopt;
}

bool OmniboxPopupLocationBarHost::ShouldReceiveFocus() const {
  return true;
}

std::unique_ptr<OmniboxPopupDeactivationBlocker>
OmniboxPopupLocationBarHost::CreateDeactivationBlocker() {
  return nullptr;
}

OmniboxController* OmniboxPopupLocationBarHost::GetOmniboxController() {
  return &controller_.get();
}

void OmniboxPopupLocationBarHost::OnViewBoundsChanged(
    views::View* observed_view) {
  UpdateLayout();
}

void OmniboxPopupLocationBarHost::ClearFocusFromContent() {
  views::FocusManager* focus_manager = location_bar_view_->GetFocusManager();
  if (!content_ || !focus_manager) {
    return;
  }
  if (content_->Contains(focus_manager->GetStoredFocusView())) {
    focus_manager->SetStoredFocusView(nullptr);
  }
  if (content_->Contains(focus_manager->GetFocusedView())) {
    focus_manager->ClearFocus();
  }
}

void OmniboxPopupLocationBarHost::UpdateLayout() {
  const gfx::Rect bounds = location_bar_view_->GetLocalBounds();
  if (!content_ || bounds.width() == 0) {
    return;
  }
  // Keep `content_` above children that `LocationBarView` adds later.
  location_bar_view_->ReorderChildView(content_, -1);
  content_->SetBoundsRect(bounds);
}
