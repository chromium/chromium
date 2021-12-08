// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"

#include <initializer_list>
#include <memory>
#include <numeric>
#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
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
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
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

// The amount of time the promo should stay onscreen.
constexpr base::TimeDelta kDefaultTimeoutWithoutButtons = base::Seconds(10);
constexpr base::TimeDelta kDefaultTimeoutWithButtons = base::Seconds(0);

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

// The background color of the button when highlighted.
constexpr SkColor kBubbleButtonHighlightColor = gfx::kGoogleBlue300;

class MdIPHBubbleButton : public views::MdTextButton {
 public:
  METADATA_HEADER(MdIPHBubbleButton);

  MdIPHBubbleButton(PressedCallback callback,
                    const std::u16string& text,
                    bool is_default_button)
      : MdTextButton(callback,
                     text,
                     ChromeTextContext::CONTEXT_IPH_BUBBLE_BUTTON),
        button_colors_(ComputeButtonColors(is_default_button)) {
    // Prominent style gives a button hover highlight.
    SetProminent(true);
    // TODO(kerenzhu): IPH bubble uses blue600 as the background color
    // for both regular and dark mode. We might want to use a
    // dark-mode-appropriate background color so that overriding text color
    // is not needed.
    SetEnabledTextColors(button_colors_.text_color);
    // TODO(crbug/1112244): Temporary fix for Mac. Bubble shouldn't be in
    // inactive style when the bubble loses focus.
    SetTextColor(ButtonState::STATE_DISABLED, button_colors_.text_color);
    views::FocusRing::Get(this)->SetColor(button_colors_.focus_ring_color);
    GetViewAccessibility().OverrideIsLeaf(true);
  }
  MdIPHBubbleButton(const MdIPHBubbleButton&) = delete;
  MdIPHBubbleButton& operator=(const MdIPHBubbleButton&) = delete;
  ~MdIPHBubbleButton() override = default;

  void UpdateBackgroundColor() override {
    // Prominent MD button does not have a border.
    // Override this method to draw a border.
    // Adapted from MdTextButton::UpdateBackgroundColor()
    SkColor bg_color = button_colors_.background_color;
    if (GetState() == STATE_PRESSED)
      bg_color = GetNativeTheme()->GetSystemButtonPressedColor(bg_color);

    SetBackground(CreateBackgroundFromPainter(
        views::Painter::CreateRoundRectWith1PxBorderPainter(
            bg_color, button_colors_.background_stroke_color,
            GetCornerRadius())));
  }

 private:
  struct ButtonColors {
    SkColor text_color;
    SkColor background_color;
    SkColor background_stroke_color;
    SkColor focus_ring_color;
  };

  static ButtonColors ComputeButtonColors(bool is_default_button) {
    const SkColor kBubbleBackgroundColor = ThemeProperties::GetDefaultColor(
        ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND, false);
    ButtonColors button_colors;
    button_colors.text_color =
        is_default_button ? kBubbleBackgroundColor : kBubbleButtonTextColor;
    button_colors.background_color =
        is_default_button ? kBubbleButtonTextColor : kBubbleBackgroundColor;
    button_colors.background_stroke_color =
        is_default_button ? kBubbleButtonTextColor : kBubbleButtonBorderColor;
    button_colors.focus_ring_color = kBubbleBackgroundColor;
    return button_colors;
  }

  ButtonColors button_colors_;
};

BEGIN_METADATA(MdIPHBubbleButton, views::MdTextButton)
END_METADATA

// Displays a simple "X" close button that will close a promo bubble view.
// The alt-text and button callback can be set based on the needs of the
// specific bubble.
class ClosePromoButton : public views::ImageButton {
 public:
  METADATA_HEADER(ClosePromoButton);
  ClosePromoButton(int accessible_name_id, PressedCallback callback) {
    SetCallback(callback);
    SetImage(
        views::ImageButton::STATE_NORMAL,
        gfx::CreateVectorIcon(views::kIcCloseIcon, 16, kBubbleButtonTextColor));
    views::ConfigureVectorImageButton(this);
    views::HighlightPathGenerator::Install(
        this,
        std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));
    views::InkDrop::Get(this)->SetBaseColor(kBubbleButtonHighlightColor);
    SetAccessibleName(l10n_util::GetStringUTF16(accessible_name_id));
  }
};

