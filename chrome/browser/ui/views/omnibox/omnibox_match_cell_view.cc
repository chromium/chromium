// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"

#include <algorithm>

#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "extensions/common/image_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/layout_provider.h"

namespace {

// The diameter of the answer layout images.
static constexpr int kAnswerImageSize = 24;

// The edge length of the entity suggestions images.
static constexpr int kEntityImageSize = 32;

////////////////////////////////////////////////////////////////////////////////
// PlaceholderImageSource:

class PlaceholderImageSource : public gfx::CanvasImageSource {
 public:
  PlaceholderImageSource(const gfx::Size& canvas_size, SkColor color);
  ~PlaceholderImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

 private:
  const SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(PlaceholderImageSource);
};

PlaceholderImageSource::PlaceholderImageSource(const gfx::Size& canvas_size,
                                               SkColor color)
    : gfx::CanvasImageSource(canvas_size), color_(color) {}

void PlaceholderImageSource::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStrokeAndFill_Style);
  flags.setColor(color_);
  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MEDIUM);
  canvas->sk_canvas()->drawRoundRect(gfx::RectToSkRect(gfx::Rect(size())),
                                     corner_radius, corner_radius, flags);
}

////////////////////////////////////////////////////////////////////////////////
// EncircledImageSource:

class EncircledImageSource : public gfx::CanvasImageSource {
 public:
  EncircledImageSource(int radius, SkColor color, const gfx::ImageSkia& image);
  ~EncircledImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

 private:
  const int radius_;
  const SkColor color_;
  const gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(EncircledImageSource);
};

EncircledImageSource::EncircledImageSource(int radius,
                                           SkColor color,
                                           const gfx::ImageSkia& image)
    : gfx::CanvasImageSource(gfx::Size(radius * 2, radius * 2)),
      radius_(radius),
      color_(color),
      image_(image) {}

void EncircledImageSource::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStrokeAndFill_Style);
  flags.setColor(color_);
  canvas->DrawCircle(gfx::Point(radius_, radius_), radius_, flags);
  const int x = radius_ - image_.width() / 2;
  const int y = radius_ - image_.height() / 2;
  canvas->DrawImageInt(image_, x, y);
}

////////////////////////////////////////////////////////////////////////////////
// RoundedCornerImageView:

class RoundedCornerImageView : public views::ImageView {
 public:
  RoundedCornerImageView() = default;

  // views::ImageView:
  bool CanProcessEventsWithinSubtree() const override { return false; }

 protected:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(RoundedCornerImageView);
};

void RoundedCornerImageView::OnPaint(gfx::Canvas* canvas) {
  SkPath mask;
  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MEDIUM);
  mask.addRoundRect(gfx::RectToSkRect(GetImageBounds()), corner_radius,
                    corner_radius);
  canvas->ClipPath(mask, true);
  ImageView::OnPaint(canvas);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxMatchCellView:

OmniboxMatchCellView::OmniboxMatchCellView(OmniboxResultView* result_view) {
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  answer_image_view_ = AddChildView(std::make_unique<RoundedCornerImageView>());
  content_view_ = AddChildView(std::make_unique<OmniboxTextView>(result_view));
  description_view_ =
      AddChildView(std::make_unique<OmniboxTextView>(result_view));
  separator_view_ =
      AddChildView(std::make_unique<OmniboxTextView>(result_view));
  separator_view_->SetText(
      l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR));
}

OmniboxMatchCellView::~OmniboxMatchCellView() = default;

// static
int OmniboxMatchCellView::GetTextIndent() {
  return ui::MaterialDesignController::touch_ui() ? 51 : 47;
}

