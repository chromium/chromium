// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_origin_text.h"

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "url/gurl.h"

namespace {

constexpr gfx::Tween::Type kTweenType = gfx::Tween::FAST_OUT_SLOW_IN_2;

}  // namespace

WebAppOriginText::WebAppOriginText(Browser* browser) {
  DCHECK(web_app::AppBrowserController::IsWebApp(browser));

  browser->tab_strip_model()->AddObserver(this);
  Observe(browser->tab_strip_model()->GetActiveWebContents());

  SetID(VIEW_ID_WEB_APP_ORIGIN_TEXT);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  label_ = std::make_unique<views::Label>(
               origin_text_, ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
               views::style::STYLE_EMPHASIZED)
               .release();
  label_->SetElideBehavior(gfx::ELIDE_HEAD);
  label_->SetSubpixelRenderingEnabled(false);
  // Disable Label's auto readability to ensure consistent colors in the title
  // bar (see http://crbug.com/814121#c2).
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->layer()->SetOpacity(0);
  label_->layer()->GetAnimator()->AddObserver(this);
  label_->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  AddChildView(label_.get());

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

  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void WebAppOriginText::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  SetVisible(false);
}

const std::u16string& WebAppOriginText::GetLabelTextForTesting() {
  CHECK(label_ != nullptr);
  return label_->GetText();
}

void WebAppOriginText::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed() && !tab_strip_model->empty()) {
    Observe(selection.new_contents);
  }
}

void WebAppOriginText::DidFinishNavigation(content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || handle->IsSameDocument()) {
    return;
  }
  content::WebContents* web_contents = handle->GetWebContents();
  if (!web_contents) {
    return;
  }
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return;
  }
  web_app::AppBrowserController* app_controller = browser->app_controller();
  if (!app_controller) {
    return;
  }
  std::u16string new_origin_text = app_controller->GetLaunchFlashText();
  if (new_origin_text.empty() || new_origin_text == origin_text_) {
    return;
  }
  origin_text_ = std::move(new_origin_text);
  label_->SetText(origin_text_);
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
    GetViewAccessibility().SetName(label_->GetText());
  } else {
    GetViewAccessibility().RemoveName();
  }
}

BEGIN_METADATA(WebAppOriginText)
END_METADATA
