// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_location_bar_host.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_content_config.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"
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
  content_observation_.Observe(content_);
  location_bar_observation_.Observe(&location_bar);
}

OmniboxPopupLocationBarHost::~OmniboxPopupLocationBarHost() {
  content_observation_.Reset();
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

void OmniboxPopupLocationBarHost::OnViewBlurred(views::View* observed_view) {
  // Focus moving within the omnibox, e.g. to `LocationBarView` on
  // `FocusLocation()`, isn't a blur. Deactivating the window is, as for
  // `OmniboxViewViews`.
  if (observed_view != content_ || IsViewsFocusInOmnibox()) {
    return;
  }
  OmniboxPopupViewFullWebUI::BlurOmnibox(*controller_, [] {});
  if (views::Widget* widget = location_bar_view_->GetWidget();
      widget && !widget->IsActive()) {
    // `FocusManager::StoreFocusedView()` stores `content_` after `Blur()`
    // returns. Replace it with `LocationBarView` (the Full WebUI focus proxy),
    // whose `OnFocus()` refocuses the omnibox on reactivation, and which
    // `RestoreFocusedView()` can focus even if `content_` was hidden.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &OmniboxPopupLocationBarHost::StoreLocationBarForReactivation,
            weak_factory_.GetWeakPtr()));
  }
}

bool OmniboxPopupLocationBarHost::IsViewsFocusInOmnibox() const {
  views::FocusManager* focus_manager = location_bar_view_->GetFocusManager();
  if (!focus_manager) {
    return false;
  }
  views::View* focused = focus_manager->GetFocusedView();
  return focused && (focused == &*location_bar_view_ ||
                     (content_ && content_->Contains(focused)));
}

void OmniboxPopupLocationBarHost::StoreLocationBarForReactivation() {
  views::Widget* widget = location_bar_view_->GetWidget();
  views::FocusManager* focus_manager = location_bar_view_->GetFocusManager();
  if (widget && !widget->IsActive() && focus_manager &&
      focus_manager->GetStoredFocusView() == content_) {
    focus_manager->SetStoredFocusView(&*location_bar_view_);
#if BUILDFLAG(IS_MAC)
    // While inactive, take native first responder away from the WebView, so
    // reactivation doesn't focus the page (WebView::OnWebContentsFocused())
    // before LocationBarView's posted refocus runs.
    focus_manager->ClearNativeFocus();
#endif
  }
}

void OmniboxPopupLocationBarHost::ClearFocusFromContent() {
  views::FocusManager* focus_manager = location_bar_view_->GetFocusManager();
  if (!content_ || !focus_manager) {
    return;
  }
  const bool stored_in_content =
      content_->Contains(focus_manager->GetStoredFocusView());
  const bool focused_in_content =
      content_->Contains(focus_manager->GetFocusedView());
  if (stored_in_content) {
    focus_manager->SetStoredFocusView(nullptr);
  }
  if (focused_in_content) {
    focus_manager->ClearFocus();
  }
#if BUILDFLAG(IS_MAC)
  if (!focused_in_content && stored_in_content &&
      !focus_manager->GetFocusedView()) {
    // On deactivation this runs inside `FocusManager::SetFocusedView(nullptr)`
    // (from `OnViewBlurred()`): views focus is already cleared, but `content_`
    // is still the stored focus view, since `FocusManager` stores each view it
    // focuses and resets that only after `Blur()` returns. AppKit walks the
    // key-view loop for a new first responder when the first responder's view
    // is hidden, which would move it to the page and re-activate the window
    // (see `WebContentsNSViewBridge::SetVisible()`), so clear native focus
    // first.
    focus_manager->ClearNativeFocus();
  }
#endif
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
