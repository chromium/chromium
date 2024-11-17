// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/hover_button.h"

#include <algorithm>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
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

int GetVerticalSpacing() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
             DISTANCE_CONTROL_LIST_VERTICAL) /
         2;
}

// Wrapper class for the icon that maintains consistent spacing for both badged
// and unbadged icons.
// Badging may make the icon slightly wider (but not taller). However, the
// layout should be the same whether or not the icon is badged, so allow the
// badged part of the icon to extend into the padding.
class IconWrapper : public views::View {
  METADATA_HEADER(IconWrapper, views::View)

 public:
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
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int icon_height = icon_->GetPreferredSize(available_size).height();
    const int icon_label_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL);
    return gfx::Size(icon_height + icon_label_spacing, icon_height);
  }

  views::View* icon() { return icon_; }

 private:
  raw_ptr<views::View> icon_;
};

BEGIN_METADATA(IconWrapper)
END_METADATA

}  // namespace

HoverButton::HoverButton(PressedCallback callback, const std::u16string& text)
    : views::LabelButton(
          base::BindRepeating(&HoverButton::OnPressed, base::Unretained(this)),
          text,
          views::style::CONTEXT_BUTTON),
      callback_(std::move(callback)) {
  SetButtonController(std::make_unique<HoverButtonController>(
      this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(this)));

  views::InstallRectHighlightPathGenerator(this);

  SetInstallFocusRingOnFocus(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  SetBorder(CreateBorderWithVerticalSpacing(GetVerticalSpacing()));

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                               /*highlight_on_hover=*/false,
                                               /*highlight_on_focus=*/true);
  views::InkDrop::Get(this)->SetBaseColorId(kColorHoverButtonBackgroundHovered);
  // kColorHoverButtonBackgroundHovered has its own opacity.
  // sets the opacity to 100% * opacity(kColorHoverButtonBackgroundHovered).
  views::InkDrop::Get(this)->SetVisibleOpacity(1.0f);
  views::InkDrop::Get(this)->SetHighlightOpacity(1.0f);

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
                         bool add_vertical_label_spacing,
                         const std::u16string& footer)
    : HoverButton(std::move(callback), std::u16string()) {
  label()->SetHandlesTooltips(false);

  // Set the layout manager to ignore the ink_drop_container to ensure the ink
  // drop tracks the bounds of its parent.
  ink_drop_container()->SetProperty(views::kViewIgnoredByLayoutKey, true);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // The vertical space that must exist on the top and the bottom of the item
  // to ensure the proper spacing is maintained between items when stacking
  // vertically.
  const int vertical_spacing = GetVerticalSpacing();
  if (icon_view) {
    icon_wrapper_ = AddChildView(
        std::make_unique<IconWrapper>(std::move(icon_view), vertical_spacing));
    icon_view_ = static_cast<IconWrapper*>(icon_wrapper_)->icon();
  }

  // |label_wrapper| will hold the title as well as subtitle and footer, if
  // present.
  auto label_wrapper = std::make_unique<views::View>();

  title_ = label_wrapper->AddChildView(std::make_unique<views::StyledLabel>());
  title_->SetText(title);
  // Allow the StyledLabel for title to assume its preferred size on a single
  // line and let the flex layout attenuate its width if necessary.
  title_->SizeToFit(0);
  // Hover the whole button when hovering |title_|. This is OK because |title_|
  // will never have a link in it.
  title_->SetCanProcessEventsWithinSubtree(false);
  // A title text update may result in the same label size and not trigger any
  // observers. Thus, we need to add a callback that updates tooltip and
  // accessible name when title text changes.
  text_changed_subscriptions_.push_back(title_->AddTextChangedCallback(
      base::BindRepeating(&HoverButton::UpdateTooltipAndAccessibleName,
                          base::Unretained(this))));

  if (!subtitle.empty()) {
    std::unique_ptr<views::Label> subtitle_label =
        CreateSecondaryLabel(subtitle);
    subtitle_ = label_wrapper->AddChildView(std::move(subtitle_label));
  }
  if (!footer.empty()) {
    std::unique_ptr<views::Label> footer_label = CreateSecondaryLabel(footer);
    footer_ = label_wrapper->AddChildView(std::move(footer_label));
  }

  label_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded, true));
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

gfx::Size HoverButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (label_wrapper_) {
    return GetLayoutManager()->GetPreferredSize(this, available_size);
  }

  return views::LabelButton::CalculatePreferredSize(available_size);
}

void HoverButton::SetBorder(std::unique_ptr<views::Border> b) {
  LabelButton::SetBorder(std::move(b));
  PreferredSizeChanged();
}

void HoverButton::PreferredSizeChanged() {
  LabelButton::PreferredSizeChanged();
  if (GetLayoutManager()) {
    SetMinSize(GetLayoutManager()->GetPreferredSize(this));
  }
}

void HoverButton::OnViewBoundsChanged(View* observed_view) {
  LabelButton::OnViewBoundsChanged(observed_view);
  if (observed_view == label_wrapper_) {
    UpdateTooltipAndAccessibleName();
  }
}

void HoverButton::SetTitleTextStyle(views::style::TextStyle text_style,
                                    SkColor background_color,
                                    std::optional<ui::ColorId> color_id) {
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

void HoverButton::SetFooterTextStyle(int text_content,
                                     views::style::TextStyle text_style) {
  if (!footer()) {
    return;
  }

  footer()->SetTextContext(text_content);
  footer()->SetTextStyle(text_style);
  footer()->SetAutoColorReadabilityEnabled(true);

  // `footer_`'s preferred size may have changed. Notify the view because
  // `footer_` is an indirect child and thus
  // HoverButton::ChildPreferredSizeChanged() is not called.
  PreferredSizeChanged();
}

void HoverButton::SetIconHorizontalMargins(int left, int right) {
  int vertical_spacing = GetVerticalSpacing();
  icon_wrapper_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(vertical_spacing, left, vertical_spacing, right));
}

void HoverButton::UpdateTooltipAndAccessibleName() {
  std::vector<std::u16string_view> texts = {title_->GetText()};
  if (subtitle_) {
    texts.push_back(subtitle_->GetText());
  }
  if (footer_) {
    texts.push_back(footer_->GetText());
  }
  const std::u16string accessible_name = base::JoinString(texts, u"\n");

  // views::StyledLabels only add tooltips for any links they may have. However,
  // since HoverButton will never insert a link inside its child StyledLabel,
  // decide whether it needs a tooltip by checking whether the available space
  // is smaller than its preferred size.
  const bool needs_tooltip =
      label_wrapper_->GetPreferredSize().width() > label_wrapper_->width();
  SetTooltipText(needs_tooltip ? accessible_name : std::u16string());
  GetViewAccessibility().SetName(accessible_name);
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

void HoverButton::OnPressed(const ui::Event& event) {
  if (callback_) {
    callback_.Run(event);
  }
}

std::unique_ptr<views::Label> HoverButton::CreateSecondaryLabel(
    const std::u16string& text) {
  auto label = std::make_unique<views::Label>(
      text, views::style::CONTEXT_BUTTON, views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAutoColorReadabilityEnabled(false);
  // A subtitle text update may result in the same label size and not trigger
  // any observers. Thus, we need to add a callback that updates tooltip and
  // accessible name when subtitle text changes.
  text_changed_subscriptions_.push_back(label->AddTextChangedCallback(
      base::BindRepeating(&HoverButton::UpdateTooltipAndAccessibleName,
                          base::Unretained(this))));
  return label;
}

BEGIN_METADATA(HoverButton)
END_METADATA
