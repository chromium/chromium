// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"

#include <algorithm>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/common/color_parser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"

namespace {

// The edge length of the favicon, answer icon, and entity backgrounds if the
// kUniformRowHeight flag is enabled.
static constexpr int kUniformRowHeightIconSize = 28;

// The edge length of favicon backgrounds.
int GetIconSize() {
  // When `kSquareSuggestIconIcons` is disabled, icons don't have backgrounds
  // and aren't resized in `OmniboxMatchCellView`.
  DCHECK(OmniboxFieldTrial::kSquareSuggestIconIcons.Get());
  return kUniformRowHeightIconSize;
}

// The size (edge length or diameter) of the answer icon backgrounds (which may
// be squares or circles).
int GetAnswerImageSize() {
  return OmniboxFieldTrial::kSquareSuggestIconAnswers.Get()
             ? kUniformRowHeightIconSize  // Square edge length.
             : 24;                        // Circle diameter.
}

// The edge length of the entity suggestions images.
int GetEntityImageSize() {
  return OmniboxFieldTrial::IsUniformRowHeightEnabled()
             ? kUniformRowHeightIconSize
             : 32;
}

// The radius of the rounded square backgrounds of icons, answers, and entities.
int GetIconAndImageCornerRadius() {
  // When all params are disabled, icons and images won't have rounded square
  // backgrounds.
  DCHECK(OmniboxFieldTrial::kSquareSuggestIconAnswers.Get() ||
         OmniboxFieldTrial::kSquareSuggestIconIcons.Get() ||
         OmniboxFieldTrial::kSquareSuggestIconEntities.Get());
  return 4;
}

// The size of entities relative to their background. 0.5 means entities take up
// half of the space.
double GetEntityBackgroundScale() {
  // When `kSquareSuggestIconEntities` is disabled, entities shouldn't be
  // scaled.
  DCHECK(OmniboxFieldTrial::kSquareSuggestIconEntities.Get());
  double scale = OmniboxFieldTrial::kSquareSuggestIconEntitiesScale.Get();
  DCHECK_GT(scale, 0);
  DCHECK_LE(scale, 1);
  return scale;
}

////////////////////////////////////////////////////////////////////////////////
// PlaceholderImageSource:

class PlaceholderImageSource : public gfx::CanvasImageSource {
 public:
  PlaceholderImageSource(const gfx::Size& canvas_size, SkColor color);
  PlaceholderImageSource(const PlaceholderImageSource&) = delete;
  PlaceholderImageSource& operator=(const PlaceholderImageSource&) = delete;
  ~PlaceholderImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

 private:
  const SkColor color_;
};

PlaceholderImageSource::PlaceholderImageSource(const gfx::Size& canvas_size,
                                               SkColor color)
    : gfx::CanvasImageSource(canvas_size), color_(color) {}

void PlaceholderImageSource::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color_);
  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMedium);
  canvas->sk_canvas()->drawRoundRect(gfx::RectToSkRect(gfx::Rect(size())),
                                     corner_radius, corner_radius, flags);
}

////////////////////////////////////////////////////////////////////////////////
// RoundedCornerImageView:

class RoundedCornerImageView : public views::ImageView {
 public:
  METADATA_HEADER(RoundedCornerImageView);
  RoundedCornerImageView() = default;
  RoundedCornerImageView(const RoundedCornerImageView&) = delete;
  RoundedCornerImageView& operator=(const RoundedCornerImageView&) = delete;

  // views::ImageView:
  bool GetCanProcessEventsWithinSubtree() const override { return false; }

 protected:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;
};

void RoundedCornerImageView::OnPaint(gfx::Canvas* canvas) {
  SkPath mask;
  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMedium);
  mask.addRoundRect(gfx::RectToSkRect(GetImageBounds()), corner_radius,
                    corner_radius);
  canvas->ClipPath(mask, true);
  ImageView::OnPaint(canvas);
}

BEGIN_METADATA(RoundedCornerImageView, views::ImageView)
END_METADATA

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxMatchCellView:

