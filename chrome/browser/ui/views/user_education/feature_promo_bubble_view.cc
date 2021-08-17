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
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_associated_data.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/dot_indicator.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
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
constexpr gfx::Insets kBubbleContentsInsets(16, 20);

// The insets from the button border to the text inside.
constexpr gfx::Insets kBubbleButtonPadding(6, 16);

// The text color of the button.
constexpr SkColor kBubbleButtonTextColor = SK_ColorWHITE;

// The outline color of the button.
constexpr SkColor kBubbleButtonBorderColor = gfx::kGoogleGrey300;

// The focus ring color of the button.
constexpr SkColor kBubbleButtonFocusRingColor = SK_ColorWHITE;

// The background color of the button when focused.
constexpr SkColor kBubbleButtonFocusedBackgroundColor = gfx::kGoogleBlue600;

// The background color of the button when highlighted.
constexpr SkColor kBubbleButtonHighlightColor = gfx::kGoogleBlue300;

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
    views::FocusRing::Get(this)->SetColor(kBubbleButtonFocusRingColor);
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

class CloseTutorialButton : public views::ImageButton {
 public:
  METADATA_HEADER(CloseTutorialButton);
  explicit CloseTutorialButton(PressedCallback callback) {
    SetCallback(callback);
    SetImage(
        views::ImageButton::STATE_NORMAL,
        gfx::CreateVectorIcon(views::kIcCloseIcon, 16, kBubbleButtonTextColor));
    views::ConfigureVectorImageButton(this);
    views::HighlightPathGenerator::Install(
        this,
        std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));
    views::InkDrop::Get(this)->SetBaseColor(kBubbleButtonHighlightColor);
    SetProperty(views::kFlexBehaviorKey,
                views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                         views::MaximumFlexSizeRule::kUnbounded)
                    .WithAlignment(views::LayoutAlignment::kEnd));
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_CLOSE_TUTORIAL));
  }
};

BEGIN_METADATA(CloseTutorialButton, views::ImageButton)
END_METADATA

class DotView : public views::View {
 public:
  METADATA_HEADER(DotView);
  DotView(gfx::Size size, SkColor fill_color, SkColor stroke_color)
      : size_(size), fill_color_(fill_color), stroke_color_(stroke_color) {
    // In order to anti-alias properly, we'll grow by the stroke width and then
    // have the excess space be subtracted from the margins by the layout.
    SetProperty(views::kInternalPaddingKey, gfx::Insets(kStrokeWidth));
  }
  ~DotView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = size_;
    const gfx::Insets* const insets = GetProperty(views::kInternalPaddingKey);
    size.Enlarge(insets->width(), insets->height());
    return size;
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::RectF local_bounds = gfx::RectF(GetLocalBounds());
    DCHECK_GT(local_bounds.width(), size_.width());
    DCHECK_GT(local_bounds.height(), size_.height());
    const gfx::PointF center_point = local_bounds.CenterPoint();
    const float radius = (size_.width() - kStrokeWidth) / 2.0f;

    cc::PaintFlags fill_flags;
    fill_flags.setStyle(cc::PaintFlags::kFill_Style);
    fill_flags.setAntiAlias(true);
    fill_flags.setColor(fill_color_);
    canvas->DrawCircle(center_point, radius, fill_flags);

    cc::PaintFlags stroke_flags;
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(kStrokeWidth);
    stroke_flags.setAntiAlias(true);
    stroke_flags.setColor(stroke_color_);
    canvas->DrawCircle(center_point, radius, stroke_flags);
  }

 private:
  static constexpr int kStrokeWidth = 1;

  const gfx::Size size_;
  const SkColor fill_color_;
  const SkColor stroke_color_;
};

