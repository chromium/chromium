// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/accessibility/caption_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace {

// Formatting constants
static constexpr int kLineHeightDip = 24;
static constexpr int kNumLinesCollapsed = 2;
static constexpr int kNumLinesExpanded = 8;
static constexpr int kCornerRadiusDip = 4;
static constexpr int kSidePaddingDip = 18;
static constexpr int kButtonDip = 16;
static constexpr int kButtonCircleHighlightPaddingDip = 2;
// The preferred width of the bubble within its anchor.
static constexpr double kPreferredAnchorWidthPercentage = 0.8;
static constexpr int kMaxWidthDip = 536;
// Margin of the bubble with respect to the anchor window.
static constexpr int kMinAnchorMarginDip = 20;
static constexpr uint16_t kCaptionBubbleAlpha = 230;  // 90% opacity
static constexpr char kPrimaryFont[] = "Roboto";
static constexpr char kSecondaryFont[] = "Arial";
static constexpr char kTertiaryFont[] = "sans-serif";
static constexpr int kFontSizePx = 16;
static constexpr double kDefaultRatioInParentX = 0.5;
static constexpr double kDefaultRatioInParentY = 1;
static constexpr int kErrorImageSizeDip = 20;
static constexpr int kErrorMessageBetweenChildSpacingDip = 16;
static constexpr int kFocusRingInnerInsetDip = 3;
static constexpr int kWidgetDisplacementWithArrowKeyDip = 16;
static constexpr int kNoActivityIntervalSeconds = 5;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. These should be the same as
// LiveCaptionSessionEvent in enums.xml.
enum class SessionEvent {
  // We began showing captions for an audio stream.
  kStreamStarted = 0,
  // The audio stream ended and the caption bubble closes.
  kStreamEnded = 1,
  // The close button was clicked, so we stopped listening to an audio stream.
  kCloseButtonClicked = 2,
  kMaxValue = kCloseButtonClicked,
};

void LogSessionEvent(SessionEvent event) {
  base::UmaHistogramEnumeration("Accessibility.LiveCaption.Session", event);
}

std::unique_ptr<views::ImageButton> BuildImageButton(
    views::Button::PressedCallback callback,
    const int tooltip_text_id) {
  auto button = views::CreateVectorImageButton(std::move(callback));
  button->SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
  button->SizeToPreferredSize();
  views::InstallCircleHighlightPathGenerator(
      button.get(), gfx::Insets(kButtonCircleHighlightPaddingDip));
  return button;
}

// ui::CaptionStyle styles are CSS strings that can sometimes have !important.
// This method removes " !important" if it exists at the end of a CSS string.
std::string MaybeRemoveCSSImportant(std::string css_string) {
  RE2::Replace(&css_string, "\\s+!important", "");
  return css_string;
}

// Parses a CSS color string that is in the form rgba and has a non-zero alpha
// value into an SkColor and sets it to be the value of the passed-in SkColor
// address. Returns whether or not the operation was a success.
//
// `css_string` is the CSS color string passed in. It is in the form
//     rgba(#,#,#,#). r, g, and b are integers between 0 and 255, inclusive.
//     a is a double between 0.0 and 1.0. There may be whitespace in between the
//     commas.
// `sk_color` is the address of an SKColor. If the operation is a success, the
//     function will set sk_color's value to be the parsed SkColor. If the
//     operation is not a success, sk_color's value will not change.
//
// As of spring 2021, all OS's use rgba to define the caption style colors.
// However, the ui::CaptionStyle spec also allows for the use of any valid CSS
// color spec. This function will need to be revisited should ui::CaptionStyle
// colors use non-rgba to define their colors.
bool ParseNonTransparentRGBACSSColorString(std::string css_string,
                                           SkColor* sk_color) {
  std::string rgba = MaybeRemoveCSSImportant(css_string);
  if (rgba.empty())
    return false;
  uint16_t r, g, b;
  double a;
  bool match = RE2::FullMatch(
      rgba, "rgba\\((\\d+),\\s*(\\d+),\\s*(\\d+),\\s*(\\d+\\.?\\d*)\\)", &r, &g,
      &b, &a);
  // If the opacity is set to 0 (fully transparent), we ignore the user's
  // preferred style and use our default color.
  if (!match || a == 0)
    return false;
  uint16_t a_int = uint16_t{a * 255};
#if defined(OS_MAC)
  // On Mac, any opacity lower than 90% leaves rendering artifacts which make
  // it appear like there is a layer of faint text beneath the actual text.
  // TODO(crbug.com/1199419): Fix the rendering issue and then remove this
  // workaround.
  a_int = std::max(kCaptionBubbleAlpha, a_int);
#endif
  *sk_color = SkColorSetARGB(a_int, r, g, b);
  return match;
}

}  // namespace

