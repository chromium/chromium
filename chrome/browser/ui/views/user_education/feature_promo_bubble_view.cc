// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_timeout.h"
#include "chrome/grit/generated_resources.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/dot_indicator.h"
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

// Maximum width of the bubble. Longer strings will cause wrapping.
constexpr int kBubbleMaxWidthDip = 340;

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
constexpr SkColor kBubbleButtonFocusedBackgroundColor = gfx::kGoogleBlue600;

class MdIPHBubbleButton : public views::MdTextButton {
 public:
  METADATA_HEADER(MdIPHBubbleButton);

  MdIPHBubbleButton(PressedCallback callback,
                    const std::u16string& text,
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
  MdIPHBubbleButton(const MdIPHBubbleButton&) = delete;
  MdIPHBubbleButton& operator=(const MdIPHBubbleButton&) = delete;
  ~MdIPHBubbleButton() override = default;

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
        views::Painter::CreateRoundRectWith1PxBorderPainter(
            bg_color, stroke_color, GetCornerRadius())));
  }

 private:
  bool has_border_;
};

BEGIN_METADATA(MdIPHBubbleButton, views::MdTextButton)
END_METADATA

class DotView : public views::View {
 public:
  METADATA_HEADER(DotView);
  DotView(gfx::Size size, SkColor fill_color, SkColor stroke_color)
      : size_(size), fill_color_(fill_color), stroke_color_(stroke_color) {}
  ~DotView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override { return size_; }

  void OnPaint(gfx::Canvas* canvas) override {
    DCHECK_EQ(width(), height());

    const float kStrokeWidth = 1.0f;
    gfx::RectF local_bounds = gfx::RectF(GetLocalBounds());
    local_bounds.Inset(gfx::InsetsF(1.0f));
    const gfx::PointF center_point = local_bounds.CenterPoint();
    const float radius = local_bounds.width() / 2.0f;

    cc::PaintFlags stroke_flags;
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(kStrokeWidth);
    stroke_flags.setAntiAlias(true);
    stroke_flags.setColor(stroke_color_);
    canvas->DrawCircle(center_point, radius, stroke_flags);

    cc::PaintFlags fill_flags;
    fill_flags.setStyle(cc::PaintFlags::kFill_Style);
    fill_flags.setAntiAlias(true);
    fill_flags.setColor(fill_color_);
    canvas->DrawCircle(center_point, radius, fill_flags);
  }

 private:
  const gfx::Size size_;
  const SkColor fill_color_;
  const SkColor stroke_color_;
};

BEGIN_METADATA(DotView, views::View)
END_METADATA

}  // namespace

