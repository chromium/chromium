// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/hover_button.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::Border> CreateBorderWithVerticalSpacing(
    int vertical_spacing) {
  const int horizontal_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_HORIZONTAL_PADDING);
  return views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_spacing, horizontal_spacing));
}

// Wrapper class for the icon that maintains consistent spacing for both badged
// and unbadged icons.
// Badging may make the icon slightly wider (but not taller). However, the
// layout should be the same whether or not the icon is badged, so allow the
// badged part of the icon to extend into the padding.
class IconWrapper : public views::View {
 public:
  METADATA_HEADER(IconWrapper);
  explicit IconWrapper(std::unique_ptr<views::View> icon, int vertical_spacing)
      : icon_(AddChildView(std::move(icon))) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    // Make sure hovering over the icon also hovers the |HoverButton|.
    SetCanProcessEventsWithinSubtree(false);
    // Don't cover |icon| when the ink drops are being painted.
    // |MenuButton| already does this with its own image.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetProperty(views::kMarginsKey, gfx::Insets::VH(vertical_spacing, 0));
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    const int icon_height = icon_->GetPreferredSize().height();
    const int icon_label_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL);
    return gfx::Size(icon_height + icon_label_spacing, icon_height);
  }

  views::View* icon() { return icon_; }

 private:
  raw_ptr<views::View> icon_;
};

BEGIN_METADATA(IconWrapper, views::View)
END_METADATA

}  // namespace

HoverButton::HoverButton(PressedCallback callback, const std::u16string& text)
    : views::LabelButton(callback, text, views::style::CONTEXT_BUTTON) {
  SetButtonController(std::make_unique<HoverButtonController>(
      this, std::move(callback),
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(this)));

  views::InstallRectHighlightPathGenerator(this);

  SetInstallFocusRingOnFocus(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  const int vert_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_CONTROL_LIST_VERTICAL) /
                           2;
  SetBorder(CreateBorderWithVerticalSpacing(vert_spacing));

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                               /*highlight_on_hover=*/false,
                                               /*highlight_on_focus=*/true);
  views::InkDrop::Get(this)->SetBaseColorId(
      views::TypographyProvider::Get().GetColorId(
          views::style::CONTEXT_BUTTON, views::style::STYLE_SECONDARY));

  SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                           ui::EF_RIGHT_MOUSE_BUTTON);
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnRelease);
}

HoverButton::HoverButton(PressedCallback callback,
                         const ui::ImageModel& icon,
                         const std::u16string& text)
    : HoverButton(std::move(callback), text) {
  SetImageModel(STATE_NORMAL, icon);
}

HoverButton::HoverButton(PressedCallback callback,
                         std::unique_ptr<views::View> icon_view,
                         const std::u16string& title,
                         const std::u16string& subtitle,
                         std::unique_ptr<views::View> secondary_view,
                         bool add_vertical_label_spacing)
    : HoverButton(std::move(callback), std::u16string()) {
  label()->SetHandlesTooltips(false);

  // Set the layout manager to ignore the ink_drop_container to ensure the ink
  // drop tracks the bounds of its parent.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetChildViewIgnoredByLayout(ink_drop_container(), true);

  // The vertical space that must exist on the top and the bottom of the item
  // to ensure the proper spacing is maintained between items when stacking
  // vertically.
  const int vertical_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   DISTANCE_CONTROL_LIST_VERTICAL) /
                               2;
  if (icon_view) {
    icon_view_ = AddChildView(std::make_unique<IconWrapper>(
                                  std::move(icon_view), vertical_spacing))
                     ->icon();
  }

  // |label_wrapper| will hold both the title and subtitle if it exists.
  auto label_wrapper = std::make_unique<views::View>();

  title_ = label_wrapper->AddChildView(std::make_unique<views::StyledLabel>());
  title_->SetText(title);
  // Allow the StyledLabel for title to assume its preferred size on a single
  // line and let the flex layout attenuate its width if necessary.
  title_->SizeToFit(0);
  // Hover the whole button when hovering |title_|. This is OK because |title_|
  // will never have a link in it.
  title_->SetCanProcessEventsWithinSubtree(false);

  if (!subtitle.empty()) {
    auto subtitle_label = std::make_unique<views::Label>(
        subtitle, views::style::CONTEXT_BUTTON, views::style::STYLE_SECONDARY);
    subtitle_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    subtitle_label->SetAutoColorReadabilityEnabled(false);
    subtitle_ = label_wrapper->AddChildView(std::move(subtitle_label));
  }

  label_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  label_wrapper->SetCanProcessEventsWithinSubtree(false);
  label_wrapper->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(add_vertical_label_spacing ? vertical_spacing : 0, 0));
  label_wrapper_ = AddChildView(std::move(label_wrapper));
  // Observe |label_wrapper_| bounds changes to ensure the HoverButton tooltip
  // is kept in sync with the size.
  label_observation_.Observe(label_wrapper_.get());

  if (secondary_view) {
    secondary_view->SetCanProcessEventsWithinSubtree(false);
    // |secondary_view| needs a layer otherwise it's obscured by the layer
    // used in drawing ink drops.
    secondary_view->SetPaintToLayer();
    secondary_view->layer()->SetFillsBoundsOpaquely(false);
    const int icon_label_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL);

    // Set vertical margins such that the vertical distance between HoverButtons
    // is maintained.
    secondary_view->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(vertical_spacing, icon_label_spacing,
                          vertical_spacing, 0));
    secondary_view_ = AddChildView(std::move(secondary_view));
  }

  // Create the appropriate border with no vertical insets. The required spacing
  // will be met via margins set on the containing views.
  SetBorder(CreateBorderWithVerticalSpacing(0));
}