namespace captions {
// CaptionBubble implementation of BubbleFrameView. This class takes care
// of making the caption draggable and handling the focus ring when the
// Caption Bubble is focused.
class CaptionBubbleFrameView : public views::BubbleFrameView {
 public:
  METADATA_HEADER(CaptionBubbleFrameView);
  explicit CaptionBubbleFrameView(views::View* close_button,
                                  views::View* expand_button,
                                  views::View* collapse_button)
      : views::BubbleFrameView(gfx::Insets(), gfx::Insets()),
        close_button_(close_button),
        expand_button_(expand_button),
        collapse_button_(collapse_button) {
    // The focus ring is drawn on CaptionBubbleFrameView because it has the
    // correct bounds, but focused state is taken from the CaptionBubble.
    focus_ring_ = views::FocusRing::Install(this);

    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::FLOAT, views::BubbleBorder::DIALOG_SHADOW,
        gfx::kPlaceholderColor);
    border->SetCornerRadius(kCornerRadiusDip);
#if defined(OS_MAC)
    // Inset the border so that there's space to draw a focus ring on Mac
    // without clipping by the system window.
    border->set_insets(border->GetBorderAndShadowInsets() + gfx::Insets(1));
#endif
    gfx::Insets shadow = border->GetBorderAndShadowInsets();
    gfx::Insets padding = gfx::Insets(kFocusRingInnerInsetDip);
    focus_ring_->SetPathGenerator(
        std::make_unique<views::RoundRectHighlightPathGenerator>(
            shadow - padding, kCornerRadiusDip + 2));
    views::BubbleFrameView::SetBubbleBorder(std::move(border));
    focus_ring_->SetHasFocusPredicate([](View* view) {
      auto* frame_view = static_cast<CaptionBubbleFrameView*>(view);
      return frame_view->contents_focused();
    });
  }

  ~CaptionBubbleFrameView() override = default;
  CaptionBubbleFrameView(const CaptionBubbleFrameView&) = delete;
  CaptionBubbleFrameView& operator=(const CaptionBubbleFrameView&) = delete;

  void UpdateFocusRing(bool focused) {
    contents_focused_ = focused;
    focus_ring_->SchedulePaint();
  }

  bool contents_focused() { return contents_focused_; }

  // TODO(crbug.com/1055150): This does not work on Linux because the bubble is
  // not a top-level view, so it doesn't receive events. See crbug.com/1074054
  // for more about why it doesn't work.
  int NonClientHitTest(const gfx::Point& point) override {
    // Outside of the window bounds, do nothing.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    // |point| is in coordinates relative to CaptionBubbleFrameView, i.e.
    // (0,0) is the upper left corner of this view. Convert it to screen
    // coordinates to see whether one of the buttons contains this point.
    // If it is, return HTCLIENT, so that the click is sent through to be
    // handled by CaptionBubble::BubblePressed().
    gfx::Point point_in_screen =
        GetBoundsInScreen().origin() + gfx::Vector2d(point.x(), point.y());
    if (close_button_->GetBoundsInScreen().Contains(point_in_screen) ||
        expand_button_->GetBoundsInScreen().Contains(point_in_screen) ||
        collapse_button_->GetBoundsInScreen().Contains(point_in_screen))
      return HTCLIENT;

    // Ensure it's within the BubbleFrameView. This takes into account the
    // rounded corners and drop shadow of the BubbleBorder.
    int hit = views::BubbleFrameView::NonClientHitTest(point);

    // After BubbleFrameView::NonClientHitTest processes the bubble-specific
    // hits such as the rounded corners, it checks hits to the bubble's client
    // view. Any hits to ClientFrameView::NonClientHitTest return HTCLIENT or
    // HTNOWHERE. Override these to return HTCAPTION in order to make the
    // entire widget draggable.
    return (hit == HTCLIENT || hit == HTNOWHERE) ? HTCAPTION : hit;
  }

  void Layout() override {
    views::BubbleFrameView::Layout();
    focus_ring_->Layout();
  }

 private:
  views::View* close_button_;
  views::View* expand_button_;
  views::View* collapse_button_;
  views::FocusRing* focus_ring_ = nullptr;
  bool contents_focused_ = false;
};

