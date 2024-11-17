// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/blur_switch_view_controller.h"

#include "chrome/browser/media_effects/media_effects_manager_binder.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

BlurSwitchViewController::BlurSwitchViewController(
    views::View& parent_view,
    base::WeakPtr<content::BrowserContext> browser_context)
    : browser_context_(browser_context) {
  auto* container =
      parent_view.AddChildView(std::make_unique<views::BoxLayoutView>());
  container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container->SetInsideBorderInsets(gfx::Insets::VH(10, 10));
  container->SetBetweenChildSpacing(5);

  auto* label = container->AddChildView(std::make_unique<views::Label>(
      u"Blur", views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  blur_switch_ = container->AddChildView(std::make_unique<views::ToggleButton>(
      base::BindRepeating(&BlurSwitchViewController::OnBlurSwitchPressed,
                          weak_ptr_factory_.GetWeakPtr())));
  blur_switch_->SetAccessibleName(u"blur");
}

BlurSwitchViewController::~BlurSwitchViewController() = default;

void BlurSwitchViewController::BindVideoEffectsManager(
    const std::string& active_device_id) {
  ResetConnections();
  if (!browser_context_) {
    return;
  }
  media_effects::BindVideoEffectsManager(
      active_device_id, browser_context_.get(),
      video_effects_manager_.BindNewPipeAndPassReceiver());
  video_effects_manager_->AddObserver(
      video_effects_configuration_observer_.BindNewPipeAndPassRemote());
  video_effects_manager_->GetConfiguration(
      base::BindOnce(&BlurSwitchViewController::OnConfigurationChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BlurSwitchViewController::ResetConnections() {
  video_effects_manager_.reset();
  video_effects_configuration_observer_.reset();
}

void BlurSwitchViewController::OnBlurSwitchPressed() {
  CHECK(blur_switch_);

  video_effects_manager_->SetConfiguration(
      media::mojom::VideoEffectsConfiguration::New(
          nullptr,
          blur_switch_->GetIsOn() ? media::mojom::Blur::New() : nullptr,
          nullptr),
      base::DoNothing());
}

void BlurSwitchViewController::OnConfigurationChanged(
    media::mojom::VideoEffectsConfigurationPtr configuration) {
  CHECK(blur_switch_);

  // SetIsOn is a noop if the value hasn't changed.
  blur_switch_->SetIsOn(!configuration->blur.is_null());
}
