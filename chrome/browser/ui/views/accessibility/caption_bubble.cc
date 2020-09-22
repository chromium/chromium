// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
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
static constexpr int kCaptionBubbleAlpha = 230;  // 90% opacity
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

}  // namespace

namespace captions {
// CaptionBubble implementation of BubbleFrameView. This class takes care
// of making the caption draggable and handling the focus ring when the
// Caption Bubble is focused.
class CaptionBubbleFrameView : public views::BubbleFrameView {
 public:
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

  const char* GetClassName() const override { return "CaptionBubbleFrameView"; }

 private:
  views::View* close_button_;
  views::View* expand_button_;
  views::View* collapse_button_;
  views::FocusRing* focus_ring_ = nullptr;
  bool contents_focused_ = false;
};

CaptionBubble::CaptionBubble(views::View* anchor,
                             BrowserView* browser_view,
                             base::OnceClosure destroyed_callback)
    : BubbleDialogDelegateView(anchor,
                               views::BubbleBorder::FLOAT,
                               views::BubbleBorder::Shadow::NO_SHADOW),
      destroyed_callback_(std::move(destroyed_callback)),
      ratio_in_parent_x_(kDefaultRatioInParentX),
      ratio_in_parent_y_(kDefaultRatioInParentY),
      browser_view_(browser_view) {
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
  DCHECK(GetWidget());
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

  // Check that the widget which changed size is our widget. It's possible for
  // this to be called when another widget resizes.
  // Also check that our widget is visible. If it is not visible then
  // the user has not explicitly moved it (because the user can't see it),
  // so we should take no action.
  if (widget != GetWidget() || !GetWidget()->IsVisible())
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

  // TODO(crbug.com/1055150): Use system caption color scheme rather than
  // hard-coding the colors.
  SkColor caption_bubble_color_ =
      SkColorSetA(gfx::kGoogleGrey900, kCaptionBubbleAlpha);
  set_color(caption_bubble_color_);
  set_close_on_deactivate(false);

  auto label = std::make_unique<views::Label>();
  label->SetMultiLine(true);
  label->SetMaximumWidth(kMaxWidthDip - kSidePaddingDip * 2);
  label->SetEnabledColor(SK_ColorWHITE);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_TOP);
  label->SetTooltipText(base::string16());
  // Render text truncates the end of text that is greater than 10000 chars.
  // While it is unlikely that the text will exceed 10000 chars, it is not
  // impossible, if the speech service sends a very long transcription_result.
  // In order to guarantee that the caption bubble displays the last lines, and
  // in order to ensure that caption_bubble_->GetTextIndexOfLine() is correct,
  // set the truncate_length to 0 to ensure that it never truncates.
  label->SetTruncateLength(0);

  auto title = std::make_unique<views::Label>();
  title->SetEnabledColor(gfx::kGoogleGrey500);
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title->SetText(l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_TITLE));

  auto error_text = std::make_unique<views::Label>();
  error_text->SetEnabledColor(SK_ColorWHITE);
  error_text->SetBackgroundColor(SK_ColorTRANSPARENT);
  error_text->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  error_text->SetText(l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_ERROR));

  auto error_icon = std::make_unique<views::ImageView>();
  error_icon->SetImage(
      gfx::CreateVectorIcon(vector_icons::kErrorOutlineIcon, SK_ColorWHITE));

  auto error_message = std::make_unique<views::View>();
  error_message
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kErrorMessageBetweenChildSpacingDip))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  error_message->SetVisible(false);

  auto expand_button =
      BuildImageButton(kCaretDownIcon, IDS_LIVE_CAPTION_BUBBLE_EXPAND);
  expand_button->SetVisible(!is_expanded_);

  auto collapse_button =
      BuildImageButton(kCaretUpIcon, IDS_LIVE_CAPTION_BUBBLE_COLLAPSE);
  collapse_button->SetVisible(is_expanded_);

  auto close_button = BuildImageButton(vector_icons::kCloseRoundedIcon,
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

  UpdateTextSize();
  UpdateContentSize();
}