BEGIN_METADATA(CaptionBubbleFrameView, views::BubbleFrameView)
END_METADATA

// CaptionBubble implementation of Label. This class takes care of setting up
// the accessible virtual views of the label in order to support braille
// accessibility. The CaptionBubbleLabel is a readonly document with a paragraph
// inside. Inside the paragraph are staticText nodes, one for each visual line
// in the rendered text of the label. These staticText nodes are shown on a
// braille display so that a braille user can read the caption text line by
// line.
class CaptionBubbleLabel : public views::Label {
 public:
  METADATA_HEADER(CaptionBubbleLabel);
  CaptionBubbleLabel() {
    // TODO(crbug.com/1191091): Override GetAccessibleNodeData and set the role
    // of the CaptionBubbleLabel to be kDocument, rather than adding a kDocument
    // as a virtual view. This is a temporary fix to ensure that the kDocument
    // node appears in the accessibility tree.
    // Views are not supposed to be documents (see
    // `ViewAccessibility::IsValidRoleForViews` for more information) but we
    // make an exception here. The CaptionBubbleLabel is designed to be
    // interacted with by a braille display in virtual buffer mode. In order to
    // activate the virtual buffer in NVDA, we set the top-level virtual view in
    // CaptionBubbleLabel to be a readonly document.
    auto ax_document = std::make_unique<views::AXVirtualView>();
    ax_document->GetCustomData().role = ax::mojom::Role::kDocument;
    ax_document->GetCustomData().SetRestriction(
        ax::mojom::Restriction::kReadOnly);
    GetViewAccessibility().AddVirtualChildView(std::move(ax_document));
  }

  ~CaptionBubbleLabel() override = default;
  CaptionBubbleLabel(const CaptionBubbleLabel&) = delete;
  CaptionBubbleLabel& operator=(const CaptionBubbleLabel&) = delete;

  void SetText(const std::u16string& text) override {
    views::Label::SetText(text);

    auto& ax_document = GetViewAccessibility().virtual_children()[0];
    auto& ax_lines = ax_document->children();
    if (text.empty() && !ax_lines.empty()) {
      ax_document->RemoveAllChildViews();
      return;
    }

    const size_t num_lines = GetRequiredLines();
    size_t start = 0;
    for (size_t i = 0; i < num_lines - 1; ++i) {
      size_t end = GetTextIndexOfLine(i + 1);
      std::u16string substring = text.substr(start, end - start);
      UpdateAXLine(substring, i, gfx::Range(start, end));
      start = end;
    }
    std::u16string substring = text.substr(start, text.size() - start);
    if (!substring.empty()) {
      UpdateAXLine(substring, num_lines - 1, gfx::Range(start, text.size()));
    }

    // Remove all ax_lines that don't have a corresponding line.
    size_t num_ax_lines = ax_lines.size();
    for (size_t i = num_lines; i < num_ax_lines; ++i) {
      ax_document->RemoveChildView(ax_lines.back().get());
    }

    NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
  }

 private:
  void UpdateAXLine(const std::u16string& line_text,
                    const size_t line_index,
                    const gfx::Range& text_range) {
    auto& ax_document = GetViewAccessibility().virtual_children()[0];
    auto& ax_lines = ax_document->children();

    // Add a new virtual child for a new line of text.
    DCHECK(line_index <= ax_lines.size());
    if (line_index == ax_lines.size()) {
      auto ax_line = std::make_unique<views::AXVirtualView>();
      ax_line->GetCustomData().role = ax::mojom::Role::kStaticText;
      ax_document->AddChildView(std::move(ax_line));
    }

    // Set the virtual child's name as line text.
    ui::AXNodeData& ax_node_data = ax_lines[line_index]->GetCustomData();
    if (base::UTF8ToUTF16(ax_node_data.GetStringAttribute(
            ax::mojom::StringAttribute::kName)) != line_text) {
      ax_node_data.SetName(line_text);
      std::vector<gfx::Rect> bounds = GetSubstringBounds(text_range);
      ax_node_data.relative_bounds.bounds = gfx::RectF(bounds[0]);
    }
  }
};

BEGIN_METADATA(CaptionBubbleLabel, views::Label)
END_METADATA

