// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"

#include <memory>
#include <utility>

#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_timeout.h"
#include "chrome/grit/generated_resources.h"
#include "components/variations/variations_associated_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace {

// The amount of time the promo should stay onscreen if the user
// never hovers over it.
constexpr base::TimeDelta kDelayDefault = base::TimeDelta::FromSeconds(10);

// The amount of time the promo should stay onscreen after the
// user stops hovering over it.
constexpr base::TimeDelta kDelayShort = base::TimeDelta::FromSeconds(3);

// The insets from the bubble border to the text inside.
constexpr gfx::Insets kBubbleContentsInsets(12, 16);

constexpr gfx::Insets kBubbleButtonPadding(10, 10);

}  // namespace

namespace views {

class MdIPHBubbleButton : public MdTextButton {
 public:
  MdIPHBubbleButton(ButtonListener* listener,
                    const base::string16& text,
                    bool has_border)
      : MdTextButton(listener,
                     text,
                     ChromeTextContext::CONTEXT_IPH_BUBBLE_BUTTON),
        has_border_(has_border) {
    // Prominent style gives a button hover highlight.
    SetProminent(true);
    // Button color is the same as IPH bubble's color.
    SetBgColorOverride(SK_ColorTRANSPARENT);
  }

  void UpdateBackgroundColor() override {
    // Prominent MD button does not have a border.
    // Override this method to draw a border.
    // Adapted from MdTextButton::UpdateBackgroundColor()
    ui::NativeTheme* theme = GetNativeTheme();

    SkColor bg_color = SK_ColorTRANSPARENT;
    if (GetState() == STATE_PRESSED)
      theme->GetSystemButtonPressedColor(bg_color);

    SkColor stroke_color =
        has_border_
            ? theme->GetSystemColor(ui::NativeTheme::kColorId_ButtonBorderColor)
            : SK_ColorTRANSPARENT;

    SetBackground(CreateBackgroundFromPainter(
        Painter::CreateRoundRectWith1PxBorderPainter(bg_color, stroke_color,
                                                     GetCornerRadius())));
  }

 private:
  bool has_border_;
  DISALLOW_COPY_AND_ASSIGN(MdIPHBubbleButton);
};

}  // namespace views

FeaturePromoBubbleView::FeaturePromoBubbleView(
    const FeaturePromoBubbleParams& params,
    base::RepeatingClosure snooze_callback,
    base::RepeatingClosure dismiss_callback)
    : BubbleDialogDelegateView(params.anchor_view, params.arrow),
      snoozable_(params.allow_snooze),
      activation_action_(params.activation_action),
      preferred_width_(params.preferred_width),
      snooze_callback_(snooze_callback),
      dismiss_callback_(dismiss_callback) {
  DCHECK(params.anchor_view);
  UseCompactMargins();

  // Bubble will not auto-dismiss for snoozble IPH.
  if (!snoozable_) {
    feature_promo_bubble_timeout_ = std::make_unique<FeaturePromoBubbleTimeout>(
        params.timeout_default ? *params.timeout_default : kDelayDefault,
        params.timeout_short ? *params.timeout_short : kDelayShort);
  }

  const base::string16 body_text =
      l10n_util::GetStringUTF16(params.body_string_specifier);

  // Feature promos are purely informational. We can skip reading the UI
  // elements inside the bubble and just have the information announced when the
  // bubble shows. To do so, we change the a11y tree to make this a leaf node
  // and set the name to the message we want to announce.
  GetViewAccessibility().OverrideIsLeaf(true);
  if (!params.screenreader_string_specifier) {
    accessible_name_ = body_text;
  } else if (params.feature_accelerator) {
    accessible_name_ = l10n_util::GetStringFUTF16(
        *params.screenreader_string_specifier,
        params.feature_accelerator->GetShortcutText());
  } else {
    accessible_name_ =
        l10n_util::GetStringUTF16(*params.screenreader_string_specifier);
  }

  // We get the theme provider from the anchor view since our widget hasn't been
  // created yet.
  const ui::ThemeProvider* theme_provider =
      params.anchor_view->GetThemeProvider();
  const views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  DCHECK(theme_provider);
  DCHECK(layout_provider);

  const SkColor background_color = theme_provider->GetColor(
      ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND);
  const SkColor text_color = theme_provider->GetColor(
      ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_TEXT);
  const int vertical_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL);

  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBubbleContentsInsets,
      vertical_spacing);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  SetLayoutManager(std::move(box_layout));

  if (params.title_string_specifier.has_value()) {
    auto* title_label = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(params.title_string_specifier.value())));
    title_label->SetBackgroundColor(background_color);
    title_label->SetEnabledColor(text_color);
    title_label->SetFontList(views::style::GetFont(
        views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));

    if (params.preferred_width.has_value()) {
      title_label->SetMultiLine(true);
      title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
  }

  auto* body_label = AddChildView(std::make_unique<views::Label>(body_text));
  body_label->SetBackgroundColor(background_color);
  body_label->SetEnabledColor(text_color);

  if (params.preferred_width.has_value()) {
    body_label->SetMultiLine(true);
    body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  if (snoozable_) {
    auto* button_container = AddChildView(std::make_unique<views::View>());
    auto* button_layout =
        button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));

    button_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kEnd);

    const base::string16 skip_text =
        l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);

    dismiss_button_ = button_container->AddChildView(
        std::make_unique<views::MdIPHBubbleButton>(this, skip_text, false));
    dismiss_button_->SetCustomPadding(kBubbleButtonPadding);
    dismiss_button_->SetProperty(
        views::kMarginsKey,
        gfx::Insets(0, layout_provider->GetDistanceMetric(
                           views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

    const base::string16 snooze_text =
        l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);

    snooze_button_ = button_container->AddChildView(
        std::make_unique<views::MdIPHBubbleButton>(this, snooze_text, true));
    snooze_button_->SetCustomPadding(kBubbleButtonPadding);
  }

  if (params.activation_action ==
      FeaturePromoBubbleParams::ActivationAction::DO_NOT_ACTIVATE) {
    SetCanActivate(false);
    set_shadow(views::BubbleBorder::BIG_SHADOW);
  }

  set_margins(gfx::Insets());
  set_title_margins(gfx::Insets());
  SetButtons(ui::DIALOG_BUTTON_NONE);

  set_color(background_color);

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  GetBubbleFrameView()->SetCornerRadius(
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_HIGH));

  widget->Show();
  if (feature_promo_bubble_timeout_)
    feature_promo_bubble_timeout_->OnBubbleShown(this);
}