// static
void OmniboxMatchCellView::ComputeMatchMaxWidths(
    int contents_width,
    int separator_width,
    int description_width,
    int available_width,
    bool description_on_separate_line,
    bool allow_shrinking_contents,
    int* contents_max_width,
    int* description_max_width) {
  available_width = std::max(available_width, 0);
  *contents_max_width = std::min(contents_width, available_width);
  *description_max_width = std::min(description_width, available_width);

  // If the description is empty, or the contents and description are on
  // separate lines, each can get the full available width.
  if (!description_width || description_on_separate_line)
    return;

  // If we want to display the description, we need to reserve enough space for
  // the separator.
  available_width -= separator_width;
  if (available_width < 0) {
    *description_max_width = 0;
    return;
  }

  if (contents_width + description_width > available_width) {
    if (allow_shrinking_contents) {
      // Try to split the available space fairly between contents and
      // description (if one wants less than half, give it all it wants and
      // give the other the remaining space; otherwise, give each half).
      // However, if this makes the contents too narrow to show a significant
      // amount of information, give the contents more space.
      *contents_max_width = std::max((available_width + 1) / 2,
                                     available_width - description_width);

      const int kMinimumContentsWidth = 300;
      *contents_max_width = std::min(
          std::min(std::max(*contents_max_width, kMinimumContentsWidth),
                   contents_width),
          available_width);
    }

    // Give the description the remaining space, unless this makes it too small
    // to display anything meaningful, in which case just hide the description
    // and let the contents take up the whole width.
    *description_max_width =
        std::min(description_width, available_width - *contents_max_width);
    const int kMinimumDescriptionWidth = 75;
    if (*description_max_width <
        std::min(description_width, kMinimumDescriptionWidth)) {
      *description_max_width = 0;
      // Since we're not going to display the description, the contents can have
      // the space we reserved for the separator.
      available_width += separator_width;
      *contents_max_width = std::min(contents_width, available_width);
    }
  }
}

OmniboxMatchCellView::OmniboxMatchCellView(OmniboxResultView* result_view) {
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  answer_image_view_ = AddChildView(std::make_unique<RoundedCornerImageView>());
  tail_suggest_ellipse_view_ =
      AddChildView(std::make_unique<OmniboxTextView>(result_view));
  tail_suggest_ellipse_view_->SetText(AutocompleteMatch::kEllipsis);
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
  return ui::TouchUiController::Get()->touch_ui() ||
                 OmniboxFieldTrial::IsCr23LayoutEnabled()
             ? 52
             : 47;
}

// static
bool OmniboxMatchCellView::ShouldDisplayImage(const AutocompleteMatch& match) {
  return match.answer || match.type == AutocompleteMatchType::CALCULATOR ||
         !match.image_url.is_empty();
}