CaptionBubble::CaptionBubble(views::View* anchor,
                             BrowserView* browser_view,
                             base::OnceClosure destroyed_callback)
    : BubbleDialogDelegateView(anchor,
                               views::BubbleBorder::FLOAT,
                               views::BubbleBorder::Shadow::NO_SHADOW),
      destroyed_callback_(std::move(destroyed_callback)),
      ratio_in_parent_x_(kDefaultRatioInParentX),
      ratio_in_parent_y_(kDefaultRatioInParentY),
      browser_view_(browser_view),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  // Bubbles that use transparent colors should not paint their ClientViews to a
  // layer as doing so could result in visual artifacts.
  SetPaintClientToLayer(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_draggable(true);
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_F6, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_F6, ui::EF_SHIFT_DOWN));
  // The CaptionBubble is focusable. It will alert the CaptionBubbleFrameView
  // when its focus changes so that the focus ring can be updated.
  // TODO(crbug.com/1055150): Consider using
  // View::FocusBehavior::ACCESSIBLE_ONLY. However, that does not seem to get
  // OnFocus() and OnBlur() called so we never draw the custom focus ring.
  SetFocusBehavior(View::FocusBehavior::ALWAYS);
  inactivity_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, base::TimeDelta::FromSeconds(kNoActivityIntervalSeconds),
      base::BindRepeating(&CaptionBubble::OnInactivityTimeout,
                          base::Unretained(this)),
      tick_clock_);
  inactivity_timer_->Stop();
}

CaptionBubble::~CaptionBubble() {
  if (model_)
    model_->RemoveObserver();
}

gfx::Rect CaptionBubble::GetBubbleBounds() {
  // Get the height and width of the full bubble using the superclass method.
  // This includes shadow and insets.
  gfx::Rect original_bounds =
      views::BubbleDialogDelegateView::GetBubbleBounds();

  gfx::Rect anchor_rect = GetAnchorView()->GetBoundsInScreen();
  // Calculate the desired width based on the original bubble's width (which is
  // the max allowed per the spec).
  int min_width = anchor_rect.width() - kMinAnchorMarginDip * 2;
  int desired_width = anchor_rect.width() * kPreferredAnchorWidthPercentage;
  int width = std::max(min_width, desired_width);
  if (width > original_bounds.width())
    width = original_bounds.width();
  int height = original_bounds.height();

  // The placement is based on the ratio between the center of the widget and
  // the center of the anchor_rect.
  int target_x =
      anchor_rect.x() + anchor_rect.width() * ratio_in_parent_x_ - width / 2.0;
  int target_y = anchor_rect.y() + anchor_rect.height() * ratio_in_parent_y_ -
                 height / 2.0;
  latest_bounds_ = gfx::Rect(target_x, target_y, width, height);
  latest_anchor_bounds_ = GetAnchorView()->GetBoundsInScreen();
  anchor_rect.Inset(gfx::Insets(kMinAnchorMarginDip));
  if (!anchor_rect.Contains(latest_bounds_)) {
    latest_bounds_.AdjustToFit(anchor_rect);
  }
  // If it still doesn't fit after being adjusted to fit, then it is too tall
  // or too wide for the tiny window, and we need to simply hide it. Otherwise,
  // ensure it is shown.
  DCHECK(GetWidget());
  bool can_layout = latest_bounds_.height() >= height;
  if (can_layout != can_layout_) {
    can_layout_ = can_layout;
    UpdateBubbleVisibility();
  }

  return latest_bounds_;
}

void CaptionBubble::OnWidgetBoundsChanged(views::Widget* widget,
                                          const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget, GetWidget());
  gfx::Rect widget_bounds = GetWidget()->GetWindowBoundsInScreen();
  gfx::Rect anchor_rect = GetAnchorView()->GetBoundsInScreen();
  if (latest_bounds_ == widget_bounds && latest_anchor_bounds_ == anchor_rect) {
    return;
  }

  if (latest_anchor_bounds_ != anchor_rect) {
    // The window has moved. Reposition the widget within it.
    SizeToContents();
    return;
  }

  // Check that our widget is visible. If it is not visible then
  // the user has not explicitly moved it (because the user can't see it),
  // so we should take no action.
  if (!GetWidget()->IsVisible())
    return;

  // The widget has moved within the window. Recalculate the desired ratio
  // within the parent.
  gfx::Rect bounds_rect = GetAnchorView()->GetBoundsInScreen();
  bounds_rect.Inset(gfx::Insets(kMinAnchorMarginDip));

  bool out_of_bounds = false;
  if (!bounds_rect.Contains(widget_bounds)) {
    widget_bounds.AdjustToFit(bounds_rect);
    out_of_bounds = true;
  }

  ratio_in_parent_x_ = (widget_bounds.CenterPoint().x() - anchor_rect.x()) /
                       (1.0 * anchor_rect.width());
  ratio_in_parent_y_ = (widget_bounds.CenterPoint().y() - anchor_rect.y()) /
                       (1.0 * anchor_rect.height());

  if (out_of_bounds)
    SizeToContents();

  // If the widget is visible and unfocused, probably due to a mouse drag, reset
  // the inactivity timer.
  if (GetWidget()->IsVisible() && !HasFocus())
    inactivity_timer_->Reset();
}