void OmniboxMatchCellView::OnMatchUpdate(const OmniboxResultView* result_view,
                                         const AutocompleteMatch& match) {
  is_rich_suggestion_ = match.answer ||
                        match.type == AutocompleteMatchType::CALCULATOR ||
                        !match.image_url.empty();
  is_search_type_ = AutocompleteMatch::IsSearchType(match.type);

  // Decide layout style once before Layout, while match data is available.
  const bool two_line =
      is_rich_suggestion_ || match.ShouldShowTabMatchButton() || match.pedal;
  layout_style_ = two_line ? LayoutStyle::TWO_LINE_SUGGESTION
                           : LayoutStyle::ONE_LINE_SUGGESTION;

  // Set up the separator.
  separator_view_->SetSize(two_line ? gfx::Size()
                                    : separator_view_->GetPreferredSize());

  // Set up the small icon.
  icon_view_->SetSize(is_rich_suggestion_ ? gfx::Size()
                                          : icon_view_->GetPreferredSize());

  const auto apply_vector_icon = [=](const gfx::VectorIcon& vector_icon) {
    const auto& icon = gfx::CreateVectorIcon(vector_icon, SK_ColorWHITE);
    answer_image_view_->SetImage(
        gfx::CanvasImageSource::MakeImageSkia<EncircledImageSource>(
            kAnswerImageSize / 2, gfx::kGoogleBlue600, icon));
  };
  if (match.type == AutocompleteMatchType::CALCULATOR) {
    apply_vector_icon(omnibox::kAnswerCalculatorIcon);
  } else if (!is_rich_suggestion_) {
    answer_image_view_->SetImage(gfx::ImageSkia());
    answer_image_view_->SetSize(gfx::Size());
  } else {
    // Determine if we have a local icon (or else it will be downloaded).
    if (match.answer) {
      switch (match.answer->type()) {
        case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
          apply_vector_icon(omnibox::kAnswerCurrencyIcon);
          break;
        case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
          apply_vector_icon(omnibox::kAnswerDictionaryIcon);
          break;
        case SuggestionAnswer::ANSWER_TYPE_FINANCE:
          apply_vector_icon(omnibox::kAnswerFinanceIcon);
          break;
        case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
          apply_vector_icon(omnibox::kAnswerSunriseIcon);
          break;
        case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
          apply_vector_icon(omnibox::kAnswerTranslationIcon);
          break;
        case SuggestionAnswer::ANSWER_TYPE_WEATHER:
          // Weather icons are downloaded. Do nothing.
          break;
        case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
          apply_vector_icon(omnibox::kAnswerWhenIsIcon);
          break;
        default:
          apply_vector_icon(omnibox::kAnswerDefaultIcon);
          break;
      }
      // Always set the image size so that downloaded images get the correct
      // size (such as Weather answers).
      answer_image_view_->SetImageSize(
          gfx::Size(kAnswerImageSize, kAnswerImageSize));
    } else {
      SkColor color = result_view->GetColor(OmniboxPart::RESULTS_BACKGROUND);
      extensions::image_util::ParseHexColorString(match.image_dominant_color,
                                                  &color);
      color = SkColorSetA(color, 0x40);  // 25% transparency (arbitrary).
      constexpr gfx::Size size(kEntityImageSize, kEntityImageSize);
      answer_image_view_->SetImageSize(size);
      answer_image_view_->SetImage(
          gfx::CanvasImageSource::MakeImageSkia<PlaceholderImageSource>(size,
                                                                        color));
    }
  }
  SetTailSuggestCommonPrefixWidth(
      (match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL)
          ? match.tail_suggest_common_prefix  // Used for indent calculation.
          : base::string16());
}

void OmniboxMatchCellView::SetImage(const gfx::ImageSkia& image) {
  answer_image_view_->SetImage(image);

  // Usually, answer images are square. But if that's not the case, setting
  // answer_image_view_ size proportional to the image size preserves
  // the aspect ratio.
  int width = image.width();
  int height = image.height();
  if (width == height)
    return;
  const int max = std::max(width, height);
  width = kEntityImageSize * width / max;
  height = kEntityImageSize * height / max;
  answer_image_view_->SetImageSize(gfx::Size(width, height));
}