// Explicitly don't use the default DIALOG_SHADOW as it will show a black
// outline in dark mode on Mac. Use our own shadow instead. The shadow type is
// the same for all other platforms.
FeaturePromoBubbleView::FeaturePromoBubbleView(CreateParams params)
    : BubbleDialogDelegateView(params.anchor_view,
                               params.arrow,
                               views::BubbleBorder::STANDARD_SHADOW),
      preferred_width_(params.preferred_width) {
  DCHECK(params.anchor_view);
  DCHECK(params.persist_on_blur || params.focus_on_create)
      << "A bubble that closes on blur must be initially focused.";
  UseCompactMargins();

  // Bubble will not auto-dismiss if there's buttons.
  if (params.buttons.empty()) {
    feature_promo_bubble_timeout_ = std::make_unique<FeaturePromoBubbleTimeout>(
        params.timeout_default ? *params.timeout_default : kDelayDefault,
        params.timeout_short ? *params.timeout_short : kDelayShort);
  }

  const std::u16string body_text = std::move(params.body_text);

  if (params.screenreader_text)
    accessible_name_ = std::move(*params.screenreader_text);
  else
    accessible_name_ = body_text;

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

  if (params.tutorial_progress_current) {
    DCHECK(params.tutorial_progress_max);
    views::View* progress_indicator_container =
        AddChildView(std::make_unique<views::View>());
    views::BoxLayout* const box_layout =
        progress_indicator_container->SetLayoutManager(
            std::make_unique<views::BoxLayout>(
                views::BoxLayout::Orientation::kHorizontal));
    box_layout->set_between_child_spacing(text_vertical_spacing);

    // TODO(crbug.com/1197208): surface progress information in a11y tree

    for (int i = 0; i < params.tutorial_progress_max; ++i) {
      SkColor fill_color = i < params.tutorial_progress_current
                               ? SK_ColorWHITE
                               : SK_ColorTRANSPARENT;
      // TODO(crbug.com/1197208): formalize dot size
      progress_indicator_container->AddChildView(std::make_unique<DotView>(
          gfx::Size(8, 8), fill_color, SK_ColorWHITE));
    }
  }

  ChromeTextContext body_label_context;
  if (params.title_text.has_value()) {
    auto* title_label = AddChildView(std::make_unique<views::Label>(
        std::move(*params.title_text),
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
  body_label->SetMultiLine(true);

  if (!params.buttons.empty()) {
    auto* button_container = AddChildView(std::make_unique<views::View>());
    auto* button_layout =
        button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));

    button_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kEnd);
    button_container->SetProperty(
        views::kMarginsKey, gfx::Insets(button_vertical_spacing, 0, 0, 0));

    auto close_bubble_and_run_callback = [](FeaturePromoBubbleView* view,
                                            base::RepeatingClosure callback,
                                            const ui::Event& event) {
      view->CloseBubble();
      callback.Run();
    };

    const int button_spacing = layout_provider->GetDistanceMetric(
        views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

    bool is_first_button = true;
    for (ButtonParams& button_params : params.buttons) {
      MdIPHBubbleButton* const button =
          button_container->AddChildView(std::make_unique<MdIPHBubbleButton>(
              base::BindRepeating(close_bubble_and_run_callback,
                                  base::Unretained(this),
                                  std::move(button_params.callback)),
              std::move(button_params.text), button_params.has_border));
      buttons_.push_back(button);

      button->SetMinSize(gfx::Size(0, 0));
      button->SetCustomPadding(kBubbleButtonPadding);

      if (!is_first_button) {
        button->SetProperty(views::kMarginsKey,
                            gfx::Insets(0, button_spacing, 0, 0));
      }
      is_first_button = false;
    }
  }

  set_close_on_deactivate(!params.persist_on_blur);

  set_margins(gfx::Insets());
  set_title_margins(gfx::Insets());
  SetButtons(ui::DIALOG_BUTTON_NONE);

  set_color(background_color);

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  GetBubbleFrameView()->SetCornerRadius(
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh));

  if (params.focus_on_create)
    widget->Show();
  else
    widget->ShowInactive();

  if (feature_promo_bubble_timeout_)
    feature_promo_bubble_timeout_->OnBubbleShown(this);
}

FeaturePromoBubbleView::~FeaturePromoBubbleView() = default;

FeaturePromoBubbleView::ButtonParams::ButtonParams() = default;
FeaturePromoBubbleView::ButtonParams::ButtonParams(ButtonParams&&) = default;
FeaturePromoBubbleView::ButtonParams::~ButtonParams() = default;

FeaturePromoBubbleView::ButtonParams&
FeaturePromoBubbleView::ButtonParams::operator=(
    FeaturePromoBubbleView::ButtonParams&&) = default;

FeaturePromoBubbleView::CreateParams::CreateParams() = default;
FeaturePromoBubbleView::CreateParams::CreateParams(CreateParams&&) = default;
FeaturePromoBubbleView::CreateParams::~CreateParams() = default;

FeaturePromoBubbleView::CreateParams&
FeaturePromoBubbleView::CreateParams::operator=(
    FeaturePromoBubbleView::CreateParams&&) = default;

// static
FeaturePromoBubbleView* FeaturePromoBubbleView::Create(CreateParams params) {
  return new FeaturePromoBubbleView(std::move(params));
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

ax::mojom::Role FeaturePromoBubbleView::GetAccessibleWindowRole() {
  // Since we don't have any controls for the user to interact with (we're just
  // an information bubble), override our role to kAlert.
  return ax::mojom::Role::kAlert;
}

std::u16string FeaturePromoBubbleView::GetAccessibleWindowTitle() const {
  return accessible_name_;
}

gfx::Size FeaturePromoBubbleView::CalculatePreferredSize() const {
  if (preferred_width_.has_value()) {
    return gfx::Size(preferred_width_.value(),
                     GetHeightForWidth(preferred_width_.value()));
  }

  gfx::Size layout_manager_preferred_size = View::CalculatePreferredSize();

  // Wrap if the width is larger than |kBubbleMaxWidthDip|.
  if (layout_manager_preferred_size.width() > kBubbleMaxWidthDip) {
    return gfx::Size(kBubbleMaxWidthDip, GetHeightForWidth(kBubbleMaxWidthDip));
  }

  return layout_manager_preferred_size;
}

views::Button* FeaturePromoBubbleView::GetButtonForTesting(int index) const {
  return buttons_[index];
}

BEGIN_METADATA(FeaturePromoBubbleView, views::BubbleDialogDelegateView)
END_METADATA
