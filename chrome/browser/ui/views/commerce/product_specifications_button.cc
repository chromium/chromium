// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/product_specifications_button.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view_class_properties.h"

namespace {
// TODO(b/335015993): These constants are shared with TabSearchContainer, and
// should be moved into one place. Similar for the LockedExpansionMode enum used
// in this file.
constexpr int kProductSpecificationsButtonCornerRadius = 10;
constexpr int kProductSpecificationsButtonLabelMargin = 10;
constexpr int kProductSpecificationsCloseButtonMargin = 8;
constexpr int kProductSpecificationsCloseButtonSize = 16;

constexpr base::TimeDelta kExpansionInDuration = base::Milliseconds(500);
constexpr base::TimeDelta kExpansionOutDuration = base::Milliseconds(250);
constexpr base::TimeDelta kOpacityInDuration = base::Milliseconds(300);
constexpr base::TimeDelta kOpacityOutDuration = base::Milliseconds(100);
constexpr base::TimeDelta kOpacityDelay = base::Milliseconds(100);
constexpr base::TimeDelta kShowDuration = base::Seconds(20);

}  // namespace

ProductSpecificationsButton::ProductSpecificationsButton(
    TabStripController* tab_strip_controller,
    TabStripModel* tab_strip_model,
    commerce::ProductSpecificationsEntryPointController* entry_point_controller,
    bool before_tab_strip,
    View* locked_expansion_view)
    : TabStripControlButton(
          tab_strip_controller,
          base::BindRepeating(&ProductSpecificationsButton::OnClicked,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_COMPARE_ENTRY_POINT_DEFAULT),
          Edge::kNone),
      locked_expansion_view_(locked_expansion_view),
      tab_strip_model_(tab_strip_model),
      entry_point_controller_(entry_point_controller) {
  mouse_watcher_ = std::make_unique<views::MouseWatcher>(
      std::make_unique<views::MouseWatcherViewHost>(locked_expansion_view,
                                                    gfx::Insets()),
      this);
  CHECK(entry_point_controller_);
  entry_point_controller_observations_.Observe(entry_point_controller_);
  auto* const layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);

  SetProperty(views::kElementIdentifierKey,
              kProductSpecificationsButtonElementId);

  SetTooltipText(l10n_util::GetStringUTF16(IDS_COMPARE_ENTRY_POINT_DEFAULT));
  // TODO(b/325661685): Set accessibility name of the button.
  SetLabelStyle(views::style::STYLE_BODY_3_EMPHASIS);
  label()->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);

  const gfx::Insets label_margin =
      gfx::Insets().set_left(kProductSpecificationsButtonLabelMargin);
  label()->SetProperty(views::kMarginsKey, label_margin);

  SetForegroundFrameActiveColorId(kColorTabSearchButtonCRForegroundFrameActive);
  SetForegroundFrameInactiveColorId(
      kColorTabSearchButtonCRForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);
  SetCloseButton(base::BindRepeating(&ProductSpecificationsButton::OnDismissed,
                                     base::Unretained(this)));

  const int space_between_buttons = 2;
  gfx::Insets margin = gfx::Insets();
  if (before_tab_strip) {
    margin.set_left(space_between_buttons);
  } else {
    margin.set_right(space_between_buttons);
  }
  SetProperty(views::kMarginsKey, margin);
  SetOpacity(0);

  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);

  set_paint_transparent_for_custom_image_theme(false);

  layout_manager->SetFlexForView(close_button_, 1);

  SetLayoutManager(std::make_unique<views::FlexLayout>());

  UpdateColors();

  // Button is not visible by default to avoid grabbing focus.
  SetVisible(false);
}

ProductSpecificationsButton::~ProductSpecificationsButton() = default;

gfx::Size ProductSpecificationsButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int full_width =
      GetLayoutManager()->GetPreferredSize(this, available_size).width();
  const int width = full_width * width_factor_;
  const int height =
      TabStripControlButton::CalculatePreferredSize(available_size).height();
  return gfx::Size(width, height);
}

void ProductSpecificationsButton::MouseMovedOutOfHost() {
  SetLockedExpansionMode(LockedExpansionMode::kNone);
}

void ProductSpecificationsButton::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void ProductSpecificationsButton::AnimationEnded(
    const gfx::Animation* animation) {
  ApplyAnimationValue(animation);
  // If the button went from shown -> hidden, unblock the tab strip from
  // showing other modal UIs. Compare to 0.5 to distinguish between show/hide
  // while avoiding potentially inexact float comparison to 0.0.
  // When hiding animation finishes, set the button to not visible to avoid
  // grabbing focus.
  if (animation == &expansion_animation_ &&
      animation->GetCurrentValue() < 0.5) {
    SetVisible(false);
    if (scoped_tab_strip_modal_ui_) {
      scoped_tab_strip_modal_ui_.reset();
    }
  }
}

void ProductSpecificationsButton::AnimationProgressed(
    const gfx::Animation* animation) {
  ApplyAnimationValue(animation);
}

void ProductSpecificationsButton::Show() {
  // If the button is already showing, don't update locked expansion mode.
  if (expansion_animation_.IsShowing()) {
    return;
  }

  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillShow);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteShow();
  }
}

void ProductSpecificationsButton::Hide() {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillHide);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteHide();
  }
}

void ProductSpecificationsButton::ShowEntryPointWithTitle(
    const std::u16string& title) {
  SetText(title);
  SetTooltipText(title);
  Show();
}

void ProductSpecificationsButton::HideEntryPoint() {
  Hide();
}

void ProductSpecificationsButton::SetOpacity(float factor) {
  label()->layer()->SetOpacity(factor);
  close_button_->layer()->SetOpacity(factor);
}