void CaptionBubble::Init() {
  views::View* content_container = new views::View();
  content_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetInteriorMargin(gfx::Insets(0, kSidePaddingDip))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width*/ true));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);
  UseCompactMargins();

  set_close_on_deactivate(false);
  // The caption bubble starts out hidden and unable to be activated.
  SetCanActivate(false);

  auto label = std::make_unique<CaptionBubbleLabel>();
  label->SetMultiLine(true);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_TOP);
  label->SetTooltipText(std::u16string());
  // Render text truncates the end of text that is greater than 10000 chars.
  // While it is unlikely that the text will exceed 10000 chars, it is not
  // impossible, if the speech service sends a very long transcription_result.
  // In order to guarantee that the caption bubble displays the last lines, and
  // in order to ensure that caption_bubble_->GetTextIndexOfLine() is correct,
  // set the truncate_length to 0 to ensure that it never truncates.
  label->SetTruncateLength(0);

  auto title = std::make_unique<views::Label>();
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title->SetText(l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_TITLE));
  title->GetViewAccessibility().OverrideIsIgnored(true);

  auto error_text = std::make_unique<views::Label>();
  error_text->SetBackgroundColor(SK_ColorTRANSPARENT);
  error_text->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  error_text->SetText(l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_ERROR));

  auto error_icon = std::make_unique<views::ImageView>();

  auto error_message = std::make_unique<views::View>();
  error_message
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kErrorMessageBetweenChildSpacingDip))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  error_message->SetVisible(false);

  views::Button::PressedCallback expand_or_collapse_callback =
      base::BindRepeating(&CaptionBubble::ExpandOrCollapseButtonPressed,
                          base::Unretained(this));
  auto expand_button = BuildImageButton(expand_or_collapse_callback,
                                        IDS_LIVE_CAPTION_BUBBLE_EXPAND);
  expand_button->SetVisible(!is_expanded_);

  auto collapse_button = BuildImageButton(
      std::move(expand_or_collapse_callback), IDS_LIVE_CAPTION_BUBBLE_COLLAPSE);
  collapse_button->SetVisible(is_expanded_);

  auto close_button =
      BuildImageButton(base::BindRepeating(&CaptionBubble::CloseButtonPressed,
                                           base::Unretained(this)),
                       IDS_LIVE_CAPTION_BUBBLE_CLOSE);

  title_ = content_container->AddChildView(std::move(title));
  label_ = content_container->AddChildView(std::move(label));

  error_icon_ = error_message->AddChildView(std::move(error_icon));
  error_text_ = error_message->AddChildView(std::move(error_text));
  error_message_ = content_container->AddChildView(std::move(error_message));

  expand_button_ = content_container->AddChildView(std::move(expand_button));
  collapse_button_ =
      content_container->AddChildView(std::move(collapse_button));

  close_button_ = AddChildView(std::move(close_button));
  content_container_ = AddChildView(std::move(content_container));

  SetCaptionBubbleStyle();
  UpdateContentSize();
}

bool CaptionBubble::ShouldShowCloseButton() const {
  // We draw our own close button so that we can capture the button presses and
  // so we can customize its appearance.
  return false;
}

std::unique_ptr<views::NonClientFrameView>
CaptionBubble::CreateNonClientFrameView(views::Widget* widget) {
  auto frame = std::make_unique<CaptionBubbleFrameView>(
      close_button_, expand_button_, collapse_button_);
  frame_ = frame.get();
  return frame;
}

