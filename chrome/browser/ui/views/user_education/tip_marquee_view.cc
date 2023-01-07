// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/tip_marquee_view.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/resources_util.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr char16_t kTipMarqueeViewSeparator[] = u" - ";

// TODO(crbug.com/1171654): move to localized strings when out of tech demo mode
constexpr char16_t kTipMarqueeViewGotIt[] = u"Got it";
constexpr char16_t kTipMarqueeViewClickToHideTip[] = u"Click to hide tip";
constexpr char16_t kTipMarqueeViewClickToShowTip[] = u"Click to show tip";
constexpr char16_t kTipMarqueeViewClickToLearnMore[] = u"Click to learn more";

// ------------------------------------------------------------------
// TODO(crbug.com/1171654): remove the entire section below before this code
// ships, once the UX demo is over.
//
// This section provides the ability to show a demo tip marquee on startup with
// placeholder text, for UX evaluation only. This code should be removed before
// beta roll.
//
// Valid command-line arguments are:
//  --tip-marquee-view-test=simple
//      Displays a tip with no "learn more" link
//  --tip-marquee-view-test=learn-more
//      Displays a tip with a "learn more" link that displays a sample bubble
//
constexpr char kTipMarqueeViewTestSwitch[] = "tip-marquee-view-test";
constexpr char kTipMarqueeViewTestTypeSimple[] = "simple";
constexpr char kTipMarqueeViewTestTypeLearnMore[] = "learn-more";
constexpr char16_t kTipMarqueeViewTestTitleText[] = u"Lorem Ipsum";
constexpr char16_t kTipMarqueeViewTestText[] =
    u"Lorem ipsum dolor sit amet consectetur";
constexpr char16_t kTipMarqueeViewTestBodyText[] =
    u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    u"tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    u"veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    u"commodo consequat.";

class TestTipMarqueeViewLearnMoreBubble
    : public views::BubbleDialogDelegateView {
 public:
  explicit TestTipMarqueeViewLearnMoreBubble(TipMarqueeView* marquee)
      : BubbleDialogDelegateView(marquee, views::BubbleBorder::TOP_LEFT),
        marquee_(marquee) {
    SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
    SetButtonLabel(ui::DIALOG_BUTTON_OK, kTipMarqueeViewGotIt);
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   l10n_util::GetStringUTF16(IDS_CLOSE));
    SetAcceptCallback(base::BindOnce(
        &TestTipMarqueeViewLearnMoreBubble::OnAccept, base::Unretained(this)));
    set_close_on_deactivate(true);
    SetOwnedByWidget(true);

    auto* const layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kHorizontal);
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    layout->SetInteriorMargin(gfx::Insets::TLBR(10, 0, 0, 0));
    views::View* const placeholder_image =
        AddChildView(std::make_unique<views::View>());
    placeholder_image->SetPreferredSize(gfx::Size(150, 175));
    // In real UI, we wouldn't use kColorMidground directly, but rather create
    // a new color ID mapped to it (or similar).
    placeholder_image->SetBackground(
        views::CreateThemedSolidBackground(ui::kColorMidground));

    views::View* const rhs_view = AddChildView(std::make_unique<views::View>());
    auto* const rhs_layout =
        rhs_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
    rhs_layout->SetOrientation(views::LayoutOrientation::kVertical);
    rhs_layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    rhs_layout->SetInteriorMargin(gfx::Insets::TLBR(0, 16, 0, 0));

    auto* const title_text =
        rhs_view->AddChildView(std::make_unique<views::Label>(
            kTipMarqueeViewTestTitleText, views::style::CONTEXT_DIALOG_TITLE));
    title_text->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(6, 0, 10, 0));

    auto* const body_text = rhs_view->AddChildView(
        std::make_unique<views::Label>(kTipMarqueeViewTestBodyText,
                                       views::style::CONTEXT_DIALOG_BODY_TEXT));
    body_text->SetMultiLine(true);
    body_text->SetMaximumWidth(250);
    body_text->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  }

 private:
  void OnAccept() { marquee_->ClearAndHideTip(); }

  const raw_ptr<TipMarqueeView> marquee_;
};

void ShowTestTipMarqueeViewLearnMoreBubble(TipMarqueeView* marquee) {
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<TestTipMarqueeViewLearnMoreBubble>(marquee))
      ->Show();
}