const char* OmniboxMatchCellView::GetClassName() const {
  return "OmniboxMatchCellView";
}

gfx::Insets OmniboxMatchCellView::GetInsets() const {
  const bool single_line = layout_style_ == LayoutStyle::ONE_LINE_SUGGESTION;
  const int vertical_margin = single_line ? 8 : 4;
  return gfx::Insets(vertical_margin, 4, vertical_margin,
                     OmniboxMatchCellView::kMarginRight);
}

void OmniboxMatchCellView::Layout() {
  views::View::Layout();

  const bool two_line = layout_style_ == LayoutStyle::TWO_LINE_SUGGESTION;
  const gfx::Rect child_area = GetContentsBounds();
  int x = child_area.x();
  int y = child_area.y();
  const int row_height = child_area.height();
  views::ImageView* const image_view =
      (two_line && is_rich_suggestion_) ? answer_image_view_ : icon_view_;
  image_view->SetBounds(x, y, 40, row_height);

  const int text_indent = GetTextIndent() + tail_suggest_common_prefix_width_;
  x += text_indent;
  const int text_width = child_area.width() - text_indent;

  if (two_line) {
    if (description_view_->text().empty()) {
      // This vertically centers content in the rare case that no description is
      // provided.
      content_view_->SetBounds(x, y, text_width, row_height);
      description_view_->SetSize(gfx::Size());
    } else {
      content_view_->SetBounds(x, y, text_width,
                               content_view_->GetLineHeight());
      description_view_->SetBounds(
          x, content_view_->bounds().bottom(), text_width,
          description_view_->GetHeightForWidth(text_width));
    }
  } else {
    int content_width = content_view_->GetPreferredSize().width();
    int description_width = description_view_->GetPreferredSize().width();
    const gfx::Size separator_size = separator_view_->GetPreferredSize();
    OmniboxPopupModel::ComputeMatchMaxWidths(
        content_width, separator_size.width(), description_width, text_width,
        /*description_on_separate_line=*/false, !is_search_type_,
        &content_width, &description_width);
    content_view_->SetBounds(x, y, content_width, row_height);
    if (description_width) {
      x += content_view_->width();
      separator_view_->SetSize(separator_size);
      separator_view_->SetBounds(x, y, separator_view_->width(), row_height);
      x += separator_view_->width();
      description_view_->SetBounds(x, y, description_width, row_height);
    } else {
      separator_view_->SetSize(gfx::Size());
      description_view_->SetSize(gfx::Size());
    }
  }
}

bool OmniboxMatchCellView::CanProcessEventsWithinSubtree() const {
  return false;
}

gfx::Size OmniboxMatchCellView::CalculatePreferredSize() const {
  int height = content_view_->GetLineHeight() + GetInsets().height();
  if (layout_style_ == LayoutStyle::TWO_LINE_SUGGESTION)
    height += description_view_->GetHeightForWidth(width() - GetTextIndent());
  // Width is not calculated because it's not needed by current callers.
  return gfx::Size(0, height);
}

void OmniboxMatchCellView::SetTailSuggestCommonPrefixWidth(
    const base::string16& common_prefix) {
  InvalidateLayout();
  if (common_prefix.empty()) {
    tail_suggest_common_prefix_width_ = 0;
    return;
  }
  std::unique_ptr<gfx::RenderText> render_text =
      content_view_->CreateRenderText(common_prefix);
  tail_suggest_common_prefix_width_ = render_text->GetStringSize().width();
  // Only calculate fixed string width once.
  if (!ellipsis_width_) {
    render_text->SetText(base::ASCIIToUTF16(AutocompleteMatch::kEllipsis));
    ellipsis_width_ = render_text->GetStringSize().width();
  }
  // Indent text by prefix, but come back by width of ellipsis.
  tail_suggest_common_prefix_width_ -= ellipsis_width_;
}