void CaptionBubble::OnKeyEvent(ui::KeyEvent* event) {
  // Use the arrow keys to move.
  if (event->type() == ui::ET_KEY_PRESSED) {
    gfx::Vector2d offset;
    if (event->key_code() == ui::VKEY_UP)
      offset.set_y(-kWidgetDisplacementWithArrowKeyDip);
    if (event->key_code() == ui::VKEY_DOWN)
      offset.set_y(kWidgetDisplacementWithArrowKeyDip);
    if (event->key_code() == ui::VKEY_LEFT)
      offset.set_x(-kWidgetDisplacementWithArrowKeyDip);
    if (event->key_code() == ui::VKEY_RIGHT)
      offset.set_x(kWidgetDisplacementWithArrowKeyDip);
    if (offset != gfx::Vector2d()) {
      DCHECK(GetWidget());
      gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
      bounds.Offset(offset);
      GetWidget()->SetBounds(bounds);
      int x = 100 * base::ClampToRange(ratio_in_parent_x_, 0.0, 1.0);
      int y = 100 * base::ClampToRange(ratio_in_parent_y_, 0.0, 1.0);
      GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
          IDS_LIVE_CAPTION_BUBBLE_MOVE_SCREENREADER_ANNOUNCEMENT,
          base::NumberToString16(x), base::NumberToString16(y)));
      return;
    }
  }
  views::BubbleDialogDelegateView::OnKeyEvent(event);
}

bool CaptionBubble::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    // We don't want to close when the user hits "escape", because this isn't a
    // normal dialog bubble -- it's meant to be up all the time. We just want to
    // release focus back to the page in that case.
    // Users should use the "close" button to close the bubble.
    GetAnchorView()->RequestFocus();
    GetAnchorView()->GetWidget()->Activate();
    return true;
  }
  if (accelerator.key_code() == ui::VKEY_F6) {
    // F6 rotates focus through the panes in the browser. Use
    // BrowserView::AcceleratorPressed so that metrics are logged appropriately.
    browser_view_->AcceleratorPressed(accelerator);
    // Remove focus from this widget.
    browser_view_->GetWidget()->Activate();
    return true;
  }
  NOTREACHED();
  return false;
}

void CaptionBubble::OnFocus() {
  frame_->UpdateFocusRing(true);
  inactivity_timer_->Stop();
}

void CaptionBubble::OnBlur() {
  frame_->UpdateFocusRing(false);
  inactivity_timer_->Reset();
}

void CaptionBubble::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(title_->GetText());
}

std::u16string CaptionBubble::GetAccessibleWindowTitle() const {
  return title_->GetText();
}

void CaptionBubble::AddedToWidget() {
  DCHECK(GetWidget());
  DCHECK(GetAnchorView());
  DCHECK(anchor_widget());
  GetWidget()->SetFocusTraversableParent(
      anchor_widget()->GetFocusTraversable());
  GetWidget()->SetFocusTraversableParentView(GetAnchorView());
  GetAnchorView()->SetProperty(views::kAnchoredDialogKey,
                               static_cast<DialogDelegate*>(this));
}

void CaptionBubble::CloseButtonPressed() {
  LogSessionEvent(SessionEvent::kCloseButtonClicked);
  if (model_)
    model_->Close();
}

void CaptionBubble::ExpandOrCollapseButtonPressed() {
  is_expanded_ = !is_expanded_;
  base::UmaHistogramBoolean("Accessibility.LiveCaption.ExpandBubble",
                            is_expanded_);
  views::Button *old_button = collapse_button_, *new_button = expand_button_;
  if (is_expanded_)
    std::swap(old_button, new_button);
  bool button_had_focus = old_button->HasFocus();
  OnIsExpandedChanged();
  // TODO(crbug.com/1055150): Ensure that the button keeps focus on mac.
  if (button_had_focus)
    new_button->RequestFocus();
  inactivity_timer_->Reset();
}

void CaptionBubble::SetModel(CaptionBubbleModel* model) {
  if (model_)
    model_->RemoveObserver();
  model_ = model;
  if (model_) {
    model_->SetObserver(this);
  } else {
    UpdateBubbleVisibility();
  }
}

void CaptionBubble::OnTextChanged() {
  DCHECK(model_);
  std::string text = model_->GetFullText();
  label_->SetText(base::UTF8ToUTF16(text));
  UpdateBubbleAndTitleVisibility();
  if (GetWidget()->IsVisible())
    inactivity_timer_->Reset();
}

void CaptionBubble::OnErrorChanged() {
  DCHECK(model_);
  bool has_error = model_->HasError();
  label_->SetVisible(!has_error);
  error_message_->SetVisible(has_error);
  expand_button_->SetVisible(!has_error && !is_expanded_);
  collapse_button_->SetVisible(!has_error && is_expanded_);

  // The error is only 1 line, so redraw the bubble.
  Redraw();
}

