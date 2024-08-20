// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_view.h"

#include <initializer_list>
#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_event_relay.h"
#include "components/variations/variations_associated_data.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
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
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
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
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace user_education {

namespace {

// Translates from HelpBubbleArrow to the Views equivalent.
views::BubbleBorder::Arrow TranslateArrow(HelpBubbleArrow arrow) {
  switch (arrow) {
    case HelpBubbleArrow::kNone:
      return views::BubbleBorder::NONE;
    case HelpBubbleArrow::kTopLeft:
      return views::BubbleBorder::TOP_LEFT;
    case HelpBubbleArrow::kTopRight:
      return views::BubbleBorder::TOP_RIGHT;
    case HelpBubbleArrow::kBottomLeft:
      return views::BubbleBorder::BOTTOM_LEFT;
    case HelpBubbleArrow::kBottomRight:
      return views::BubbleBorder::BOTTOM_RIGHT;
    case HelpBubbleArrow::kLeftTop:
      return views::BubbleBorder::LEFT_TOP;
    case HelpBubbleArrow::kRightTop:
      return views::BubbleBorder::RIGHT_TOP;
    case HelpBubbleArrow::kLeftBottom:
      return views::BubbleBorder::LEFT_BOTTOM;
    case HelpBubbleArrow::kRightBottom:
      return views::BubbleBorder::RIGHT_BOTTOM;
    case HelpBubbleArrow::kTopCenter:
      return views::BubbleBorder::TOP_CENTER;
    case HelpBubbleArrow::kBottomCenter:
      return views::BubbleBorder::BOTTOM_CENTER;
    case HelpBubbleArrow::kLeftCenter:
      return views::BubbleBorder::LEFT_CENTER;
    case HelpBubbleArrow::kRightCenter:
      return views::BubbleBorder::RIGHT_CENTER;
  }
}

class MdIPHBubbleButton : public views::MdTextButton {
  METADATA_HEADER(MdIPHBubbleButton, views::MdTextButton)

 public:
  MdIPHBubbleButton(const HelpBubbleDelegate* delegate,
                    PressedCallback callback,
                    const std::u16string& text,
                    bool is_default_button)
      : MdTextButton(std::move(callback), text),
        delegate_(delegate),
        is_default_button_(is_default_button) {
    GetViewAccessibility().SetIsLeaf(true);

    views::FocusRing::Get(this)->SetColorId(
        delegate_->GetHelpBubbleForegroundColorId());

    ui::ColorId foreground_color =
        is_default_button_
            ? delegate_->GetHelpBubbleDefaultButtonForegroundColorId()
            : delegate_->GetHelpBubbleForegroundColorId();
    SetEnabledTextColorIds(foreground_color);
    // TODO(crbug.com/40709599): Temporary fix for Mac. Bubble shouldn't be in
    // inactive style when the bubble loses focus.
    SetTextColorId(ButtonState::STATE_DISABLED, foreground_color);

    // The default behavior in 2023 refresh is for MD buttons is to have the
    // alpha baked into the color, but we currently don't have that yet, so
    // switch back to using the old default alpha blending mode.
    auto* const ink_drop = views::InkDrop::Get(this);
    ink_drop->SetBaseColorId(
        is_default_button_
            ? delegate_->GetHelpBubbleDefaultButtonForegroundColorId()
            : delegate_->GetHelpBubbleForegroundColorId());
    ink_drop->SetHighlightOpacity(std::nullopt);
  }
  MdIPHBubbleButton(const MdIPHBubbleButton&) = delete;
  MdIPHBubbleButton& operator=(const MdIPHBubbleButton&) = delete;
  ~MdIPHBubbleButton() override = default;

  void UpdateBackgroundColor() override {
    // Prominent MD button does not have a border.
    // Override this method to draw a border.
    // Adapted from MdTextButton::UpdateBackgroundColor()
    const auto* color_provider = GetColorProvider();
    if (!color_provider)
      return;
    SkColor background_color = color_provider->GetColor(
        is_default_button_
            ? delegate_->GetHelpBubbleDefaultButtonBackgroundColorId()
            : delegate_->GetHelpBubbleBackgroundColorId());
    if (GetState() == STATE_PRESSED) {
      background_color =
          GetNativeTheme()->GetSystemButtonPressedColor(background_color);
    }
    const SkColor stroke_color = color_provider->GetColor(
        is_default_button_
            ? delegate_->GetHelpBubbleDefaultButtonBackgroundColorId()
            : delegate_->GetHelpBubbleButtonBorderColorId());
    SetBackground(CreateBackgroundFromPainter(
        views::Painter::CreateRoundRectWith1PxBorderPainter(
            background_color, stroke_color, GetCornerRadiusValue())));
  }

 private:
  const raw_ptr<const HelpBubbleDelegate> delegate_;
  bool is_default_button_;
};

BEGIN_METADATA(MdIPHBubbleButton)
END_METADATA

// Displays a simple "X" close button that will close a promo bubble view.
// The alt-text and button callback can be set based on the needs of the
// specific bubble.
class ClosePromoButton : public views::ImageButton {
  METADATA_HEADER(ClosePromoButton, views::ImageButton)

 public:
  ClosePromoButton(const HelpBubbleDelegate* delegate,
                   const std::u16string accessible_name,
                   PressedCallback callback)
      : delegate_(delegate) {
    SetCallback(std::move(callback));
    views::ConfigureVectorImageButton(this);
    views::HighlightPathGenerator::Install(
        this,
        std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));
    GetViewAccessibility().SetName(accessible_name);
    SetTooltipText(accessible_name);

    constexpr int kIconSize = 16;
    SetImageModel(views::ImageButton::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      views::kIcCloseIcon,
                      delegate_->GetHelpBubbleForegroundColorId(), kIconSize));

    constexpr float kCloseButtonFocusRingHaloThickness = 1.25f;
    views::FocusRing::Get(this)->SetHaloThickness(
        kCloseButtonFocusRingHaloThickness);
  }

  void OnThemeChanged() override {
    views::ImageButton::OnThemeChanged();
    const auto* color_provider = GetColorProvider();
    views::InkDrop::Get(this)->SetBaseColor(color_provider->GetColor(
        delegate_->GetHelpBubbleCloseButtonInkDropColorId()));
    views::FocusRing::Get(this)->SetColorId(
        delegate_->GetHelpBubbleForegroundColorId());
  }

 private:
  const raw_ptr<const HelpBubbleDelegate> delegate_;
};

