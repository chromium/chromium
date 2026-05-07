// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_button.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/glic/glic_actor_constants.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/class_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace glic {

namespace {

gfx::Insets GetIconMargins(bool label_shown) {
  int left = 6;
  int right = 5;

  if (label_shown) {
    // Extra left margin if the label is shown.
    left += 2;
  }

  return gfx::Insets().set_left_right(left, right);
}
}  // namespace

TabStripGlicButton::TabStripGlicButton(
    BrowserWindowInterface* browser_window_interface,
    base::RepeatingClosure expansion_animation_done_callback,
    const std::u16string& tooltip,
    PressedCallback pressed_callback,
    PressedCallback close_pressed_callback)
    : GlicButton<TabStripNudgeButton>(browser_window_interface,
                                      expansion_animation_done_callback,
                                      tooltip,
                                      kIconSize,
                                      /** TabStripNudgeButton args */
                                      browser_window_interface,
                                      std::move(pressed_callback),
                                      std::move(close_pressed_callback),
                                      GetLabelText(),
                                      kGlicNudgeButtonElementId,
                                      Edge::kNone,
                                      gfx::VectorIcon::EmptyIcon(),
                                      /*show_close_button=*/true) {
  TabStripNudgeButton::UpdateIcon();
  OnLabelVisibilityChanged();
  auto* image_view = static_cast<views::ImageView*>(image_container_view());
  image_view->SetImageSize({kIconSize, kIconSize});

  SetLabelMargins();

  UpdateColors();

  set_context_menu_controller(this);
}

TabStripGlicButton::~TabStripGlicButton() = default;

bool TabStripGlicButton::GetIsShowingNudge() const {
  return width_state_ == GlicButton<TabStripNudgeButton>::WidthState::kNudge;
}

gfx::Rect TabStripGlicButton::GetBoundsWithInset() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetInsets());
  return bounds;
}

void TabStripGlicButton::OnLabelVisibilityChanged() {
  image_container_view()->SetProperty(
      views::kMarginsKey, GetIconMargins(!IsAnimatingTextVisibility()));
}

void TabStripGlicButton::ResetSplitButtonCornerStyling() {
  SetLeftRightCornerRadii(TabStripNudgeButton::GetCornerRadius(),
                          TabStripNudgeButton::GetCornerRadius());
}

void TabStripGlicButton::SetLabelMargins() {
  int bottom = 1;

  int right = kLabelRightMargin;
  if (!close_button()->GetVisible()) {
    right += 4;
  }

  label()->SetProperty(views::kMarginsKey,
                       gfx::Insets().set_right(right).set_bottom(bottom));
}

void TabStripGlicButton::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  GlicButton<TabStripNudgeButton>::ShowContextMenuForViewImpl(source, point,
                                                              source_type);
}

gfx::SlideAnimation* TabStripGlicButton::GetExpansionAnimationForTesting() {
  return width_animation_controller_->GetAnimationForTesting();  // IN-TEST
}

float TabStripGlicButton::GetWidthFactor() const {
  return TabStripNudgeButton::GetWidthFactor();
}

BEGIN_METADATA(TabStripGlicButton)
END_METADATA

}  // namespace glic