constexpr int DotView::kStrokeWidth;

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
        params.timeout_no_interaction ? *params.timeout_no_interaction
                                      : kDelayDefault,
        params.timeout_after_interaction ? *params.timeout_after_interaction
                                         : kDelayShort,
        params.timeout_callback);
  }

  const std::u16string body_text = std::move(params.body_text);

  if (params.screenreader_text)
    accessible_name_ = std::move(*params.screenreader_text);
  else
    accessible_name_ = body_text;

  // Since we don't have any controls for the user to interact with (we're just
  // an information bubble), override our role to kAlert.
  SetAccessibleRole(ax::mojom::Role::kAlert);

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

  // Add child views.
  views::View* top_row_container = nullptr;
  if (params.tutorial_progress_current || params.has_close_button)
    top_row_container = AddChildView(std::make_unique<views::View>());

  // Add progress indicator.
  if (params.tutorial_progress_current) {
    DCHECK(params.tutorial_progress_max);
    // TODO(crbug.com/1197208): surface progress information in a11y tree
    for (int i = 0; i < params.tutorial_progress_max; ++i) {
      SkColor fill_color = i < params.tutorial_progress_current
                               ? SK_ColorWHITE
                               : SK_ColorTRANSPARENT;
      // TODO(crbug.com/1197208): formalize dot size
      top_row_container->AddChildView(std::make_unique<DotView>(
          gfx::Size(8, 8), fill_color, SK_ColorWHITE));
    }
  }

  auto close_bubble_and_run_callback = [](FeaturePromoBubbleView* view,
                                          base::RepeatingClosure callback,
                                          const ui::Event& event) {
    view->CloseBubble();
    callback.Run();
  };

  // Add close button.
  if (params.has_close_button) {
    // Close button should only be used for tutorial use cases.
    DCHECK(params.tutorial_progress_current);
    top_row_container->AddChildView(
        std::make_unique<CloseTutorialButton>(base::BindRepeating(
            close_bubble_and_run_callback, base::Unretained(this),
            params.dismiss_callback.has_value()
                ? std::move(params.dismiss_callback.value())
                : base::DoNothing())));
  }

  // Add body icon (optional) and associated containers.
  views::View* outer_label_container = nullptr;
  views::View* inner_label_container = nullptr;
  views::ImageView* icon_view = nullptr;
  constexpr int kBodyIconSize = 24;
  if (params.body_icon) {
    outer_label_container = AddChildView(std::make_unique<views::View>());
    icon_view = outer_label_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            *params.body_icon, text_color, kBodyIconSize)));
    inner_label_container =
        outer_label_container->AddChildView(std::make_unique<views::View>());
  }

  views::View* const label_parent =
      inner_label_container ? inner_label_container : this;

  // Add title label.
  views::Label* title_label = nullptr;
  if (params.title_text.has_value()) {
    title_label = label_parent->AddChildView(std::make_unique<views::Label>(
        std::move(*params.title_text),
        ChromeTextContext::CONTEXT_IPH_BUBBLE_TITLE));
    title_label->SetBackgroundColor(background_color);
    title_label->SetEnabledColor(text_color);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetMultiLine(true);
    title_label->SetElideBehavior(gfx::NO_ELIDE);
  }

  // Add body label.
  auto* const body_label = label_parent->AddChildView(
      std::make_unique<views::Label>(body_text, CONTEXT_IPH_BUBBLE_BODY));
  body_label->SetBackgroundColor(background_color);
  body_label->SetEnabledColor(text_color);
  body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body_label->SetMultiLine(true);
  body_label->SetElideBehavior(gfx::NO_ELIDE);

  // Add buttons.
  views::View* button_container = nullptr;
  if (!params.buttons.empty()) {
    button_container = AddChildView(std::make_unique<views::View>());
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
    }
  }

  // Set up layouts. This is the default vertical spacing that is also used to
  // separate progress indicators for symmetry.
  // TODO(dfried): consider whether we could take font ascender and descender
  // height and factor them into margin calculations.
  const int default_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  // Create primary layout (vertical).
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(kBubbleContentsInsets)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(0, 0, default_spacing, 0))
      .SetIgnoreDefaultMainAxisMargins(true);

  // Set up top row container layout.
  const int kCloseButtonHeight = 24;
  if (top_row_container) {
    auto& top_layout =
        top_row_container
            ->SetLayoutManager(std::make_unique<views::FlexLayout>())
            ->SetOrientation(views::LayoutOrientation::kHorizontal)
            .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
            .SetMinimumCrossAxisSize(kCloseButtonHeight)
            .SetDefault(views::kMarginsKey,
                        gfx::Insets(0, default_spacing, 0, 0))
            .SetIgnoreDefaultMainAxisMargins(true);
    top_row_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(top_layout.GetDefaultFlexRule()));
  }

  // Set label flex properties. This ensures that if the width of the bubble
  // maxes out the text will shrink on the cross-axis and grow to multiple
  // lines without getting cut off.
  const views::FlexSpecification text_flex(
      views::LayoutOrientation::kVertical,
      views::MinimumFlexSizeRule::kPreferred,
      views::MaximumFlexSizeRule::kPreferred,
      /* adjust_height_for_width = */ true,
      views::MinimumFlexSizeRule::kScaleToMinimum);
  body_label->SetProperty(views::kFlexBehaviorKey, text_flex);
  if (title_label)
    title_label->SetProperty(views::kFlexBehaviorKey, text_flex);

  if (outer_label_container) {
    auto& outer_layout =
        outer_label_container
            ->SetLayoutManager(std::make_unique<views::FlexLayout>())
            ->SetOrientation(views::LayoutOrientation::kHorizontal)
            .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
            .SetIgnoreDefaultMainAxisMargins(true);
    outer_label_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(outer_layout.GetDefaultFlexRule()));

    auto& inner_layout =
        inner_label_container
            ->SetLayoutManager(std::make_unique<views::FlexLayout>())
            ->SetOrientation(views::LayoutOrientation::kVertical)
            .SetCollapseMargins(true)
            .SetDefault(views::kMarginsKey,
                        gfx::Insets(0, 0, default_spacing, 0))
            .SetIgnoreDefaultMainAxisMargins(true);
    inner_label_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(inner_layout.GetDefaultFlexRule()));

    // Align the midpoint of the first line of text with the icon.
    const gfx::FontList& first_line_font =
        title_label ? title_label->font_list() : body_label->font_list();
    const int to_text_midpoint =
        first_line_font.GetBaseline() - first_line_font.GetCapHeight() / 2;
    const int icon_top_margin = to_text_midpoint - kBodyIconSize / 2;
    icon_view->SetProperty(
        views::kMarginsKey,
        gfx::Insets(std::max(0, icon_top_margin), 0, 0, default_spacing));
    inner_label_container->SetProperty(
        views::kMarginsKey,
        gfx::Insets(std::max(0, -icon_top_margin), 0, 0, 0));
  }

  // Set up button container layout.
  if (button_container) {
    // Add in the default spacing between bubble content and bottom/buttons.
    button_container->SetProperty(
        views::kMarginsKey,
        gfx::Insets(layout_provider->GetDistanceMetric(
                        views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                    0, 0, 0));

    // Create button container internal layout.
    auto& button_layout =
        button_container
            ->SetLayoutManager(std::make_unique<views::FlexLayout>())
            ->SetOrientation(views::LayoutOrientation::kHorizontal)
            .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
            .SetDefault(
                views::kMarginsKey,
                gfx::Insets(0,
                            layout_provider->GetDistanceMetric(
                                views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                            0, 0))
            .SetIgnoreDefaultMainAxisMargins(true);
    button_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(button_layout.GetDefaultFlexRule()));
  }

  // Set up the bubble itself.

  set_close_on_deactivate(!params.persist_on_blur);

  set_margins(gfx::Insets());
  set_title_margins(gfx::Insets());
  SetButtons(ui::DIALOG_BUTTON_NONE);

  set_color(background_color);

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  auto* const frame_view = GetBubbleFrameView();
  frame_view->SetCornerRadius(
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh));
  frame_view->SetDisplayVisibleArrow(true);
  SizeToContents();

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

FeaturePromoBubbleTimeout* FeaturePromoBubbleView::GetTimeoutForTesting() {
  return feature_promo_bubble_timeout_.get();
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

std::u16string FeaturePromoBubbleView::GetAccessibleWindowTitle() const {
  return accessible_name_;
}

gfx::Size FeaturePromoBubbleView::CalculatePreferredSize() const {
  if (preferred_width_.has_value()) {
    return gfx::Size(preferred_width_.value(),
                     GetHeightForWidth(preferred_width_.value()));
  }

  const gfx::Size layout_manager_preferred_size =
      View::CalculatePreferredSize();

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