void MaybeShowTestTipMarqueeView(TipMarqueeView* marquee) {
  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTipMarqueeViewTestSwitch)) {
    const std::string test_type =
        command_line->GetSwitchValueASCII(kTipMarqueeViewTestSwitch);
    if (test_type == kTipMarqueeViewTestTypeSimple) {
      marquee->SetAndShowTip(kTipMarqueeViewTestText);
    } else if (test_type == kTipMarqueeViewTestTypeLearnMore) {
      marquee->SetAndShowTip(
          kTipMarqueeViewTestText,
          base::BindRepeating(&ShowTestTipMarqueeViewLearnMoreBubble));
    } else {
      LOG(WARNING) << "Invalid switch value: --" << kTipMarqueeViewTestSwitch
                   << "=" << test_type;
    }
  }
}

// TODO(crbug.com/1171654): remove the entire section above before this code
// ships, once the UX demo is over.
// ------------------------------------------------------------------

// The width of the multiline text display in the overflow bubble.
constexpr int kTipMarqueeViewOverflowTextWidth = 250;

class TipMarqueeOverflowBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(TipMarqueeOverflowBubbleView);
  TipMarqueeOverflowBubbleView(TipMarqueeView* tip_marquee_view,
                               const std::u16string& text)
      : BubbleDialogDelegateView(tip_marquee_view,
                                 views::BubbleBorder::TOP_LEFT),
        tip_marquee_view_(tip_marquee_view) {
    SetButtons(ui::DIALOG_BUTTON_OK);
    SetButtonLabel(ui::DIALOG_BUTTON_OK, kTipMarqueeViewGotIt);
    SetAcceptCallback(base::BindOnce(&TipMarqueeOverflowBubbleView::OnAccept,
                                     base::Unretained(this)));
    set_close_on_deactivate(true);
    SetOwnedByWidget(true);

    auto* const label = AddChildView(std::make_unique<views::Label>(
        text, views::style::CONTEXT_DIALOG_BODY_TEXT));
    label->SetMultiLine(true);
    label->SetMaximumWidth(kTipMarqueeViewOverflowTextWidth);
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }
  ~TipMarqueeOverflowBubbleView() override = default;

 private:
  void OnAccept() {
    LOG(WARNING) << "OnAccept()";
    tip_marquee_view_->ClearAndHideTip();
  }

  const raw_ptr<TipMarqueeView> tip_marquee_view_;
};

