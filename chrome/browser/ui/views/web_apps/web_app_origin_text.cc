// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_origin_text.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace {

constexpr gfx::Tween::Type kTweenType = gfx::Tween::FAST_OUT_SLOW_IN_2;

}  // namespace

WebAppOriginText::WebAppOriginText(Browser* browser) {
  DCHECK(web_app::AppBrowserController::IsForWebAppBrowser(browser));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  label_ = std::make_unique<views::Label>(
               browser->app_controller()->GetFormattedUrlOrigin(),
               ChromeTextContext::CONTEXT_BODY_TEXT_SMALL,
               ChromeTextStyle::STYLE_EMPHASIZED)
               .release();
  label_->SetElideBehavior(gfx::ELIDE_HEAD);
  label_->SetSubpixelRenderingEnabled(false);
  // Disable Label's auto readability to ensure consistent colors in the title
  // bar (see http://crbug.com/814121#c2).
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->layer()->SetOpacity(0);
  AddChildView(label_);

  // Clip child views to this view.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);
}

WebAppOriginText::~WebAppOriginText() = default;

void WebAppOriginText::SetTextColor(SkColor color) {
  label_->SetEnabledColor(color);
}

void WebAppOriginText::StartFadeAnimation() {
  ui::Layer* label_layer = label_->layer();

  // Current state will become the first animation keyframe.
  DCHECK_EQ(label_layer->opacity(), 0);

  auto opacity_sequence = std::make_unique<ui::LayerAnimationSequence>();

  // Fade in.
  auto opacity_keyframe = ui::LayerAnimationElement::CreateOpacityElement(
      1, WebAppFrameToolbarView::kOriginFadeInDuration);
  opacity_keyframe->set_tween_type(kTweenType);
  opacity_sequence->AddElement(std::move(opacity_keyframe));

  // Pause.
  opacity_sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      0, WebAppFrameToolbarView::kOriginPauseDuration));

  // Fade out.
  opacity_keyframe = ui::LayerAnimationElement::CreateOpacityElement(
      0, WebAppFrameToolbarView::kOriginFadeOutDuration);
  opacity_keyframe->set_tween_type(kTweenType);
  opacity_sequence->AddElement(std::move(opacity_keyframe));

  label_layer->GetAnimator()->StartAnimation(opacity_sequence.release());

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebAppOriginText::AnimationComplete,
                     weak_factory_.GetWeakPtr()),
      WebAppFrameToolbarView::OriginTotalDuration());

  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void WebAppOriginText::AnimationComplete() {
  SetVisible(false);
}

void WebAppOriginText::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kApplication;
  node_data->SetName(label_->GetText());
}
