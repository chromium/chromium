// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/views/layer_dimmer.h"

#include <memory>

#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace javascript_dialogs {

LayerDimmer::LayerDimmer(aura::Window* parent, aura::Window* dialog)
    : parent_(parent), dialog_(dialog) {
  auto* parentLayer = parent_->layer();

  layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  layer_->SetName(parent_->GetName() + "_LayerDimmer");
  layer_->SetAnimator(new ui::LayerAnimator(base::Milliseconds(200)));
  layer_->SetColor(SkColorSetA(SK_ColorBLACK, 127));
  {
    // Don't animate these changes right now.
    ui::ScopedLayerAnimationSettings settings(layer_->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(0));
    layer_->SetOpacity(0.f);
    layer_->SetBounds(parent_->bounds());
  }

  parentLayer->Add(layer_.get());
  StackLayerUnderDialog();

  parent_->AddObserver(this);
  dialog_->AddObserver(this);
}

LayerDimmer::~LayerDimmer() {
  if (parent_) {
    parent_->RemoveObserver(this);
  }
  if (dialog_) {
    dialog_->RemoveObserver(this);
  }
}

void LayerDimmer::Show() {
  layer_->SetOpacity(1.0f);
  layer_->ScheduleDraw();
}

void LayerDimmer::Hide() {
  layer_->SetOpacity(0.f);
  layer_->ScheduleDraw();
}

void LayerDimmer::StackLayerUnderDialog() {
  auto* parentLayer = parent_->layer();
  parentLayer->StackBelow(layer_.get(), dialog_->layer());
  layer_->ScheduleDraw();
}

void LayerDimmer::OnWindowBoundsChanged(aura::Window* window,
                                        const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds,
                                        ui::PropertyChangeReason reason) {
  if (window == parent_) {
    // Don't animate this bounds change.
    ui::ScopedLayerAnimationSettings settings(layer_->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(0));
    layer_->SetBounds(gfx::Rect(new_bounds.size()));
  }
}

void LayerDimmer::OnWindowDestroying(aura::Window* window) {
  if (window == parent_) {
    parent_->RemoveObserver(this);
    parent_ = nullptr;
  } else {
    DCHECK_EQ(dialog_, window);
    dialog_->RemoveObserver(this);
    dialog_ = nullptr;
  }
}

void LayerDimmer::OnWindowStackingChanged(aura::Window* window) {
  if (window == dialog_) {
    StackLayerUnderDialog();
  }
}

}  // namespace javascript_dialogs