BEGIN_METADATA(ClosePromoButton)
END_METADATA

class DotView : public views::View {
  METADATA_HEADER(DotView, views::View)

 public:
  DotView(const HelpBubbleDelegate* delegate, gfx::Size size, bool should_fill)
      : delegate_(delegate), size_(size), should_fill_(should_fill) {
    // In order to anti-alias properly, we'll grow by the stroke width and then
    // have the excess space be subtracted from the margins by the layout.
    SetProperty(views::kInternalPaddingKey, gfx::Insets(kStrokeWidth));
  }
  ~DotView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
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

    const SkColor color = GetColorProvider()->GetColor(
        delegate_->GetHelpBubbleForegroundColorId());
    if (should_fill_) {
      cc::PaintFlags fill_flags;
      fill_flags.setStyle(cc::PaintFlags::kFill_Style);
      fill_flags.setAntiAlias(true);
      fill_flags.setColor(color);
      canvas->DrawCircle(center_point, radius, fill_flags);
    }

    cc::PaintFlags stroke_flags;
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(kStrokeWidth);
    stroke_flags.setAntiAlias(true);
    stroke_flags.setColor(color);
    canvas->DrawCircle(center_point, radius, stroke_flags);
  }

 private:
  static constexpr int kStrokeWidth = 1;

  raw_ptr<const HelpBubbleDelegate> delegate_;
  const gfx::Size size_;
  const bool should_fill_;
};

constexpr int DotView::kStrokeWidth;

BEGIN_METADATA(DotView)
END_METADATA

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleView,
                                      kHelpBubbleElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleView,
                                      kDefaultButtonIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleView,
                                      kFirstNonDefaultButtonIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleView, kCloseButtonIdForTesting);

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleView, kBodyTextIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleView, kTitleTextIdForTesting);

