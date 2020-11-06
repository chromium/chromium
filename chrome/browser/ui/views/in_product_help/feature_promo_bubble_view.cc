// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_timeout.h"
#include "chrome/grit/generated_resources.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
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
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
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

// The insets from the button border to the text inside.
constexpr gfx::Insets kBubbleButtonPadding(8, 10);

// The text color of the button.
constexpr SkColor kBubbleButtonTextColor = SK_ColorWHITE;

// The outline color of the button.
constexpr SkColor kBubbleButtonBorderColor = gfx::kGoogleGrey300;

// The focus ring color of the button.
constexpr SkColor kBubbleButtonFocusRingColor = SK_ColorWHITE;

// The background color of the button when focused.
constexpr SkColor kBubbleButtonFocusedBackgroundColor =
    gfx::kGoogleBlue600;
}  // namespace

namespace views {

class MdIPHBubbleButton : public MdTextButton {
 public:
  MdIPHBubbleButton(PressedCallback callback,
                    const base::string16& text,
                    bool has_border)
      : MdTextButton(callback,
                     text,
                     ChromeTextContext::CONTEXT_IPH_BUBBLE_BUTTON),
        has_border_(has_border) {
    // Prominent style gives a button hover highlight.
    SetProminent(true);
    // TODO(kerenzhu): IPH bubble uses blue600 as the background color
    // for both regular and dark mode. We might want to use a
    // dark-mode-appropriate background color so that overriding text color
    // is not needed.
    SetEnabledTextColors(kBubbleButtonTextColor);
    // TODO(crbug/1112244): Temporary fix for Mac. Bubble shouldn't be in
    // inactive style when the bubble loses focus.
    SetTextColor(ButtonState::STATE_DISABLED, kBubbleButtonTextColor);
    focus_ring()->SetColor(kBubbleButtonFocusRingColor);
    GetViewAccessibility().OverrideIsLeaf(true);
  }

  void UpdateBackgroundColor() override {
    // Prominent MD button does not have a border.
    // Override this method to draw a border.
    // Adapted from MdTextButton::UpdateBackgroundColor()
    ui::NativeTheme* theme = GetNativeTheme();

    // Default button background color is the same as IPH bubble's color.
    const SkColor kBubbleBackgroundColor = ThemeProperties::GetDefaultColor(
        ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND, false);

    SkColor bg_color = HasFocus() ? kBubbleButtonFocusedBackgroundColor
                                  : kBubbleBackgroundColor;
    if (GetState() == STATE_PRESSED)
      bg_color = theme->GetSystemButtonPressedColor(bg_color);

    SkColor stroke_color =
        has_border_ ? kBubbleButtonBorderColor : kBubbleBackgroundColor;

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
      focusable_(params.allow_focus),
      persist_on_blur_(params.persist_on_blur),
      snoozable_(params.allow_snooze),
      preferred_width_(params.preferred_width) {
  DCHECK(params.anchor_view);
  DCHECK(!params.allow_snooze || params.allow_focus)
      << "A snoozable bubble must be focusable to allow keyboard "
         "accessibility.";
  DCHECK(!params.persist_on_blur || params.allow_focus)
      << "A bubble that persists on blur must be focusable.";
  UseCompactMargins();

  // Bubble will not auto-dismiss for snoozble IPH.
  if (!snoozable_) {
    feature_promo_bubble_timeout_ = std::make_unique<FeaturePromoBubbleTimeout>(
        params.timeout_default ? *params.timeout_default : kDelayDefault,
        params.timeout_short ? *params.timeout_short : kDelayShort);
  }

  const base::string16 body_text =
      l10n_util::GetStringUTF16(params.body_string_specifier);

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
  const int text_vertical_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int button_vertical_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL);

  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBubbleContentsInsets,
      text_vertical_spacing);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  SetLayoutManager(std::move(box_layout));

  ChromeTextContext body_label_context;
  if (params.title_string_specifier.has_value()) {
    auto* title_label = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(params.title_string_specifier.value()),
        ChromeTextContext::CONTEXT_IPH_BUBBLE_TITLE));
    title_label->SetBackgroundColor(background_color);
    title_label->SetEnabledColor(text_color);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    if (params.preferred_width.has_value())
      title_label->SetMultiLine(true);

    body_label_context = CONTEXT_IPH_BUBBLE_BODY_WITH_TITLE;
  } else {
    body_label_context = CONTEXT_IPH_BUBBLE_BODY_WITHOUT_TITLE;
  }

  auto* body_label = AddChildView(
      std::make_unique<views::Label>(body_text, body_label_context));
  body_label->SetBackgroundColor(background_color);
  body_label->SetEnabledColor(text_color);
  body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (params.preferred_width.has_value())
    body_label->SetMultiLine(true);

  if (snoozable_) {
    auto* button_container = AddChildView(std::make_unique<views::View>());
    auto* button_layout =
        button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));

    button_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kEnd);
    button_container->SetProperty(
        views::kMarginsKey, gfx::Insets(button_vertical_spacing, 0, 0, 0));

    const base::string16 snooze_text =
        l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
    const base::string16 dismiss_text =
        l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
    bool dismiss_is_leading = views::PlatformStyle::kIsOkButtonLeading;

    auto close_bubble_and_run_callback = [](FeaturePromoBubbleView* view,
                                            base::RepeatingClosure callback,
                                            const ui::Event& event) {
      view->CloseBubble();
      callback.Run();
    };

    snooze_button_ = button_container->AddChildView(
        std::make_unique<views::MdIPHBubbleButton>(
            base::BindRepeating(close_bubble_and_run_callback,
                                base::Unretained(this), snooze_callback),
            snooze_text, false));
    dismiss_button_ = button_container->AddChildViewAt(
        std::make_unique<views::MdIPHBubbleButton>(
            base::BindRepeating(close_bubble_and_run_callback,
                                base::Unretained(this), dismiss_callback),
            dismiss_text, true),
        dismiss_is_leading ? 0 : 1);

    auto* leading_button =
        dismiss_is_leading ? dismiss_button_ : snooze_button_;
    leading_button->SetProperty(
        views::kMarginsKey,
        gfx::Insets(0, layout_provider->GetDistanceMetric(
                           views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

    // The text in dismiss button will be shorter than the default min size.
    // Set min size to 0 so that the custom padding is effective.
    dismiss_button_->SetMinSize(gfx::Size(0, 0));
    snooze_button_->SetCustomPadding(kBubbleButtonPadding);
    dismiss_button_->SetCustomPadding(kBubbleButtonPadding);
  }

  if (!focusable_) {
    SetCanActivate(false);
    set_shadow(views::BubbleBorder::BIG_SHADOW);
  }

  set_close_on_deactivate(!persist_on_blur_);

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

gfx::Rect FeaturePromoBubbleView::GetBubbleBounds() {
  gfx::Rect bounds = BubbleDialogDelegateView::GetBubbleBounds();
  if (!focusable_) {
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