FeaturePromoBubbleView::~FeaturePromoBubbleView() = default;

// static
FeaturePromoBubbleView* FeaturePromoBubbleView::Create(
    const FeaturePromoBubbleParams& params,
    base::RepeatingClosure snooze_callback,
    base::RepeatingClosure dismiss_callback) {
  return new FeaturePromoBubbleView(params, snooze_callback, dismiss_callback);
}

void FeaturePromoBubbleView::CloseBubble() {
  GetWidget()->Close();
}

bool FeaturePromoBubbleView::OnMousePressed(const ui::MouseEvent& event) {
  base::RecordAction(
      base::UserMetricsAction("InProductHelp.Promos.BubbleClicked"));
  return false;
}

void FeaturePromoBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  if (feature_promo_bubble_timeout_)
    feature_promo_bubble_timeout_->OnMouseEntered();
}

void FeaturePromoBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  if (feature_promo_bubble_timeout_)
    feature_promo_bubble_timeout_->OnMouseExited();
}

void FeaturePromoBubbleView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  CloseBubble();
  if (sender == snooze_button_)
    snooze_callback_.Run();
  else  // sender == dismiss_button_
    dismiss_callback_.Run();
}

gfx::Rect FeaturePromoBubbleView::GetBubbleBounds() {
  gfx::Rect bounds = BubbleDialogDelegateView::GetBubbleBounds();
  if (activation_action_ ==
      FeaturePromoBubbleParams::ActivationAction::DO_NOT_ACTIVATE) {
    if (base::i18n::IsRTL())
      bounds.Offset(5, 0);
    else
      bounds.Offset(-5, 0);
  }
  return bounds;
}

ax::mojom::Role FeaturePromoBubbleView::GetAccessibleWindowRole() {
  // Since we don't have any controls for the user to interact with (we're just
  // an information bubble), override our role to kAlert.
  return ax::mojom::Role::kAlert;
}

base::string16 FeaturePromoBubbleView::GetAccessibleWindowTitle() const {
  return accessible_name_;
}

gfx::Size FeaturePromoBubbleView::CalculatePreferredSize() const {
  if (preferred_width_.has_value()) {
    return gfx::Size(preferred_width_.value(),
                     GetHeightForWidth(preferred_width_.value()));
  } else {
    return View::CalculatePreferredSize();
  }
}

views::Button* FeaturePromoBubbleView::GetDismissButtonForTesting() const {
  return dismiss_button_;
}

views::Button* FeaturePromoBubbleView::GetSnoozeButtonForTesting() const {
  return snooze_button_;
}