void CaptionBubble::OnIsExpandedChanged() {
  expand_button_->SetVisible(!is_expanded_);
  collapse_button_->SetVisible(is_expanded_);

  // The change of expanded state may cause the title to change visibility, and
  // it surely causes the content height to change, so redraw the bubble.
  Redraw();
}

void CaptionBubble::UpdateBubbleAndTitleVisibility() {
  // Show the title if there is room for it and no error.
  title_->SetVisible(model_ && !model_->HasError() &&
                     GetNumLinesInLabel() <
                         static_cast<size_t>(GetNumLinesVisible()));
  UpdateBubbleVisibility();
}

void CaptionBubble::UpdateBubbleVisibility() {
  DCHECK(GetWidget());
  // If there is no model set, do not show the bubble.
  if (!model_) {
    Hide();
    return;
  }

  // Hide the widget if there is no room for it, the model is closed. or the
  // bubble has no activity. Activity is defined as transcription received from
  // the speech service or user interacting with the bubble through focus,
  // pressing buttons, or dragging.
  if (!can_layout_ || model_->IsClosed() || !HasActivity()) {
    Hide();
    return;
  }

  // Show the widget if it has text or an error to display.
  if (!model_->GetFullText().empty() || model_->HasError()) {
    if (!GetWidget()->IsVisible()) {
      GetWidget()->ShowInactive();
      GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
          IDS_LIVE_CAPTION_BUBBLE_APPEAR_SCREENREADER_ANNOUNCEMENT));
      LogSessionEvent(SessionEvent::kStreamStarted);
    }
    return;
  }

  // No text and no error. Hide it.
  Hide();
}

void CaptionBubble::OnWidgetVisibilityChanged(views::Widget* widget,
                                              bool visible) {
  DCHECK_EQ(widget, GetWidget());
  // The caption bubble can only be activated when it is visible. Nothing else,
  // including the focus manager, can activate the caption bubble.
  SetCanActivate(visible);
  // Ensure that the widget is deactivated when it is hidden.
  // TODO(crbug.com/1144201): Investigate whether Hide() should always
  // deactivate widgets, and if so, remove this.
  if (!visible)
    widget->Deactivate();
}

void CaptionBubble::UpdateCaptionStyle(
    base::Optional<ui::CaptionStyle> caption_style) {
  caption_style_ = caption_style;
  SetCaptionBubbleStyle();
  Redraw();
}

size_t CaptionBubble::GetTextIndexOfLineInLabel(size_t line) const {
  return label_->GetTextIndexOfLine(line);
}

size_t CaptionBubble::GetNumLinesInLabel() const {
  return label_->GetRequiredLines();
}

int CaptionBubble::GetNumLinesVisible() {
  return is_expanded_ ? kNumLinesExpanded : kNumLinesCollapsed;
}

void CaptionBubble::SetCaptionBubbleStyle() {
  SetTextSizeAndFontFamily();
  SetTextColor();
  SetBackgroundColor();
}

double CaptionBubble::GetTextScaleFactor() {
  double textScaleFactor = 1;
  if (caption_style_) {
    std::string text_size = MaybeRemoveCSSImportant(caption_style_->text_size);
    if (!text_size.empty()) {
      // ui::CaptionStyle states that text_size is percentage as a CSS string.
      bool match =
          RE2::FullMatch(text_size, "(\\d+\\.?\\d*)%", &textScaleFactor);
      textScaleFactor = match ? textScaleFactor / 100 : 1;
    }
  }
  return textScaleFactor;
}

void CaptionBubble::SetTextSizeAndFontFamily() {
  double textScaleFactor = GetTextScaleFactor();

  std::vector<std::string> font_names;
  if (caption_style_) {
    std::string font_family =
        MaybeRemoveCSSImportant(caption_style_->font_family);
    if (!font_family.empty())
      font_names.push_back(font_family);
  }
  font_names.push_back(kPrimaryFont);
  font_names.push_back(kSecondaryFont);
  font_names.push_back(kTertiaryFont);

  const gfx::FontList font_list =
      gfx::FontList(font_names, gfx::Font::FontStyle::NORMAL,
                    kFontSizePx * textScaleFactor, gfx::Font::Weight::NORMAL);
  label_->SetFontList(font_list);
  title_->SetFontList(font_list.DeriveWithStyle(gfx::Font::FontStyle::ITALIC));
  error_text_->SetFontList(font_list);

  label_->SetLineHeight(kLineHeightDip * textScaleFactor);
  label_->SetMaximumWidth(kMaxWidthDip * textScaleFactor - kSidePaddingDip * 2);
  title_->SetLineHeight(kLineHeightDip * textScaleFactor);
  error_text_->SetLineHeight(kLineHeightDip * textScaleFactor);
  error_icon_->SetImageSize(gfx::Size(kErrorImageSizeDip * textScaleFactor,
                                      kErrorImageSizeDip * textScaleFactor));
}