void OmniboxMatchCellView::OnMatchUpdate(const OmniboxResultView* result_view,
                                         const AutocompleteMatch& match) {
  is_search_type_ = AutocompleteMatch::IsSearchType(match.type);
  has_image_ = ShouldDisplayImage(match);
  // Decide layout style once before Layout, while match data is available.
  layout_style_ = has_image_ && !OmniboxFieldTrial::IsUniformRowHeightEnabled()
                      ? LayoutStyle::TWO_LINE_SUGGESTION
                      : LayoutStyle::ONE_LINE_SUGGESTION;

  tail_suggest_ellipse_view_->SetVisible(
      !match.tail_suggest_common_prefix.empty());
  tail_suggest_ellipse_view_->ApplyTextColor(
      result_view->GetThemeState() == OmniboxPartState::SELECTED
          ? kColorOmniboxResultsTextSelected
          : kColorOmniboxText);

  // Set up the separator.
  separator_view_->SetSize(layout_style_ == LayoutStyle::TWO_LINE_SUGGESTION ||
                                   match.description.empty()
                               ? gfx::Size()
                               : separator_view_->GetPreferredSize());

  // Set up the small icon.
  icon_view_->SetSize(has_image_ ? gfx::Size()
                                 : icon_view_->GetPreferredSize());

  // Used for non-weather answer images (e.g. calc answers).
  const auto apply_vector_icon = [=](const gfx::VectorIcon& vector_icon) {
    const auto* color_provider = GetColorProvider();
    const auto foreground_color_id =
        OmniboxFieldTrial::kSquareSuggestIconAnswers.Get()
            ? kColorOmniboxAnswerIconGM3Foreground
            : kColorOmniboxAnswerIconForeground;
    const auto background_color_id =
        OmniboxFieldTrial::kSquareSuggestIconAnswers.Get()
            ? kColorOmniboxAnswerIconGM3Background
            : kColorOmniboxAnswerIconBackground;
    const auto& icon = gfx::CreateVectorIcon(
        vector_icon, color_provider->GetColor(foreground_color_id));
    answer_image_view_->SetImageSize(
        gfx::Size(GetAnswerImageSize(), GetAnswerImageSize()));
    if (OmniboxFieldTrial::kSquareSuggestIconAnswers.Get()) {
      answer_image_view_->SetImage(
          gfx::ImageSkiaOperations::CreateImageWithRoundRectBackground(
              GetAnswerImageSize(), GetIconAndImageCornerRadius(),
              color_provider->GetColor(background_color_id), icon));
    } else {
      answer_image_view_->SetImage(
          gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
              GetAnswerImageSize() / 2,
              color_provider->GetColor(background_color_id), icon));
    }
  };
  if (match.type == AutocompleteMatchType::CALCULATOR) {
    apply_vector_icon(omnibox::kAnswerCalculatorIcon);
    if (OmniboxFieldTrial::IsUniformRowHeightEnabled()) {
      separator_view_->SetSize(gfx::Size());
    }
  } else if (!has_image_) {
    answer_image_view_->SetImage(gfx::ImageSkia());
    answer_image_view_->SetSize(gfx::Size());
  } else {
    // Determine if we have a local icon (or else it will be downloaded).
    if (match.answer) {
      if (match.answer->type() == SuggestionAnswer::ANSWER_TYPE_WEATHER) {
        // Weather icons are downloaded. We just need to set the correct size.
        answer_image_view_->SetImageSize(
            gfx::Size(GetAnswerImageSize(), GetAnswerImageSize()));
      } else {
        apply_vector_icon(
            AutocompleteMatch::AnswerTypeToAnswerIcon(match.answer->type()));
      }
    } else {
      SkColor color = GetColorProvider()->GetColor(
          GetOmniboxBackgroundColorId(result_view->GetThemeState()));
      content::ParseHexColorString(match.image_dominant_color, &color);
      color = SkColorSetA(color, 0x40);  // 25% transparency (arbitrary).

      const auto size_px = GetEntityImageSize();

      gfx::Size size(size_px, size_px);
      answer_image_view_->SetImageSize(size);
      answer_image_view_->SetImage(
          gfx::CanvasImageSource::MakeImageSkia<PlaceholderImageSource>(size,
                                                                        color));
    }
  }
  SetTailSuggestCommonPrefixWidth(
      (match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL)
          ? match.tail_suggest_common_prefix  // Used for indent calculation.
          : std::u16string());
}

void OmniboxMatchCellView::SetIcon(const gfx::ImageSkia& image) {
  if (OmniboxFieldTrial::kSquareSuggestIconIcons.Get()) {
    icon_view_->SetImage(
        gfx::ImageSkiaOperations::CreateImageWithRoundRectBackground(
            GetIconSize(), GetIconAndImageCornerRadius(),
            GetColorProvider()->GetColor(kColorOmniboxResultsIconGM3Background),
            image));
  } else {
    icon_view_->SetImage(image);
  }
}

void OmniboxMatchCellView::ClearIcon() {
  icon_view_->SetImage({});
}

void OmniboxMatchCellView::SetImage(const gfx::ImageSkia& image,
                                    const AutocompleteMatch& match) {
  // Weather icons are also sourced remotely and therefore fall into this flow.
  // Other answers don't.
  bool is_weather_answer =
      match.answer &&
      match.answer->type() == SuggestionAnswer::ANSWER_TYPE_WEATHER;

  int width = image.width();
  int height = image.height();
  const int max = std::max(width, height);

  if (OmniboxFieldTrial::kSquareSuggestIconEntities.Get() &&
      !is_weather_answer) {
    answer_image_view_->SetImage(
        gfx::ImageSkiaOperations::CreateImageWithRoundRectBackground(
            max / GetEntityBackgroundScale(), GetIconAndImageCornerRadius(),
            GetColorProvider()->GetColor(kColorOmniboxResultsIconGM3Background),
            gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
                GetIconAndImageCornerRadius(), image)));

  } else {
    answer_image_view_->SetImage(image);

    // Usually, answer images are square. But if that's not the case, setting
    // answer_image_view_ size proportional to the image size preserves
    // the aspect ratio.
    if (width == height)
      return;
    int imageSize = GetEntityImageSize();
    width = imageSize * width / max;
    height = imageSize * height / max;
    answer_image_view_->SetImageSize(gfx::Size(width, height));
  }
}