// Watches for the anchor view to be destroyed or removed from its widget.
// Used in cases where the anchor element is not the same as the anchor view.
// Prevents the help bubble from lingering after its anchor is invalid, which
// can cause crashes and other strange behavior.
class HelpBubbleView::AnchorViewObserver : public views::ViewObserver {
 public:
  AnchorViewObserver(views::View* anchor_view, HelpBubbleView* help_bubble)
      : help_bubble_(help_bubble) {
    observation_.Observe(anchor_view);
  }

 private:
  // views::ViewObserver:
  void OnViewRemovedFromWidget(views::View*) override { Detach(); }
  void OnViewIsDeleting(views::View*) override { Detach(); }

  void Detach() {
    observation_.Reset();
    if (help_bubble_->GetWidget() && !help_bubble_->GetWidget()->IsClosed()) {
      help_bubble_->GetWidget()->Close();
    }
  }

  const raw_ptr<HelpBubbleView> help_bubble_;
  base::ScopedObservation<View, ViewObserver> observation_{this};
};

HelpBubbleView::HelpBubbleView(
    const HelpBubbleDelegate* delegate,
    const internal::HelpBubbleAnchorParams& anchor,
    HelpBubbleParams params,
    std::unique_ptr<HelpBubbleEventRelay> event_relay)
    : BubbleDialogDelegateView(
          anchor.view,
          TranslateArrow(params.arrow),
#if BUILDFLAG(IS_MAC)
          // On Mac, the default DIALOG_SHADOW is system-drawn, which is
          // incompatible with visible bubble arrows. Therefore, always use
          // STANDARD_SHADOW.
          views::BubbleBorder::STANDARD_SHADOW
#else
          // On other platforms, all shadows are Views-drawn; use the (slightly
          // better-looking) default DIALOG_SHADOW.
          views::BubbleBorder::DIALOG_SHADOW
#endif
          ,
          true),
      delegate_(delegate),
      event_relay_(std::move(event_relay)) {
  if (anchor.rect.has_value()) {
    SetForceAnchorRect(anchor.rect.value());
    anchor_observer_ = std::make_unique<AnchorViewObserver>(anchor.view, this);
  } else {
    // When hosted within a `views::ScrollView`, the anchor view may be
    // (partially) outside the viewport. Ensure that the anchor view is visible.
    anchor.view->ScrollViewToVisible();
  }
  DCHECK(anchor.view)
      << "A bubble that closes on blur must be initially focused.";
  UseCompactMargins();

  // Default timeout depends on whether non-close buttons are present.
  timeout_ = params.timeout.value_or(params.buttons.empty()
                                         ? kDefaultTimeoutWithoutButtons
                                         : kDefaultTimeoutWithButtons);
  timeout_callback_ = std::move(params.timeout_callback);
  SetCancelCallback(std::move(params.dismiss_callback));

  accessible_name_ = params.title_text;
  if (!accessible_name_.empty())
    accessible_name_ += u". ";
  accessible_name_ += params.screenreader_text.empty()
                          ? params.body_text
                          : params.screenreader_text;
  screenreader_hint_text_ = params.keyboard_navigation_hint;

  // Since we don't have any controls for the user to interact with (we're just
  // an information bubble), override our role to kAlert.
  SetAccessibleWindowRole(ax::mojom::Role::kAlert);

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
  if (params.progress) {
    DCHECK(params.progress->second);
    // TODO(crbug.com/40176811): surface progress information in a11y tree
    for (int i = 0; i < params.progress->second; ++i) {
      // TODO(crbug.com/40176811): formalize dot size
      progress_container->AddChildView(std::make_unique<DotView>(
          delegate, gfx::Size(8, 8), i < params.progress->first));
    }
  } else {
    progress_container->SetVisible(false);
  }

  // Add the body icon (optional).
  constexpr int kBodyIconSize = 20;
  constexpr int kBodyIconBackgroundSize = 24;
  if (params.body_icon) {
    icon_view_ = top_text_container->AddChildViewAt(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            *params.body_icon, delegate->GetHelpBubbleBackgroundColorId(),
            kBodyIconSize)),
        0);
    icon_view_->SetPreferredSize(
        gfx::Size(kBodyIconBackgroundSize, kBodyIconBackgroundSize));
    icon_view_->GetViewAccessibility().SetName(params.body_icon_alt_text);
  }

  // Add title (optional) and body label.
  if (!params.title_text.empty()) {
    views::Label* const title_label =
        top_text_container->AddChildView(std::make_unique<views::Label>(
            params.title_text, delegate->GetTitleTextContext()));
    title_label->SetProperty(views::kElementIdentifierKey,
                             kTitleTextIdForTesting);
    labels_.push_back(title_label);
    views::Label* const body_label =
        AddChildViewAt(std::make_unique<views::Label>(
                           params.body_text, delegate->GetBodyTextContext()),
                       GetIndexOf(button_container).value());
    labels_.push_back(body_label);
    body_label->SetProperty(views::kElementIdentifierKey,
                            kBodyTextIdForTesting);
  } else {
    views::Label* const body_label =
        top_text_container->AddChildView(std::make_unique<views::Label>(
            params.body_text, delegate->GetBodyTextContext()));
    labels_.push_back(body_label);
    body_label->SetProperty(views::kElementIdentifierKey,
                            kBodyTextIdForTesting);
  }

  // Set common label properties.
  for (views::Label* label : labels_) {
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetMultiLine(true);
    label->SetElideBehavior(gfx::NO_ELIDE);
  }

  // Add close button.
  std::u16string alt_text = params.close_button_alt_text;

  // This can be empty if a test doesn't set it. Set a reasonable default to
  // avoid an assertion (generated when a button with no text has no
  // accessible name).
  if (alt_text.empty()) {
    alt_text = l10n_util::GetStringUTF16(IDS_CLOSE);
  }

  // Since we set the cancel callback, we will use CancelDialog() to dismiss.
  close_button_ = (params.progress ? progress_container : top_text_container)
                      ->AddChildView(std::make_unique<ClosePromoButton>(
                          delegate, alt_text,
                          base::BindRepeating(&DialogDelegate::CancelDialog,
                                              base::Unretained(this))));
  close_button_->SetProperty(views::kElementIdentifierKey,
                             kCloseButtonIdForTesting);

  // Add other buttons.
  if (!params.buttons.empty()) {
    auto run_callback_and_close = [](HelpBubbleView* bubble_view,
                                     base::OnceClosure callback) {
      // We want to call the button callback before deleting the bubble in case
      // the caller needs to do something with it, but the callback itself
      // could close the bubble. Therefore, we need to ensure that the
      // underlying bubble view is not deleted before trying to close it.
      views::ViewTracker tracker(bubble_view);
      std::move(callback).Run();
      auto* const view = tracker.view();
      if (view && view->GetWidget() && !view->GetWidget()->IsClosed())
        view->GetWidget()->Close();
    };

    // We will hold the default button to add later, since where we add it in
    // the sequence depends on platform style.
    std::unique_ptr<MdIPHBubbleButton> default_button;
    for (HelpBubbleButtonParams& button_params : params.buttons) {
      auto button = std::make_unique<MdIPHBubbleButton>(
          delegate,
          base::BindOnce(run_callback_and_close, base::Unretained(this),
                         std::move(button_params.callback)),
          button_params.text, button_params.is_default);
      button->SetMinSize(gfx::Size(0, 0));
      if (button_params.is_default) {
        DCHECK(!default_button);
        default_button = std::move(button);
        default_button->SetProperty(views::kElementIdentifierKey,
                                    kDefaultButtonIdForTesting);
      } else {
        non_default_buttons_.push_back(
            button_container->AddChildView(std::move(button)));
      }
    }

    if (!non_default_buttons_.empty()) {
      non_default_buttons_.front()->SetProperty(
          views::kElementIdentifierKey, kFirstNonDefaultButtonIdForTesting);
    }

    // Add the default button if there is one based on platform style.
    if (default_button) {
      if (views::PlatformStyle::kIsOkButtonLeading) {
        default_button_ =
            button_container->AddChildViewAt(std::move(default_button), 0);
      } else {
        default_button_ =
            button_container->AddChildView(std::move(default_button));
      }
    }
  } else {
    button_container->SetVisible(false);
  }

  // Set up layouts. This is the default vertical spacing that is also used to
  // separate progress indicators for symmetry.
  // TODO(dfried): consider whether we could take font ascender and descender
  // height and factor them into margin calculations.
  const views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  const int default_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  // The insets from the bubble border to the text inside.
  const gfx::Insets contents_insets = gfx::Insets(20);

  // Create primary layout (vertical).
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(contents_insets)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, default_spacing, 0))
      .SetIgnoreDefaultMainAxisMargins(true);

  // Set up top row container layout.
  const int kCloseButtonHeight = 20;
  auto& progress_layout =
      progress_container
          ->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetMinimumCrossAxisSize(kCloseButtonHeight)
          .SetDefault(views::kMarginsKey,
                      gfx::Insets::TLBR(0, default_spacing, 0, 0))
          .SetIgnoreDefaultMainAxisMargins(true);
  progress_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(progress_layout.GetDefaultFlexRule()));

  // Close button should float right in whatever container it's in.
  if (close_button_) {
    close_button_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded)
            .WithAlignment(views::LayoutAlignment::kEnd));
    close_button_->SetProperty(views::kMarginsKey,
                               gfx::Insets::TLBR(0, default_spacing, 0, 0));
    close_button_->SetProperty(views::kElementIdentifierKey,
                               kCloseButtonIdForTesting);
  }

  // Icon view should have padding between it and the title or body label.
  if (icon_view_) {
    // When there is no title, distance from icon and body text to buttons can
    // appear cramped, so add a small bit of extra margin.
    const int bottom_margin =
        !params.buttons.empty() && params.title_text.empty() ? 2 : 0;
    icon_view_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, bottom_margin, default_spacing));
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

  for (views::Label* label : labels_)
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
  if (icon_view_) {
    const int indent =
        contents_insets.left() + kBodyIconBackgroundSize + default_spacing;
    for (size_t i = 1; i < labels_.size(); ++i) {
      labels_[i]->SetProperty(views::kMarginsKey,
                              gfx::Insets::TLBR(0, indent, 0, 0));
    }
  }

  // Set up button container layout.

  // Add in spacing between bubble content and bottom/buttons.
  button_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(layout_provider->GetDistanceMetric(
                            views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT),
                        0, 0, 0));

  // Create button container internal layout.
  auto& button_layout =
      button_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
          .SetDefault(
              views::kMarginsKey,
              gfx::Insets::TLBR(0,
                                layout_provider->GetDistanceMetric(
                                    views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                                0, 0))
          .SetIgnoreDefaultMainAxisMargins(true);

  // In a handful of (mostly South-Asian) languages, button text can exceed the
  // available width in the bubble if buttons are aligned horizontally. In those
  // cases - and only those cases - the bubble can switch to a vertical button
  // alignment.
  if (button_container->GetMinimumSize().width() >
      kMaxWidthDip - contents_insets.width()) {
    button_layout.SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kEnd)
        .SetDefault(views::kMarginsKey, gfx::Insets::VH(default_spacing, 0))
        .SetIgnoreDefaultMainAxisMargins(true);

    // Calculate the closest the bubble can be to the normal max width without
    // cutting off an especially long button caption.
    max_bubble_width_ =
        std::max(kMaxWidthDip, button_container->GetMinimumSize().width() +
                                   contents_insets.width());
  }

  button_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(button_layout.GetDefaultFlexRule()));

  // Want a consistent initial focused view if one is available.
  if (!button_container->children().empty()) {
    SetInitiallyFocusedView(button_container->children()[0]);
  } else if (close_button_) {
    SetInitiallyFocusedView(close_button_);
  }

  SetProperty(views::kElementIdentifierKey, kHelpBubbleElementIdForTesting);
  set_margins(gfx::Insets());
  set_title_margins(gfx::Insets());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_close_on_deactivate(false);
  set_focus_traversable_from_anchor_view(false);

  const bool suppress_events =
      event_relay_ && !event_relay_->ShouldHelpBubbleProcessEvents();
  if (suppress_events) {
    CHECK_LE(params.buttons.size(), 1U)
        << "Help bubbles that cannot activate cannot have multiple interactive "
           "buttons due to accessibility constraints.";
    SetCanActivate(false);
    set_accept_events(false);
  }

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  // This gets reset to the platform default when we call CreateBubble(), so we
  // have to change it afterwards:
  set_adjust_if_offscreen(true);
  auto* const frame_view = GetBubbleFrameView();
  frame_view->SetDisplayVisibleArrow(anchor.show_arrow &&
                                     params.arrow != HelpBubbleArrow::kNone);

  // If the primary window widget is not the anchor widget, do not use the
  // window anchor bounds.
  if (anchor_widget()->GetPrimaryWindowWidget() != anchor_widget()) {
    frame_view->set_use_anchor_window_bounds(false);
  }

  // Bubbles get a 1-dip border that's either light or dark depending on system
  // light or dark mode, but this does not match with the help bubble (see
  // b/303069420).
  frame_view->bubble_border()->set_draw_border_stroke(false);

  // TODO(crbug.com/41493925) Remove this InvalidateLayout() once the border
  // invalidate itself when it changes.
  InvalidateLayout();

  // Setup that should happen after the widget is constructed:
  if (suppress_events) {
    // This is required on Windows because of the way events are routed.
    GetBubbleFrameView()->set_hit_test_transparent(true);
  }
  if (auto* const anchor_bubble =
          anchor_widget()->widget_delegate()->AsBubbleDialogDelegate()) {
    // Make sure that if the help bubble is attaching to a dialog, the dialog
    // does not immediately dismiss when the help bubble is shown or focused.
    anchor_pin_ = anchor_bubble->PreventCloseOnDeactivate();
  }

  // Most help bubbles with buttons take focus when they show.
  const bool show_active =
      params.focus_on_show_hint.value_or(!params.buttons.empty()) &&
      !event_relay_;
  if (show_active) {
    widget->Show();
  } else {
    widget->ShowInactive();
  }

  // Begin event-forwarding if appropriate.
  if (event_relay_) {
    event_relay_->Init(this);
  }

  MaybeStartAutoCloseTimer();
}