BEGIN_METADATA(ClosePromoButton, views::ImageButton)
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

  // Default timeout depends on whether non-close buttons are present.
  timeout_ = params.timeout.value_or(params.buttons.empty()
                                         ? kDefaultTimeoutWithoutButtons
                                         : kDefaultTimeoutWithButtons);
  if (!timeout_.is_zero())
    timeout_callback_ = std::move(params.timeout_callback);

  accessible_name_ = params.screenreader_text.empty()
                         ? params.body_text
                         : params.screenreader_text;
  screenreader_hint_text_ = params.keyboard_navigation_hint;

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

  // Layout structure:
  //
  // [***ooo      x]  <--- progress container
  // [@ TITLE     x]  <--- top text container
  //    body text
  // [    cancel ok]  <--- button container
  //
  // Notes:
  // - The close button's placement depends on the presence of a progress
  //   indicator.
  // - The body text takes the place of TITLE if there is no title.
  // - If there is both a title and icon, the body text is manually indented to
  //   align with the title; this avoids having to nest an additional vertical
  //   container.
  // - Unused containers are set to not be visible.
  views::View* const progress_container =
      AddChildView(std::make_unique<views::View>());
  views::View* const top_text_container =
      AddChildView(std::make_unique<views::View>());
  views::View* const button_container =
      AddChildView(std::make_unique<views::View>());

  // Add progress indicator (optional) and its container.
  if (params.tutorial_progress_current) {
    DCHECK(params.tutorial_progress_max);
    // TODO(crbug.com/1197208): surface progress information in a11y tree
    for (int i = 0; i < params.tutorial_progress_max; ++i) {
      SkColor fill_color = i < params.tutorial_progress_current
                               ? SK_ColorWHITE
                               : SK_ColorTRANSPARENT;
      // TODO(crbug.com/1197208): formalize dot size
      progress_container->AddChildView(std::make_unique<DotView>(
          gfx::Size(8, 8), fill_color, SK_ColorWHITE));
    }
  } else {
    progress_container->SetVisible(false);
  }

  // Add the body icon (optional).
  views::ImageView* icon_view = nullptr;
  constexpr int kBodyIconSize = 20;
  constexpr int kBodyIconBackgroundSize = 24;
  if (params.body_icon) {
    icon_view = top_text_container->AddChildViewAt(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            *params.body_icon, background_color, kBodyIconSize)),
        0);
    icon_view->SetPreferredSize(
        gfx::Size(kBodyIconBackgroundSize, kBodyIconBackgroundSize));
    icon_view->SetBackground(views::CreateRoundedRectBackground(
        text_color, kBodyIconBackgroundSize / 2));
    icon_view->SetAccessibleName(l10n_util::GetStringUTF16(IDS_CHROME_TIP));
  }

  // This callback is used by both the close button and additional buttons.
  auto close_bubble_and_run_callback = [](FeaturePromoBubbleView* view,
                                          base::RepeatingClosure callback,
                                          const ui::Event& event) {
    if (view->GetWidget())
      view->CloseBubble();
    if (callback)
      callback.Run();
  };

  std::vector<views::Label*> labels;
  // Add title (optional) and body label.
  if (!params.title_text.empty()) {
    labels.push_back(
        top_text_container->AddChildView(std::make_unique<views::Label>(
            params.title_text, ChromeTextContext::CONTEXT_IPH_BUBBLE_TITLE)));
    labels.push_back(
        AddChildViewAt(std::make_unique<views::Label>(params.body_text,
                                                      CONTEXT_IPH_BUBBLE_BODY),
                       GetIndexOf(button_container)));
  } else {
    labels.push_back(
        top_text_container->AddChildView(std::make_unique<views::Label>(
            params.body_text, CONTEXT_IPH_BUBBLE_BODY)));
  }

  // Set common label properties.
  for (views::Label* label : labels) {
    label->SetBackgroundColor(background_color);
    label->SetEnabledColor(text_color);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetMultiLine(true);
    label->SetElideBehavior(gfx::NO_ELIDE);
  }

  // Add close button (optional).
  ClosePromoButton* close_button = nullptr;
  if (params.has_close_button) {
    int close_string_id =
        params.tutorial_progress_current ? IDS_CLOSE_TUTORIAL : IDS_CLOSE_PROMO;
    close_button =
        (params.tutorial_progress_max ? progress_container : top_text_container)
            ->AddChildView(std::make_unique<ClosePromoButton>(
                close_string_id,
                base::BindRepeating(close_bubble_and_run_callback,
                                    base::Unretained(this),
                                    std::move(params.dismiss_callback))));
  }

  // Add other buttons.
  for (ButtonParams& button_params : params.buttons) {
    MdIPHBubbleButton* const button =
        button_container->AddChildView(std::make_unique<MdIPHBubbleButton>(
            base::BindRepeating(close_bubble_and_run_callback,
                                base::Unretained(this),
                                std::move(button_params.callback)),
            button_params.text, button_params.has_border));
    buttons_.push_back(button);
    button->SetMinSize(gfx::Size(0, 0));
    button->SetCustomPadding(kBubbleButtonPadding);
  }
  if (params.buttons.empty())
    button_container->SetVisible(false);

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
  auto& progress_layout =
      progress_container
          ->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetMinimumCrossAxisSize(kCloseButtonHeight)
          .SetDefault(views::kMarginsKey, gfx::Insets(0, default_spacing, 0, 0))
          .SetIgnoreDefaultMainAxisMargins(true);
  progress_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(progress_layout.GetDefaultFlexRule()));

  // Close button should float right in whatever container it's in.
  if (close_button) {
    close_button->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded)
            .WithAlignment(views::LayoutAlignment::kEnd));
    close_button->SetProperty(views::kMarginsKey,
                              gfx::Insets(0, default_spacing, 0, 0));
  }

  // Icon view should have padding between it and the title or body label.
  if (icon_view) {
    icon_view->SetProperty(views::kMarginsKey,
                           gfx::Insets(0, 0, 0, default_spacing));
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

  for (views::Label* label : labels)
    label->SetProperty(views::kFlexBehaviorKey, text_flex);

  auto& top_text_layout =
      top_text_container
          ->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .SetIgnoreDefaultMainAxisMargins(true);
  top_text_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(top_text_layout.GetDefaultFlexRule()));

  // If the body icon is present, labels after the first are not parented to
  // the top text container, but still need to be inset to align with the
  // title.
  if (icon_view) {
    const int indent = kBubbleContentsInsets.left() + kBodyIconBackgroundSize +
                       default_spacing;
    for (size_t i = 1; i < labels.size(); ++i)
      labels[i]->SetProperty(views::kMarginsKey, gfx::Insets(0, indent, 0, 0));
  }

  // Set up button container layout.
  // Add in the default spacing between bubble content and bottom/buttons.
  button_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets(layout_provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0, 0, 0));

  // Create button container internal layout.
  auto& button_layout =
      button_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
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

  // Set up the bubble itself.

  set_close_on_deactivate(!params.persist_on_blur);

  // Want a consistent initial focused view if one is available.
  if (close_button)
    SetInitiallyFocusedView(close_button);
  else if (!button_container->children().empty())
    SetInitiallyFocusedView(button_container->children()[0]);

  set_margins(gfx::Insets());
  set_title_margins(gfx::Insets());
  SetButtons(ui::DIALOG_BUTTON_NONE);

  set_color(background_color);

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  auto* const frame_view = GetBubbleFrameView();
  frame_view->SetCornerRadius(
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh));
  frame_view->SetDisplayVisibleArrow(params.arrow !=
                                     views::BubbleBorder::Arrow::NONE);
  SizeToContents();

  if (params.focus_on_create) {
    widget->Show();
  } else {
    widget->ShowInactive();
    MaybeStartAutoCloseTimer();
  }
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