HoverButton::~HoverButton() = default;

void HoverButton::SetBorder(std::unique_ptr<views::Border> b) {
  LabelButton::SetBorder(std::move(b));
  PreferredSizeChanged();
}

void HoverButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);
}

void HoverButton::PreferredSizeChanged() {
  LabelButton::PreferredSizeChanged();
  if (GetLayoutManager())
    SetMinSize(GetLayoutManager()->GetPreferredSize(this));
}

void HoverButton::OnViewBoundsChanged(View* observed_view) {
  LabelButton::OnViewBoundsChanged(observed_view);
  if (observed_view == label_wrapper_)
    SetTooltipAndAccessibleName();
}

void HoverButton::SetTitleText(const std::u16string& text) {
  title_->SetText(text);
  // Allow the styled label to assume its preferred size since the text size may
  // have changed.
  PreferredSizeChanged();
}

void HoverButton::SetTitleTextStyle(views::style::TextStyle text_style,
                                    SkColor background_color,
                                    absl::optional<ui::ColorId> color_id) {
  if (!title()) {
    return;
  }

  title_->SetDefaultTextStyle(text_style);
  title_->SetDisplayedOnBackgroundColor(background_color);
  if (color_id) {
    title_->SetDefaultEnabledColorId(color_id);
  }
}

void HoverButton::SetSubtitleTextStyle(int text_context,
                                       views::style::TextStyle text_style) {
  if (!subtitle())
    return;

  subtitle()->SetTextContext(text_context);
  subtitle()->SetTextStyle(text_style);
  subtitle()->SetAutoColorReadabilityEnabled(true);

  // `subtitle_`'s preferred size may have changed. Notify the view because
  // `subtitle_` is an indirect child and thus
  // HoverButton::ChildPreferredSizeChanged() is not called.
  PreferredSizeChanged();
}

void HoverButton::SetTooltipAndAccessibleName() {
  const std::u16string accessible_name =
      subtitle_ == nullptr
          ? title_->GetText()
          : base::JoinString({title_->GetText(), subtitle_->GetText()}, u"\n");

  // views::StyledLabels only add tooltips for any links they may have. However,
  // since HoverButton will never insert a link inside its child StyledLabel,
  // decide whether it needs a tooltip by checking whether the available space
  // is smaller than its preferred size.
  const bool needs_tooltip =
      label_wrapper_->GetPreferredSize().width() > label_wrapper_->width();
  SetTooltipText(needs_tooltip ? accessible_name : std::u16string());
  SetAccessibleName(accessible_name);
}

views::Button::KeyClickAction HoverButton::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN) {
    // As the hover button is presented in the user menu, it triggers a
    // kOnKeyPress action every time the user clicks on enter on all platforms.
    // (it ignores the value of PlatformStyle::kReturnClicksFocusedControl)
    return KeyClickAction::kOnKeyPress;
  }
  return LabelButton::GetKeyClickActionForEvent(event);
}

void HoverButton::StateChanged(ButtonState old_state) {
  LabelButton::StateChanged(old_state);

  // |HoverButtons| are designed for use in a list, so ensure only one button
  // can have a hover background at any time by requesting focus on hover.
  if (GetState() == STATE_HOVERED && old_state != STATE_PRESSED) {
    RequestFocus();
  } else if (GetState() == STATE_NORMAL && HasFocus()) {
    GetFocusManager()->SetFocusedView(nullptr);
  }
}

views::View* HoverButton::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (!HitTestPoint(point))
    return nullptr;

  // Let the secondary control handle it if it has a tooltip.
  if (secondary_view_) {
    gfx::Point point_in_secondary_view(point);
    ConvertPointToTarget(this, secondary_view_, &point_in_secondary_view);
    View* handler =
        secondary_view_->GetTooltipHandlerForPoint(point_in_secondary_view);
    if (handler) {
      gfx::Point point_in_handler_view(point);
      ConvertPointToTarget(this, handler, &point_in_handler_view);
      if (!handler->GetTooltipText(point_in_secondary_view).empty()) {
        return handler;
      }
    }
  }

  return this;
}

BEGIN_METADATA(HoverButton, views::LabelButton)
END_METADATA