int ProductSpecificationsButton::GetCornerRadius() const {
  return kProductSpecificationsButtonCornerRadius;
}

void ProductSpecificationsButton::ApplyAnimationValue(
    const gfx::Animation* animation) {
  float value = animation->GetCurrentValue();
  if (animation == &expansion_animation_) {
    SetWidthFactor(value);
  } else if (animation == &opacity_animation_) {
    SetOpacity(value);
  }
}

void ProductSpecificationsButton::ExecuteShow() {
  // If the tab strip already has a modal UI showing, exit early.
  if (!tab_strip_model_->CanShowModalUI()) {
    return;
  }

  scoped_tab_strip_modal_ui_ = tab_strip_model_->ShowModalUI();

  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionInDuration));
  opacity_animation_.SetSlideDuration(GetAnimationDuration(kOpacityInDuration));
  const base::TimeDelta delay = GetAnimationDuration(kOpacityDelay);
  opacity_animation_delay_timer_.Start(
      FROM_HERE, delay, this,
      &ProductSpecificationsButton::ShowOpacityAnimation);

  SetVisible(true);
  expansion_animation_.Show();

  hide_button_timer_.Start(FROM_HERE, kShowDuration, this,
                           &ProductSpecificationsButton::OnTimeout);
  base::RecordAction(
      base::UserMetricsAction("Commerce.Compare.ProactiveChipShown"));
}

void ProductSpecificationsButton::ExecuteHide() {
  hide_button_timer_.Stop();
  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionOutDuration));
  expansion_animation_.Hide();

  opacity_animation_.SetSlideDuration(
      GetAnimationDuration(kOpacityOutDuration));
  opacity_animation_.Hide();
  if (entry_point_controller_) {
    entry_point_controller_->OnEntryPointHidden();
  }
}

void ProductSpecificationsButton::OnClicked() {
  if (entry_point_controller_) {
    entry_point_controller_->OnEntryPointExecuted();
  }
  ExecuteHide();
  base::RecordAction(
      base::UserMetricsAction("Commerce.Compare.ProactiveChipClicked"));
}

void ProductSpecificationsButton::OnDismissed() {
  ExecuteHide();
  if (entry_point_controller_) {
    entry_point_controller_->OnEntryPointDismissed();
  }
  base::RecordAction(
      base::UserMetricsAction("Commerce.Compare.ProactiveChipDismissed"));
}

void ProductSpecificationsButton::OnTimeout() {
  Hide();
  base::RecordAction(
      base::UserMetricsAction("Commerce.Compare.ProactiveChipIgnored"));
}

void ProductSpecificationsButton::SetCloseButton(
    views::LabelButton::PressedCallback pressed_callback) {
  auto close_button =
      std::make_unique<views::LabelButton>(std::move(pressed_callback));
  close_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_COMPARE_ENTRY_POINT_CLOSE));

  const ui::ImageModel icon_image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kCloseChromeRefreshIcon,
      kColorTabSearchButtonCRForegroundFrameActive,
      kProductSpecificationsCloseButtonSize);

  close_button->SetImageModel(views::Button::STATE_NORMAL, icon_image_model);
  close_button->SetImageModel(views::Button::STATE_HOVERED, icon_image_model);
  close_button->SetImageModel(views::Button::STATE_PRESSED, icon_image_model);

  close_button->SetPaintToLayer();
  close_button->layer()->SetFillsBoundsOpaquely(false);

  views::InkDrop::Get(close_button.get())
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(close_button.get())->SetHighlightOpacity(0.16f);
  views::InkDrop::Get(close_button.get())->SetVisibleOpacity(0.14f);
  views::InkDrop::Get(close_button.get())
      ->SetBaseColorId(kColorTabSearchButtonCRForegroundFrameActive);

  auto ink_drop_highlight_path =
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets());
  views::HighlightPathGenerator::Install(close_button.get(),
                                         std::move(ink_drop_highlight_path));

  close_button->SetPreferredSize(
      gfx::Size(kProductSpecificationsCloseButtonSize,
                kProductSpecificationsCloseButtonSize));
  close_button->SetBorder(nullptr);

  const gfx::Insets margin =
      gfx::Insets().set_left_right(kProductSpecificationsCloseButtonMargin,
                                   kProductSpecificationsCloseButtonMargin);
  close_button->SetProperty(views::kMarginsKey, margin);

  close_button_ = AddChildView(std::move(close_button));
}

void ProductSpecificationsButton::SetLockedExpansionMode(
    LockedExpansionMode mode) {
  if (mode == LockedExpansionMode::kNone) {
    if (locked_expansion_mode_ == LockedExpansionMode::kWillShow) {
      // Check if the entry point is still eligible for showing.
      if (entry_point_controller_->ShouldExecuteEntryPointShow()) {
        ExecuteShow();
      }
    } else if (locked_expansion_mode_ == LockedExpansionMode::kWillHide) {
      ExecuteHide();
    }
  } else {
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
  }
  locked_expansion_mode_ = mode;
}

void ProductSpecificationsButton::SetWidthFactor(float factor) {
  width_factor_ = factor;
  PreferredSizeChanged();
}

void ProductSpecificationsButton::ShowOpacityAnimation() {
  opacity_animation_.Show();
}

base::TimeDelta ProductSpecificationsButton::GetAnimationDuration(
    base::TimeDelta duration) {
  return gfx::Animation::ShouldRenderRichAnimation() ? duration
                                                     : base::TimeDelta();
}

BEGIN_METADATA(ProductSpecificationsButton)
END_METADATA