HelpBubbleView::~HelpBubbleView() = default;

void HelpBubbleView::MaybeStartAutoCloseTimer() {
  if (timeout_.is_zero())
    return;

  auto_close_timer_.Start(FROM_HERE, timeout_, this,
                          &HelpBubbleView::OnTimeout);
}

void HelpBubbleView::OnTimeout() {
  if (timeout_callback_) {
    std::move(timeout_callback_).Run();
  }
  GetWidget()->Close();
}

std::u16string HelpBubbleView::GetAccessibleWindowTitle() const {
  std::u16string result = accessible_name_;

  // If there's a keyboard navigation hint, append it after a full stop.
  if (!screenreader_hint_text_.empty() && activate_count_ <= 1)
    result += u". " + screenreader_hint_text_;

  return result;
}

void HelpBubbleView::OnWidgetActivationChanged(views::Widget* widget,
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

void HelpBubbleView::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();

  const auto* color_provider = GetColorProvider();
  const SkColor background_color =
      color_provider->GetColor(delegate_->GetHelpBubbleBackgroundColorId());
  set_color(background_color);

  const SkColor foreground_color =
      color_provider->GetColor(delegate_->GetHelpBubbleForegroundColorId());
  if (icon_view_) {
    icon_view_->SetBackground(views::CreateRoundedRectBackground(
        foreground_color, icon_view_->GetPreferredSize({}).height() / 2));
  }

  for (views::Label* label : labels_) {
    label->SetBackgroundColor(background_color);
    label->SetEnabledColor(foreground_color);
  }
}