gfx::Insets OmniboxMatchCellView::GetInsets() const {
  int vertical_margin = 0;
  if (OmniboxFieldTrial::IsChromeRefreshSuggestHoverFillShapeEnabled()) {
    vertical_margin = 0;
  } else if (OmniboxFieldTrial::IsUniformRowHeightEnabled()) {
    vertical_margin = OmniboxFieldTrial::kRichSuggestionVerticalMargin.Get();
  } else if (layout_style_ == LayoutStyle::ONE_LINE_SUGGESTION) {
    vertical_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_OMNIBOX_CELL_VERTICAL_PADDING);
  } else {
    vertical_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_OMNIBOX_TWO_LINE_CELL_VERTICAL_PADDING);
  }
  return gfx::Insets::TLBR(vertical_margin, OmniboxMatchCellView::kMarginLeft,
                           vertical_margin, OmniboxMatchCellView::kMarginRight);
}

void OmniboxMatchCellView::Layout() {
  views::View::Layout();

  const bool two_line = layout_style_ == LayoutStyle::TWO_LINE_SUGGESTION;
  const gfx::Rect child_area = GetContentsBounds();
  int x = child_area.x();
  int y = child_area.y();

  const int row_height = child_area.height();

  // The entity, answer, and icon images are horizontally centered within their
  // bounds. So their center-line will be at `image_x+kImageBoundsWidth/2`. This
  // means their left x coordinate will depend on their actual sizes. Their
  // widths depend on the state of `kSquareSuggestIcons`, its params, and
  // `kUniformRowHeight`. This code guarantees when cr23_layout is true:
  // a) Entities' left x coordinate is 16.
  // b) Entities, answers, and icons continue to be center-aligned.
  // c) Regardless of the state of those other features and their widths.
  // This applies to both touch-UI and non-touch-UI.
  // TODO(manukh): Once we have a clearer picture of what will launch, this can
  //   be simplified.
  const int image_x =
      OmniboxFieldTrial::IsCr23LayoutEnabled()
          ? 16 + GetEntityImageSize() / 2 - kImageBoundsWidth / 2
          : x;
  views::ImageView* const image_view =
      has_image_ ? answer_image_view_.get() : icon_view_.get();
  image_view->SetBounds(image_x, y, kImageBoundsWidth, row_height);

  const int text_indent = GetTextIndent() + tail_suggest_common_prefix_width_;
  x += text_indent;
  const int text_width = child_area.width() - text_indent;

  if (two_line) {
    if (description_view_->GetText().empty()) {
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
    ComputeMatchMaxWidths(content_width, separator_size.width(),
                          description_width, text_width,
                          /*description_on_separate_line=*/false,
                          !is_search_type_, &content_width, &description_width);
    if (tail_suggest_ellipse_view_->GetVisible()) {
      const int tail_suggest_ellipse_width =
          tail_suggest_ellipse_view_->GetPreferredSize().width();
      tail_suggest_ellipse_view_->SetBounds(x - tail_suggest_ellipse_width, y,
                                            tail_suggest_ellipse_width,
                                            row_height);
    }
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

bool OmniboxMatchCellView::GetCanProcessEventsWithinSubtree() const {
  return false;
}

gfx::Size OmniboxMatchCellView::CalculatePreferredSize() const {
  int height = 0;
  if (OmniboxFieldTrial::IsChromeRefreshSuggestHoverFillShapeEnabled()) {
    height = GetEntityImageSize();
  } else if (OmniboxFieldTrial::IsUniformRowHeightEnabled()) {
    height = GetEntityImageSize() +
             2 * OmniboxFieldTrial::kRichSuggestionVerticalMargin.Get();
  } else {
    height = content_view_->GetLineHeight() + GetInsets().height();
  }
  if (layout_style_ == LayoutStyle::TWO_LINE_SUGGESTION)
    height += description_view_->GetHeightForWidth(width() - GetTextIndent());
  // Width is not calculated because it's not needed by current callers.
  return gfx::Size(0, height);
}

void OmniboxMatchCellView::SetTailSuggestCommonPrefixWidth(
    const std::u16string& common_prefix) {
  InvalidateLayout();
  if (common_prefix.empty()) {
    tail_suggest_common_prefix_width_ = 0;
    return;
  }
  std::unique_ptr<gfx::RenderText> render_text =
      content_view_->CreateRenderText(common_prefix);
  tail_suggest_common_prefix_width_ = render_text->GetStringSize().width();
}

BEGIN_METADATA(OmniboxMatchCellView, views::View)
END_METADATA