std::unique_ptr<views::ImageButton> CaptionBubble::BuildImageButton(
    const gfx::VectorIcon& icon,
    const int tooltip_text_id) {
  auto button = views::CreateVectorImageButton(this);
  views::SetImageFromVectorIcon(button.get(), icon, kButtonDip, SK_ColorWHITE);
  button->SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
  button->SetInkDropBaseColor(SkColor(gfx::kGoogleGrey600));
  button->SizeToPreferredSize();
  button->SetFocusForPlatform();
  views::InstallCircleHighlightPathGenerator(
      button.get(), gfx::Insets(kButtonCircleHighlightPaddingDip));
  return button;
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
    if (event->key_code() == ui::VKEY_UP) {
      offset.set_y(-kWidgetDisplacementWithArrowKeyDip);
    }
    if (event->key_code() == ui::VKEY_DOWN) {
      offset.set_y(kWidgetDisplacementWithArrowKeyDip);
    }
    if (event->key_code() == ui::VKEY_LEFT) {
      offset.set_x(-kWidgetDisplacementWithArrowKeyDip);
    }
    if (event->key_code() == ui::VKEY_RIGHT) {
      offset.set_x(kWidgetDisplacementWithArrowKeyDip);
    }
    if (offset != gfx::Vector2d()) {
      DCHECK(GetWidget());
      gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
      bounds.Offset(offset);
      GetWidget()->SetBounds(bounds);
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
}

void CaptionBubble::OnBlur() {
  frame_->UpdateFocusRing(false);
}

// TODO(crbug.com/1055150): Determine how this should be best exposed for screen
// readers without over-verbalizing. Currently it reads the full text when
// focused and does not announce when text changes.
void CaptionBubble::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (model_ && model_->HasError()) {
    node_data->SetName(error_text_->GetText());
    node_data->SetNameFrom(ax::mojom::NameFrom::kContents);
  } else if (model_ && !model_->GetFullText().empty()) {
    node_data->SetName(model_->GetFullText());
    node_data->SetNameFrom(ax::mojom::NameFrom::kContents);
  } else {
    node_data->SetName(title_->GetText());
    node_data->SetNameFrom(ax::mojom::NameFrom::kContents);
  }
  node_data->SetDescription(title_->GetText());
  node_data->role = ax::mojom::Role::kCaption;
}

void CaptionBubble::AddedToWidget() {
  DCHECK(GetWidget());
  DCHECK(GetAnchorView());
  DCHECK(anchor_widget());
  GetWidget()->SetFocusTraversableParent(
      anchor_widget()->GetFocusTraversable());
  GetWidget()->SetFocusTraversableParentView(GetAnchorView());
  GetAnchorView()->SetProperty(views::kAnchoredDialogKey,
                               static_cast<BubbleDialogDelegate*>(this));
}

void CaptionBubble::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  if (sender == close_button_) {
    // TODO(crbug.com/1055150): This histogram currently only reports a single
    // bucket, but it will eventually be extended to report session starts and
    // natural session ends (when the audio stream ends).
    UMA_HISTOGRAM_ENUMERATION(
        "Accessibility.LiveCaption.Session",
        CaptionController::SessionEvent::kCloseButtonClicked);
    if (model_)
      model_->Close();
  } else if (sender == expand_button_ || sender == collapse_button_) {
    is_expanded_ = !is_expanded_;
    bool button_had_focus = sender->HasFocus();
    views::Button* new_button =
        is_expanded_ ? collapse_button_ : expand_button_;
    OnIsExpandedChanged();
    // TODO(crbug.com/1055150): Ensure that the button keeps focus on mac.
    if (button_had_focus)
      new_button->RequestFocus();
  }
}

void CaptionBubble::SetModel(CaptionBubbleModel* model) {
  if (model_)
    model_->RemoveObserver();
  model_ = model;
  if (model_)
    model_->SetObserver(this);
}