gfx::Size HelpBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const gfx::Size layout_manager_preferred_size =
      View::CalculatePreferredSize(available_size);

  // Wrap if the width is larger than |kBubbleMaxWidthDip|.
  if (layout_manager_preferred_size.width() > max_bubble_width_) {
    return gfx::Size(max_bubble_width_,
                     GetLayoutManager()->GetPreferredHeightForWidth(
                         this, max_bubble_width_));
  }

  if (layout_manager_preferred_size.width() < kMinWidthDip) {
    return gfx::Size(kMinWidthDip, layout_manager_preferred_size.height());
  }

  return layout_manager_preferred_size;
}

gfx::Rect HelpBubbleView::GetAnchorRect() const {
  gfx::Rect default_anchor_rect = BubbleDialogDelegateView::GetAnchorRect();
  if (!local_anchor_bounds_) {
    return default_anchor_rect;
  }

  // Ensure that we are not trying to clamp the anchor bounds to a completely
  // empty bounds.
  gfx::Size size = default_anchor_rect.size();
  size.SetToMax({1, 1});

  // Clamp the local bounds to the size of the anchor view.
  const int left = std::clamp(local_anchor_bounds_->x(), 0, size.width() - 1);
  const int right = std::clamp(local_anchor_bounds_->right(), 1, size.width());
  const int top = std::clamp(local_anchor_bounds_->y(), 0, size.height() - 1);
  const int bottom =
      std::clamp(local_anchor_bounds_->bottom(), 1, size.height());
  gfx::Rect result(left, top, right - left, bottom - top);

  // Translate back to screen coordinates.
  result.Offset(default_anchor_rect.OffsetFromOrigin());
  return result;
}

