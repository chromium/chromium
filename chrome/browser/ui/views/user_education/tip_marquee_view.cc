// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/tip_marquee_view.h"

#include <memory>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/resources_util.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

constexpr int TipMarqueeView::kTipMarqueeIconSize;
constexpr int TipMarqueeView::kTipMarqueeIconPadding;
constexpr int TipMarqueeView::kTipMarqueeIconTotalWidth;

TipMarqueeView::TipMarqueeView(int text_context, int text_style) {
  tip_text_label_ = AddChildView(std::make_unique<views::StyledLabel>());
  tip_text_label_->SetTextContext(text_context);
  tip_text_label_->SetDefaultTextStyle(text_style);
  // TODO(dfried): Figure out how to set elide behavior.
  // tip_text_label_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

  // TODO(dfried): Limit to builds where GOOGLE_CHROME_BRANDING is set.
  chrome_icon_ = *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_PRODUCT_LOGO_16);

  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, kTipMarqueeIconTotalWidth, 0, 0)));

  SetVisible(false);
}

TipMarqueeView::~TipMarqueeView() = default;

bool TipMarqueeView::SetTip(
    const base::string16& tip_text,
    LearnMoreLinkClickedCallback learn_more_link_clicked_callback) {
  tip_text_ = tip_text;
  base::string16 full_tip = tip_text;
  const base::string16 separator = base::ASCIIToUTF16(" - ");
  const size_t tip_text_length = tip_text.length();
  const bool has_learn_more_link = !learn_more_link_clicked_callback.is_null();
  if (has_learn_more_link) {
    full_tip.append(separator);
    full_tip.append(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  }
  tip_text_label_->SetText(full_tip);
  if (has_learn_more_link) {
    tip_text_label_->AddStyleRange(
        gfx::Range(tip_text_length + separator.length(), full_tip.length()),
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &TipMarqueeView::LearnMoreLinkClicked, base::Unretained(this))));
  }
  learn_more_link_clicked_callback_ = learn_more_link_clicked_callback;
  collapsed_ = false;
  SetVisible(true);
  return !collapsed_;
}

void TipMarqueeView::ClearTip() {
  tip_text_label_->SetText(base::string16());
  tip_text_.clear();
  learn_more_link_clicked_callback_.Reset();
  SetVisible(false);
}

bool TipMarqueeView::OnMousePressed(const ui::MouseEvent& event) {
  if (!IsPointInIcon(event.location()))
    return false;
  if (!GetFitsInLayout() && learn_more_link_clicked_callback_) {
    LearnMoreLinkClicked();
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
    text_rect.Inset(0,
                    std::max(0, (text_rect.height() -
                                 tip_text_label_->GetPreferredSize().height()) /
                                    2));
    tip_text_label_->SetBoundsRect(text_rect);
  }
}

void TipMarqueeView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  canvas->DrawImageInt(chrome_icon_, 0, 0, kTipMarqueeIconSize,
                       kTipMarqueeIconSize, 0,
                       (height() - kTipMarqueeIconSize) / 2,
                       kTipMarqueeIconSize, kTipMarqueeIconSize, true);
}

base::string16 TipMarqueeView::GetTooltipText(const gfx::Point& p) const {
  if (!IsPointInIcon(p))
    return View::GetTooltipText(p);

  // TODO(pkasting): Localize
  if (tip_text_label_->GetVisible())
    return base::ASCIIToUTF16("Click to hide tip");
  if (GetFitsInLayout())
    return base::ASCIIToUTF16("Click to show tip");
  base::string16 result = tip_text_;
  result.append(base::ASCIIToUTF16(" - Click to learn more"));
  return result;
}

void TipMarqueeView::LearnMoreLinkClicked() {
  // TODO(dfried): in future, when we animate the tip out, this assumption may
  // not be valid.
  DCHECK(learn_more_link_clicked_callback_);
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

BEGIN_METADATA(TipMarqueeView, views::View)
ADD_READONLY_PROPERTY_METADATA(bool, FitsInLayout)
END_METADATA