BEGIN_METADATA(TipMarqueeOverflowBubbleView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace

constexpr int TipMarqueeView::kTipMarqueeIconSize;
constexpr int TipMarqueeView::kTipMarqueeIconPadding;
constexpr int TipMarqueeView::kTipMarqueeIconTotalWidth;

TipMarqueeView::TipMarqueeView() {
  tip_text_label_ = AddChildView(std::make_unique<views::StyledLabel>());
  tip_text_label_->SetTextContext(views::style::CONTEXT_LABEL);
  tip_text_label_->SetDefaultTextStyle(views::style::STYLE_PRIMARY);
  // TODO(dfried): Figure out how to set elide behavior.
  // tip_text_label_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, kTipMarqueeIconTotalWidth, 0, 0)));

  SetVisible(false);

  MaybeShowTestTipMarqueeView(this);
}

TipMarqueeView::~TipMarqueeView() = default;

bool TipMarqueeView::SetAndShowTip(
    const std::u16string& tip_text,
    LearnMoreLinkClickedCallback learn_more_link_clicked_callback) {
  tip_text_ = tip_text;
  std::u16string full_tip = tip_text;
  const std::u16string separator = kTipMarqueeViewSeparator;
  const size_t tip_text_length = tip_text.length();
  const bool has_learn_more_link = !learn_more_link_clicked_callback.is_null();
  full_tip.append(separator);
  if (has_learn_more_link)
    full_tip.append(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  else
    full_tip.append(kTipMarqueeViewGotIt);
  tip_text_label_->SetText(full_tip);
  tip_text_label_->AddStyleRange(
      gfx::Range(tip_text_length + separator.length(), full_tip.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &TipMarqueeView::LinkClicked, base::Unretained(this))));
  learn_more_link_clicked_callback_ = learn_more_link_clicked_callback;
  collapsed_ = false;
  SetVisible(true);
  return !collapsed_;
}

void TipMarqueeView::ClearAndHideTip() {
  if (show_tip_widget_)
    show_tip_widget_->Close();
  tip_text_label_->SetText(std::u16string());
  tip_text_.clear();
  learn_more_link_clicked_callback_.Reset();
  SetVisible(false);
}

bool TipMarqueeView::OnMousePressed(const ui::MouseEvent& event) {
  if (!IsPointInIcon(event.location()))
    return false;
  if (!GetFitsInLayout()) {
    if (learn_more_link_clicked_callback_)
      LinkClicked();
    else
      ToggleOverflowWidget();
  } else {
    collapsed_ = !collapsed_;
    InvalidateLayout();
  }
  return true;
}

gfx::Size TipMarqueeView::CalculatePreferredSize() const {
  if (collapsed_)
    return GetMinimumSize();

  const gfx::Size label_size = tip_text_label_->GetPreferredSize();
  const int width = label_size.width() + kTipMarqueeIconTotalWidth;
  const int height = std::max(label_size.height(), kTipMarqueeIconSize);
  return gfx::Size(width, height);
}

gfx::Size TipMarqueeView::GetMinimumSize() const {
  return gfx::Size(kTipMarqueeIconSize, kTipMarqueeIconSize);
}

void TipMarqueeView::Layout() {
  // TODO(dfried): animate label
  if (collapsed_ || size().width() < GetPreferredSize().width()) {
    tip_text_label_->SetVisible(false);
  } else {
    tip_text_label_->SetVisible(true);
    gfx::Rect text_rect = GetContentsBounds();
    text_rect.Inset(gfx::Insets::VH(
        std::max(0, (text_rect.height() -
                     tip_text_label_->GetPreferredSize().height()) /
                        2),
        0));
    tip_text_label_->SetBoundsRect(text_rect);
  }
}

void TipMarqueeView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  gfx::ImageSkia* const icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PRODUCT_LOGO_16);
  canvas->DrawImageInt(*icon, 0, 0, kTipMarqueeIconSize, kTipMarqueeIconSize, 0,
                       (height() - kTipMarqueeIconSize) / 2,
                       kTipMarqueeIconSize, kTipMarqueeIconSize, true);
}

std::u16string TipMarqueeView::GetTooltipText(const gfx::Point& p) const {
  if (!GetVisible() || !IsPointInIcon(p))
    return View::GetTooltipText(p);

  // TODO(pkasting): Localize
  if (tip_text_label_->GetVisible())
    return kTipMarqueeViewClickToHideTip;
  if (GetFitsInLayout())
    return kTipMarqueeViewClickToShowTip;
  std::u16string result = tip_text_;
  if (learn_more_link_clicked_callback_) {
    result.append(kTipMarqueeViewSeparator);
    result.append(kTipMarqueeViewClickToLearnMore);
  }
  return result;
}

void TipMarqueeView::LinkClicked() {
  if (!learn_more_link_clicked_callback_) {
    ClearAndHideTip();
    return;
  }
  if (GetProperty(views::kAnchoredDialogKey))
    return;
  learn_more_link_clicked_callback_.Run(this);
}

bool TipMarqueeView::GetFitsInLayout() const {
  const views::SizeBounds available = parent()->GetAvailableSize(this);
  if (!available.width().is_bounded())
    return true;
  return available.width().value() >=
         tip_text_label_->GetPreferredSize().width() +
             kTipMarqueeIconTotalWidth;
}

bool TipMarqueeView::IsPointInIcon(const gfx::Point& p) const {
  const int pos = GetMirroredXInView(p.x());
  return pos < kTipMarqueeIconTotalWidth;
}

void TipMarqueeView::ToggleOverflowWidget() {
  if (show_tip_widget_) {
    show_tip_widget_->Close();
    return;
  }

  DCHECK(!tip_text_.empty());
  DCHECK(!show_tip_widget_);
  show_tip_widget_ = views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<TipMarqueeOverflowBubbleView>(this, tip_text_));
  widget_observer_.Observe(show_tip_widget_.get());
  show_tip_widget_->Show();
}

void TipMarqueeView::OnWidgetDestroying(views::Widget* widget) {
  widget_observer_.Reset();
  if (widget != show_tip_widget_)
    return;
  show_tip_widget_ = nullptr;
}

BEGIN_METADATA(TipMarqueeView, views::View)
ADD_READONLY_PROPERTY_METADATA(bool, FitsInLayout)
END_METADATA