void HelpBubbleView::OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                              views::Widget* widget) const {
  BubbleDialogDelegateView::OnBeforeBubbleWidgetInit(params, widget);
#if BUILDFLAG(IS_LINUX)
  // Help bubbles anchored to menus may be clipped to their anchors' bounds,
  // resulting in visual errors, unless they use accelerated rendering. See
  // crbug.com/1445770 for details. This also applies to bubbles anchored to
  // all accelerated windows below a certain size, especially those which are
  // not top-level application windows (see crbug.com/340523110).
  //
  // Because it is not possible to know exactly if a bubble will correctly fit
  // in the bounds of its ancestor accelerator widget, due to things like
  // anchor positioning and the possibility that a window size could change,
  // make all Linux help bubbles accelerated.
  //
  // Note: accelerated widgets are "desktop native" widgets and interact with
  // the OS window activation system; this is, in turn, a problem for Linux
  // because of known technical limitations around window activation. See the
  // following bug for more information regarding window activation issues in
  // Weston, the windowing environment used on chrome's Wayland test-bots:
  // https://gitlab.freedesktop.org/wayland/weston/-/issues/669
  params->use_accelerated_widget_override = true;
#endif
}

// static
bool HelpBubbleView::IsHelpBubble(views::DialogDelegate* dialog) {
  auto* const contents = dialog->GetContentsView();
  return contents && views::IsViewClass<HelpBubbleView>(contents);
}

bool HelpBubbleView::IsFocusInHelpBubble() const {
#if BUILDFLAG(IS_MAC)
  if (close_button_ && close_button_->HasFocus())
    return true;
  if (default_button_ && default_button_->HasFocus())
    return true;
  for (views::MdTextButton* button : non_default_buttons_) {
    if (button->HasFocus())
      return true;
  }
  return false;
#else
  return GetWidget()->IsActive();
#endif
}

views::LabelButton* HelpBubbleView::GetDefaultButtonForTesting() const {
  return default_button_;
}

views::LabelButton* HelpBubbleView::GetNonDefaultButtonForTesting(
    int index) const {
  return non_default_buttons_[index];
}

void HelpBubbleView::SetForceAnchorRect(gfx::Rect force_anchor_rect) {
  force_anchor_rect.Offset(
      -views::BubbleDialogDelegateView::GetAnchorRect().OffsetFromOrigin());
  local_anchor_bounds_ = force_anchor_rect;
}

BEGIN_METADATA(HelpBubbleView)
END_METADATA

}  // namespace user_education
