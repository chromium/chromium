// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_origin_text.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "url/gurl.h"

namespace {

constexpr gfx::Tween::Type kTweenType = gfx::Tween::FAST_OUT_SLOW_IN_2;

}  // namespace

WebAppOriginText::WebAppOriginText(BrowserWindowInterface* browser)
    : browser_(browser) {
  CHECK(web_app::AppBrowserController::IsWebApp(browser_));

  browser_->tab_strip_model()->AddObserver(this);
  Observe(browser_->tab_strip_model()->GetActiveWebContents());

  SetID(VIEW_ID_WEB_APP_ORIGIN_TEXT);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  label_ = AddChildView(std::make_unique<views::Label>(
      u"", ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_EMPHASIZED));
  label_->SetElideBehavior(gfx::ELIDE_HEAD);
  label_->SetSubpixelRenderingEnabled(false);
  // Disable Label's auto readability to ensure consistent colors in the title
  // bar (see http://crbug.com/40563850#comment3).
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->layer()->SetOpacity(0);
  label_->layer()->GetAnimator()->AddObserver(this);
  label_->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  // Clip child views to this view.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  GetViewAccessibility().SetRole(ax::mojom::Role::kApplication);
  UpdateAccessibleName();

  // This owns label_ which owns the callback.
  label_text_changed_callback_ =
      label_->AddTextChangedCallback(base::BindRepeating(
          &WebAppOriginText::UpdateAccessibleName, base::Unretained(this)));
}

WebAppOriginText::~WebAppOriginText() = default;

void WebAppOriginText::SetTextColor(SkColor color, bool show_text) {
  label_->SetEnabledColor(color);
  if (show_text) {
    StartFadeAnimation();
  }
}

void WebAppOriginText::SetAllowedToAnimate(bool allowed) {
  allowed_to_animate_ = allowed;
}

void WebAppOriginText::StartFadeAnimation() {
  if (!allowed_to_animate_) {
    return;
  }

  SetVisible(true);

  ui::Layer* label_layer = label_->layer();
  // If a fade animation is already in progress, just skip straight to visible.
  bool animation_already_in_progress =
      label_layer->GetAnimator()->is_animating();

  auto opacity_sequence = std::make_unique<ui::LayerAnimationSequence>();

  // Fade in.
  auto opacity_keyframe = ui::LayerAnimationElement::CreateOpacityElement(
      1, animation_already_in_progress
             ? base::TimeDelta()
             : WebAppToolbarButtonContainer::kOriginFadeInDuration);
  opacity_keyframe->set_tween_type(kTweenType);
  opacity_sequence->AddElement(std::move(opacity_keyframe));

  // Pause.
  opacity_sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      0, WebAppToolbarButtonContainer::kOriginPauseDuration));

  // Fade out.
  opacity_keyframe = ui::LayerAnimationElement::CreateOpacityElement(
      0, WebAppToolbarButtonContainer::kOriginFadeOutDuration);
  opacity_keyframe->set_tween_type(kTweenType);
  opacity_sequence->AddElement(std::move(opacity_keyframe));

  label_layer->GetAnimator()->StartAnimation(opacity_sequence.release());

  NotifyAccessibilityEventDeprecated(ax::mojom::Event::kValueChanged, true);
}

void WebAppOriginText::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  SetVisible(false);
  last_completed_animation_text_for_testing_ = label_->GetText();
}

void WebAppOriginText::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  SetVisible(false);
  last_completed_animation_text_for_testing_ = label_->GetText();
}

std::u16string_view WebAppOriginText::GetLabelTextForTesting() const {
  CHECK(label_);
  return label_->GetText();
}

void WebAppOriginText::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed() && !tab_strip_model->empty()) {
    Observe(selection.new_contents);
    // Don't do anything if this is the original tab loading in. The text will
    // be shown on navigation complete.
    const bool is_startup = !selection.old_contents;
    if (!is_startup) {
      // In tabbed PWAs, the new tab might be from a different origin, in which
      // case we don't want to accidentally show the previous tab's or the app's
      // origin in the label. Update the text appropriately.
      MaybeUpdateAndShowText(selection.new_contents);
    }
  }
}

void WebAppOriginText::DidFinishNavigation(content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || handle->IsSameDocument()) {
    return;
  }
  MaybeUpdateAndShowText(handle->GetWebContents());
}

void WebAppOriginText::MaybeUpdateAndShowText(
    const content::WebContents* new_contents) {
  if (!new_contents) {
    return;
  }
  const web_app::AppBrowserController* const app_controller =
      web_app::AppBrowserController::From(browser_);
  if (!app_controller) {
    return;
  }
  const std::u16string new_origin_text = app_controller->GetLaunchFlashText();
  if (new_origin_text.empty() || new_origin_text == label_->GetText()) {
    return;
  }
  // Note: this will also update the accessible name via the
  // `UpdateAccessibleName()` callback.
  label_->SetText(new_origin_text);

  // CCT UI already displays origin information so there is no need to animate
  // origin text.
  // TODO(crbug.com/40282543): Instead of DidFinishNavigation, we can use
  // ReadyToCommitNavigation if this logic does not consider
  // ShouldShowCustomTabBar or if ShouldShowCustomTabBar can take pre-commit URL
  // as input. ShouldShowCustomTabBar is currently implemented to be used after
  // navigation has committed.
  if (app_controller->ShouldShowCustomTabBar()) {
    label_->layer()->GetAnimator()->StopAnimating();
    return;
  }

  StartFadeAnimation();
}

void WebAppOriginText::UpdateAccessibleName() {
  if (!label_->GetText().empty()) {
    GetViewAccessibility().SetName(std::u16string(label_->GetText()));
  } else {
    GetViewAccessibility().RemoveName();
  }
}

BEGIN_METADATA(WebAppOriginText)
END_METADATA