void FeaturePromoBubbleView::MaybeStartAutoCloseTimer() {
  if (timeout_.is_zero())
    return;

  auto_close_timer_.Start(FROM_HERE, timeout_, this,
                          &FeaturePromoBubbleView::OnTimeout);
}

void FeaturePromoBubbleView::OnTimeout() {
  if (timeout_callback_)
    timeout_callback_.Run();
  CloseBubble();
}

void FeaturePromoBubbleView::CloseBubble() {
  GetWidget()->Close();
}

bool FeaturePromoBubbleView::OnMousePressed(const ui::MouseEvent& event) {
  base::RecordAction(
      base::UserMetricsAction("InProductHelp.Promos.BubbleClicked"));
  return false;
}

std::u16string FeaturePromoBubbleView::GetAccessibleWindowTitle() const {
  std::u16string result = accessible_name_;

  // If there's a keyboard navigation hint, append it after a full stop.
  if (!screenreader_hint_text_.empty() && activate_count_ <= 1)
    result += u". " + screenreader_hint_text_;

  return result;
}

void FeaturePromoBubbleView::OnWidgetActivationChanged(views::Widget* widget,
                                                       bool active) {
  if (widget == GetWidget()) {
    if (active) {
      ++activate_count_;
      auto_close_timer_.AbandonAndStop();
    } else {
      MaybeStartAutoCloseTimer();
    }
  }
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
