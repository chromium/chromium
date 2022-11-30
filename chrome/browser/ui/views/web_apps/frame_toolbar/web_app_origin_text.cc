// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_origin_text.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"

namespace {

constexpr gfx::Tween::Type kTweenType = gfx::Tween::FAST_OUT_SLOW_IN_2;

}  // namespace

WebAppOriginText::WebAppOriginText(Browser* browser) {
  DCHECK(web_app::AppBrowserController::IsWebApp(browser));

  SetID(VIEW_ID_WEB_APP_ORIGIN_TEXT);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  label_ = std::make_unique<views::Label>(
               browser->app_controller()->GetLaunchFlashText(),
               ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
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
}

WebAppOriginText::~WebAppOriginText() = default;

void WebAppOriginText::SetTextColor(SkColor color, bool show_text) {
  label_->SetEnabledColor(color);
  if (show_text)
    StartFadeAnimation();
}

void WebAppOriginText::StartFadeAnimation() {
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

void WebAppOriginText::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kApplication;
  node_data->SetNameChecked(label_->GetText());
}

BEGIN_METADATA(WebAppOriginText, views::View)
END_METADATA