void CaptionBubble::SetTextColor() {
  SkColor text_color = SK_ColorWHITE;  // The default text color is white.
  if (caption_style_)
    ParseNonTransparentRGBACSSColorString(caption_style_->text_color,
                                          &text_color);
  label_->SetEnabledColor(text_color);
  title_->SetEnabledColor(text_color);
  error_text_->SetEnabledColor(text_color);

  error_icon_->SetImage(
      gfx::CreateVectorIcon(vector_icons::kErrorOutlineIcon, text_color));
  views::SetImageFromVectorIcon(close_button_, vector_icons::kCloseRoundedIcon,
                                kButtonDip, text_color);
  views::SetImageFromVectorIcon(expand_button_, kCaretDownIcon, kButtonDip,
                                text_color);
  views::SetImageFromVectorIcon(collapse_button_, kCaretUpIcon, kButtonDip,
                                text_color);

  close_button_->SetInkDropBaseColor(text_color);
  expand_button_->SetInkDropBaseColor(text_color);
  collapse_button_->SetInkDropBaseColor(text_color);
}

void CaptionBubble::SetBackgroundColor() {
  // The default background color is Google Grey 900 with 90% opacity.
  SkColor background_color =
      SkColorSetA(gfx::kGoogleGrey900, kCaptionBubbleAlpha);
  if (caption_style_ && !ParseNonTransparentRGBACSSColorString(
                            caption_style_->window_color, &background_color)) {
    ParseNonTransparentRGBACSSColorString(caption_style_->background_color,
                                          &background_color);
  }
  set_color(background_color);
  OnThemeChanged();  // Need to call `OnThemeChanged` after calling `set_color`.
}

void CaptionBubble::UpdateContentSize() {
  double text_scale_factor = GetTextScaleFactor();
  int width = kMaxWidthDip * text_scale_factor;
  int content_height =
      (model_ && model_->HasError())
          ? kLineHeightDip * text_scale_factor
          : kLineHeightDip * GetNumLinesVisible() * text_scale_factor;
  // The title takes up 1 line.
  int label_height = title_->GetVisible()
                         ? content_height - kLineHeightDip * text_scale_factor
                         : content_height;
  label_->SetPreferredSize(gfx::Size(width - kSidePaddingDip, label_height));
  content_container_->SetPreferredSize(gfx::Size(width, content_height));
  SetPreferredSize(gfx::Size(
      width, content_height + close_button_->GetPreferredSize().height() +
                 expand_button_->GetPreferredSize().height()));
}

void CaptionBubble::Redraw() {
  UpdateBubbleAndTitleVisibility();
  UpdateContentSize();
  SizeToContents();
}

void CaptionBubble::Hide() {
  if (GetWidget()->IsVisible()) {
    GetWidget()->Hide();
    LogSessionEvent(SessionEvent::kStreamEnded);
  }
}

void CaptionBubble::OnInactivityTimeout() {
  Hide();

  // Clear the partial and final text in the caption bubble model and the label.
  // Does not affect the speech service. The speech service will emit a final
  // result after ~10-15 seconds of no audio which the caption bubble will
  // receive but will not display. If the speech service is in the middle of a
  // recognition phrase, and the caption bubble regains activity (such as if the
  // audio stream restarts), the speech service will emit partial results that
  // contain text cleared by the UI.
  if (model_)
    model_->ClearText();
}

bool CaptionBubble::HasActivity() {
  return model_ && (inactivity_timer_->IsRunning() || HasFocus() ||
                    !model_->GetFullText().empty() || model_->HasError());
}

views::Label* CaptionBubble::GetLabelForTesting() {
  return static_cast<views::Label*>(label_);
}

base::RetainingOneShotTimer* CaptionBubble::GetInactivityTimerForTesting() {
  return inactivity_timer_.get();
}

BEGIN_METADATA(CaptionBubble, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace captions