void CaptionBubble::OnTextChanged() {
  DCHECK(model_);
  label_->SetText(base::UTF8ToUTF16(model_->GetFullText()));
  UpdateBubbleAndTitleVisibility();
}

void CaptionBubble::OnErrorChanged() {
  DCHECK(model_);
  bool has_error = model_->HasError();
  label_->SetVisible(!has_error);
  error_message_->SetVisible(has_error);

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
  if (!model_) {
    // If there is no model set, do not show the bubble.
    if (GetWidget()->IsVisible())
      GetWidget()->Hide();
  } else if (!can_layout_ || model_->IsClosed()) {
    // Hide the widget if there is no room for it or the model is closed.
    if (GetWidget()->IsVisible())
      GetWidget()->Hide();
  } else if (!model_->GetFullText().empty() || model_->HasError()) {
    // Show the widget if it has text or an error to display. Only show the
    // widget if it isn't already visible. Always calling Widget::Show() will
    // mean the widget gets focus each time.
    if (!GetWidget()->IsVisible())
      GetWidget()->Show();
  } else if (GetWidget()->IsVisible()) {
    // No text and no error. Hide it.
    GetWidget()->Hide();
  }
}

void CaptionBubble::UpdateCaptionStyle(
    base::Optional<ui::CaptionStyle> caption_style) {
  caption_style_ = caption_style;
  UpdateTextSize();
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

double CaptionBubble::GetTextScaleFactor() {
  double textScaleFactor = 1;
  if (caption_style_) {
    // ui::CaptionStyle states that text_size is percentage as a CSS string. It
    // can sometimes have !important which is why this is a partial match.
    bool match = RE2::PartialMatch(caption_style_->text_size, "(\\d+)%",
                                   &textScaleFactor);
    textScaleFactor = match ? textScaleFactor / 100 : 1;
  }
  return textScaleFactor;
}

void CaptionBubble::UpdateTextSize() {
  double textScaleFactor = GetTextScaleFactor();

  const gfx::FontList font_list =
      gfx::FontList({kPrimaryFont, kSecondaryFont, kTertiaryFont},
                    gfx::Font::FontStyle::NORMAL, kFontSizePx * textScaleFactor,
                    gfx::Font::Weight::NORMAL);
  label_->SetFontList(font_list);
  title_->SetFontList(font_list);
  error_text_->SetFontList(font_list);

  label_->SetLineHeight(kLineHeightDip * textScaleFactor);
  title_->SetLineHeight(kLineHeightDip * textScaleFactor);
  error_text_->SetLineHeight(kLineHeightDip * textScaleFactor);
  error_icon_->SetImageSize(gfx::Size(kErrorImageSizeDip * textScaleFactor,
                                      kErrorImageSizeDip * textScaleFactor));
}

void CaptionBubble::UpdateContentSize() {
  double text_scale_factor = GetTextScaleFactor();
  int content_height =
      (model_ && model_->HasError())
          ? kLineHeightDip * text_scale_factor
          : kLineHeightDip * GetNumLinesVisible() * text_scale_factor;
  // The title takes up 1 line.
  int label_height = title_->GetVisible()
                         ? content_height - kLineHeightDip * text_scale_factor
                         : content_height;
  label_->SetPreferredSize(
      gfx::Size(kMaxWidthDip - kSidePaddingDip, label_height));
  content_container_->SetPreferredSize(gfx::Size(kMaxWidthDip, content_height));
  SetPreferredSize(
      gfx::Size(kMaxWidthDip, content_height +
                                  close_button_->GetPreferredSize().height() +
                                  expand_button_->GetPreferredSize().height()));
}

void CaptionBubble::Redraw() {
  UpdateBubbleAndTitleVisibility();
  UpdateContentSize();
  SizeToContents();
}

const char* CaptionBubble::GetClassName() const {
  return "CaptionBubble";
}

std::string CaptionBubble::GetLabelTextForTesting() {
  return base::UTF16ToUTF8(label_->GetText());
}

}  // namespace captions
