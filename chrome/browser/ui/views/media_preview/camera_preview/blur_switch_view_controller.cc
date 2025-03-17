// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/blur_switch_view_controller.h"

#include "chrome/browser/media_effects/media_effects_manager_binder.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

BlurSwitchViewController::BlurSwitchViewController(
    views::View& parent_view,
    base::WeakPtr<content::BrowserContext> browser_context)
    : browser_context_(browser_context) {
  auto* container =
      parent_view.AddChildView(std::make_unique<views::BoxLayoutView>());
  container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  auto* layout_provider = views::LayoutProvider::Get();
  container->SetInsideBorderInsets(
      gfx::Insets::TLBR(layout_provider->GetDistanceMetric(
                            views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                        0, 0, 0));
  container->SetBetweenChildSpacing(layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL));

  auto* label_container =
      container->AddChildView(std::make_unique<views::BoxLayoutView>());
  label_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetFlexForView(label_container, 1);
  auto* label = label_container->AddChildView(std::make_unique<views::Label>(
      u"Blur", views::style::CONTEXT_LABEL, views::style::STYLE_BODY_3_MEDIUM));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  auto* description =
      label_container->AddChildView(std::make_unique<views::Label>(
          u"Blur your background", views::style::CONTEXT_LABEL,
          views::style::STYLE_BODY_5));
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description->SetMultiLine(true);

  // The parent BoxLayout adds the biggest margin of its children to the
  // container height, so setting a margin on the blur switch would
  // inadvertently add to the total height of the container. This works around
  // the problem by the switch inside another container.
  auto* blur_switch_container =
      container->AddChildView(std::make_unique<views::BoxLayoutView>());
  container->SetFlexForView(blur_switch_container, 0);
  blur_switch_ =
      blur_switch_container->AddChildView(std::make_unique<views::ToggleButton>(
          base::BindRepeating(&BlurSwitchViewController::OnBlurSwitchPressed,
                              weak_ptr_factory_.GetWeakPtr())));
  blur_switch_->SetAccessibleName(u"blur");
  blur_switch_->SetProperty(views::kMarginsKey, gfx::Insets::VH(2, 0));
}

BlurSwitchViewController::~BlurSwitchViewController() = default;

void BlurSwitchViewController::BindVideoEffectsManager(
    const std::string& active_device_id) {
  ResetConnections();
  if (!browser_context_) {
    return;
  }

  video_effects_manager_ = media_effects::GetOrCreateVideoEffectsManager(
      active_device_id, browser_context_.get());
  video_effects_manager_->AddObserver(
      video_effects_configuration_observer_.BindNewPipeAndPassRemote());
}

void BlurSwitchViewController::ResetConnections() {
  video_effects_manager_.reset();
  video_effects_configuration_observer_.reset();
}

void BlurSwitchViewController::OnBlurSwitchPressed() {
  CHECK(blur_switch_);
  if (video_effects_manager_) {
    video_effects_manager_->SetConfiguration(
        media::mojom::VideoEffectsConfiguration::New(
            nullptr,
            blur_switch_->GetIsOn() ? media::mojom::Blur::New() : nullptr,
            nullptr));
  }
}

void BlurSwitchViewController::OnConfigurationChanged(
    media::mojom::VideoEffectsConfigurationPtr configuration) {
  CHECK(blur_switch_);

  // SetIsOn is a noop if the value hasn't changed.
  blur_switch_->SetIsOn(!configuration->blur.is_null());
}
