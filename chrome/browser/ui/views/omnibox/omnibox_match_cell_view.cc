// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"

#include <algorithm>
#include <optional>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/common/color_parser.h"
#include "skia/ext/image_operations.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"

namespace {

// The edge length of the favicon, answer icon, and entity backgrounds.
static constexpr int kUniformRowHeightIconSize = 28;

// The gap between the left|right edge of the IPH background to the left|right
// edge of the text bounds. Does not apply to the left side of IPHs with icons,
// since the text will have to be further right to accommodate the icons.
static constexpr int kIphTextIndent = 14;

// The radius of the rounded square backgrounds of icons, answers, and entities.
static constexpr int kIconAndImageCornerRadius = 4;

// Size of weather icon with a round square background.
static constexpr int kWeatherImageSize = 24;

// Size of the weather's round square background.
static constexpr int kWeatherBackgroundSize = 28;

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
  METADATA_HEADER(RoundedCornerImageView, views::ImageView)

 public:
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

BEGIN_METADATA(RoundedCornerImageView)
END_METADATA

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxMatchCellView:

// static
void OmniboxMatchCellView::ComputeMatchMaxWidths(int contents_width,
                                                 int separator_width,
                                                 int description_width,
                                                 int iph_link_width,
                                                 int available_width,
                                                 bool allow_shrinking_contents,
                                                 int* contents_max_width,
                                                 int* description_max_width,
                                                 int* iph_link_max_width) {
  available_width = std::max(available_width, 0);

  // The IPH link is top priority.
  *iph_link_max_width = std::min(iph_link_width, available_width);
  available_width = std::max(available_width - iph_link_width, 0);

  *contents_max_width = std::min(contents_width, available_width);
  *description_max_width = std::min(description_width, available_width);

  // If the description is empty, contents can get the full available width.
  if (!description_width)
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
    // and let the contents take up the whole width. However, when action chips
    // are inlined, we don't hide the description view (in order to match the
    // behavior of the realbox).
    *description_max_width =
        std::min(description_width, available_width - *contents_max_width);
    if (*description_max_width == 0) {
      // If we're not going to display the description, the contents can have
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
  iph_link_view_ = AddChildView(std::make_unique<views::Link>(
      u"", ChromeTextContext::CONTEXT_OMNIBOX_POPUP, views::style::STYLE_LINK));
}

OmniboxMatchCellView::~OmniboxMatchCellView() = default;

// static
bool OmniboxMatchCellView::ShouldDisplayImage(const AutocompleteMatch& match) {
  return match.answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED ||
         match.type == AutocompleteMatchType::CALCULATOR ||
         !match.image_url.is_empty();
}

void OmniboxMatchCellView::OnMatchUpdate(const OmniboxResultView* result_view,
                                         const AutocompleteMatch& match) {
  if (ShouldDisplayImage(match)) {
    CHECK(AutocompleteMatch::IsSearchType(match.type));
    layout_style_ = LayoutStyle::SEARCH_SUGGESTION_WITH_IMAGE;
  } else if (AutocompleteMatch::IsSearchType(match.type)) {
    layout_style_ = LayoutStyle::SEARCH_SUGGESTION;
  } else if (match.IsIPHSuggestion()) {
    layout_style_ = LayoutStyle::IPH_SUGGESTION;
  } else {
    layout_style_ = LayoutStyle::DEFAULT_NON_SEARCH_SUGGESTION;
  }

  tail_suggest_ellipse_view_->SetVisible(
      !match.tail_suggest_common_prefix.empty());
  tail_suggest_ellipse_view_->ApplyTextColor(
      result_view->GetThemeState() == OmniboxPartState::SELECTED
          ? kColorOmniboxResultsTextSelected
          : kColorOmniboxText);

  // Set up the separator.
  separator_view_->SetSize(match.description.empty()
                               ? gfx::Size()
                               : separator_view_->GetPreferredSize());

  // Set up the IPH link following the main IPH text.
  iph_link_view_->SetText(match.iph_link_text);
  iph_link_view_->SetVisible(layout_style_ == LayoutStyle::IPH_SUGGESTION);

  // Set up the small icon.
  icon_view_->SetSize(layout_style_ == LayoutStyle::SEARCH_SUGGESTION_WITH_IMAGE
                          ? gfx::Size()
                          : icon_view_->GetPreferredSize());

  // Used for non-weather answer images (e.g. calc answers).
  const auto apply_vector_icon = [=, this](const gfx::VectorIcon& vector_icon) {
    const auto* color_provider = GetColorProvider();
    const auto& icon = gfx::CreateVectorIcon(
        vector_icon,
        color_provider->GetColor(kColorOmniboxAnswerIconGM3Foreground));
    answer_image_view_->SetImageSize(
        gfx::Size(kUniformRowHeightIconSize, kUniformRowHeightIconSize));
    answer_image_view_->SetImage(ui::ImageModel::FromImageSkia(
        gfx::ImageSkiaOperations::CreateImageWithRoundRectBackground(
            gfx::SizeF(kUniformRowHeightIconSize, kUniformRowHeightIconSize),
            kIconAndImageCornerRadius,
            color_provider->GetColor(kColorOmniboxAnswerIconGM3Background),
            icon)));
  };
  if (match.type == AutocompleteMatchType::CALCULATOR) {
    apply_vector_icon(omnibox::kAnswerCalculatorIcon);
    separator_view_->SetSize(gfx::Size());
  } else if (layout_style_ != LayoutStyle::SEARCH_SUGGESTION_WITH_IMAGE) {
    answer_image_view_->SetImage(ui::ImageModel());
    answer_image_view_->SetSize(gfx::Size());
  } else {
    // Determine if we have a local icon (or else it will be downloaded).
    if (match.answer_type == omnibox::ANSWER_TYPE_WEATHER) {
      // Weather icons are downloaded. We just need to set the correct size.
      answer_image_view_->SetImageSize(
          gfx::Size(kUniformRowHeightIconSize, kUniformRowHeightIconSize));
    } else if (match.answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED) {
      apply_vector_icon(
          AutocompleteMatch::AnswerTypeToAnswerIcon(match.answer_type));
    } else {
      SkColor color = GetColorProvider()->GetColor(
          GetOmniboxBackgroundColorId(result_view->GetThemeState()));
      content::ParseHexColorString(match.image_dominant_color, &color);
      color = SkColorSetA(color, 0x40);  // 25% transparency (arbitrary).

      gfx::Size size(kUniformRowHeightIconSize, kUniformRowHeightIconSize);
      answer_image_view_->SetImageSize(size);
      answer_image_view_->SetImage(ui::ImageModel::FromImageSkia(
          gfx::CanvasImageSource::MakeImageSkia<PlaceholderImageSource>(
              size, color)));
    }
  }
  SetTailSuggestCommonPrefixWidth(
      (match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL)
          ? match.tail_suggest_common_prefix  // Used for indent calculation.
          : std::u16string());
}

void OmniboxMatchCellView::SetIcon(const gfx::ImageSkia& image,
                                   const AutocompleteMatch& match) {
  bool is_pedal_suggestion_row = match.type == AutocompleteMatchType::PEDAL;
  bool is_journeys_suggestion_row =
      match.type == AutocompleteMatchType::HISTORY_CLUSTER;
  bool is_instant_keyword_row =
      match.type == AutocompleteMatchType::STARTER_PACK ||
      match.type == AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH;
  if (is_pedal_suggestion_row || is_journeys_suggestion_row ||
      is_instant_keyword_row) {
    // When a PEDAL suggestion has been split out to its own row, apply a square
    // background with a distinctive color to the respective icon. Journeys
    // suggestion rows should also receive the same treatment.
    const auto background_color = is_pedal_suggestion_row ||
                                          is_journeys_suggestion_row ||
                                          is_instant_keyword_row
                                      ? kColorOmniboxAnswerIconGM3Background
                                      : kColorOmniboxResultsIconGM3Background;
    icon_view_->SetImage(ui::ImageModel::FromImageSkia(
        gfx::ImageSkiaOperations::CreateImageWithRoundRectBackground(
            gfx::SizeF(kUniformRowHeightIconSize, kUniformRowHeightIconSize),
            kIconAndImageCornerRadius,
            GetColorProvider()->GetColor(background_color), image)));
  } else {
    icon_view_->SetImage(ui::ImageModel::FromImageSkia(image));
  }
}

void OmniboxMatchCellView::ClearIcon() {
  icon_view_->SetImage(ui::ImageModel());
}

void OmniboxMatchCellView::SetImage(const gfx::ImageSkia& image,
                                    const AutocompleteMatch& match) {
  // Weather icons are also sourced remotely and therefore fall into this flow.
  // Other answers don't.
  bool is_weather_answer = match.answer_type == omnibox::ANSWER_TYPE_WEATHER;

  int width = image.width();
  int height = image.height();

  // Weather icon square background should be the same color as the pop-up
  // background.
  if (is_weather_answer) {
    // Explicitly resize the weather icon to avoid pixelation.
    gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
        image, skia::ImageOperations::RESIZE_GOOD,
        gfx::Size(kWeatherImageSize, kWeatherImageSize));
    answer_image_view_->SetImage(ui::ImageModel::FromImageSkia(
        gfx::ImageSkiaOperations::CreateImageWithRoundRectBackground(
            gfx::SizeF(kWeatherBackgroundSize, kWeatherBackgroundSize),
            kIconAndImageCornerRadius,
            GetColorProvider()->GetColor(kColorOmniboxResultsBackground),
            resized_image)));
  } else {
    answer_image_view_->SetImage(ui::ImageModel::FromImageSkia(image));

    // Usually, answer images are square. But if that's not the case, setting
    // answer_image_view_ size proportional to the image size preserves
    // the aspect ratio.
    if (width == height)
      return;
    const int max = std::max(width, height);
    width = kUniformRowHeightIconSize * width / max;
    height = kUniformRowHeightIconSize * height / max;
    answer_image_view_->SetImageSize(gfx::Size(width, height));
  }
}

gfx::Insets OmniboxMatchCellView::GetInsets() const {
  const int vertical_margin = 0;
  // IPH text bounds should be centered within the IPH background when there's
  // no IPH icon. So make their `right_margin` equal to their text's x position.
  const int right_margin =
      layout_style_ == LayoutStyle::IPH_SUGGESTION
          ? OmniboxMatchCellView::kMarginLeft + kIphTextIndent
          : 7;
  return gfx::Insets::TLBR(vertical_margin, OmniboxMatchCellView::kMarginLeft,
                           vertical_margin, right_margin);
}

void OmniboxMatchCellView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  const gfx::Rect child_area = GetContentsBounds();
  int x = child_area.x();
  int y = child_area.y();

  const int row_height = child_area.height();

  int image_x = GetImageIndent();
  views::ImageView* const image_view =
      layout_style_ == LayoutStyle::SEARCH_SUGGESTION_WITH_IMAGE
          ? answer_image_view_.get()
          : icon_view_.get();
  image_view->SetBounds(image_x, y, kImageBoundsWidth, row_height);

  const int text_indent = GetTextIndent() + tail_suggest_common_prefix_width_;
  x += text_indent;
  const int text_width = child_area.width() - text_indent;

  int content_width = content_view_->GetPreferredSize().width();
  int description_width = description_view_->GetPreferredSize().width();
  const gfx::Size separator_size = separator_view_->GetPreferredSize();
  int iph_link_width = iph_link_view_->GetPreferredSize().width();
  ComputeMatchMaxWidths(
      content_width, separator_size.width(), description_width, iph_link_width,
      /*available_width=*/text_width,
      /*allow_shrinking_contents=*/
      layout_style_ != LayoutStyle::SEARCH_SUGGESTION &&
          layout_style_ != LayoutStyle::SEARCH_SUGGESTION_WITH_IMAGE,
      &content_width, &description_width, &iph_link_width);
  if (tail_suggest_ellipse_view_->GetVisible()) {
    const int tail_suggest_ellipse_width =
        tail_suggest_ellipse_view_->GetPreferredSize().width();
    tail_suggest_ellipse_view_->SetBounds(x - tail_suggest_ellipse_width, y,
                                          tail_suggest_ellipse_width,
                                          row_height);
  }
  content_view_->SetBounds(x, y, content_width, row_height);
  x += content_view_->width();
  if (description_width) {
    separator_view_->SetSize(separator_size);
    separator_view_->SetBounds(x, y, separator_view_->width(), row_height);
    x += separator_view_->width();
    description_view_->SetBounds(x, y, description_width, row_height);
    x += description_view_->width();
  } else {
    separator_view_->SetSize(gfx::Size());
    description_view_->SetSize(gfx::Size());
  }
  iph_link_view_->SetBounds(x, y, iph_link_width, row_height);
}

gfx::Size OmniboxMatchCellView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int kMargin = 6;
  int height = kUniformRowHeightIconSize + 2 * kMargin;
  if (layout_style_ == LayoutStyle::IPH_SUGGESTION) {
    height += 4;
  }

  int width = GetInsets().width() + GetTextIndent() +
              tail_suggest_common_prefix_width_ +
              content_view_->GetPreferredSize().width();

  const int description_width = description_view_->GetPreferredSize().width();
  if (description_width > 0) {
    width += separator_view_->GetPreferredSize().width() + description_width;
  }

  width += iph_link_view_->GetPreferredSize().width();

  return gfx::Size(width, height);
}

int OmniboxMatchCellView::GetImageIndent() const {
  // Image indent ignores the `OmniboxMatchCellView::GetInsets()`.

  // This number is independent of other layout numbers; i.e., it's not meant to
  // align with any other UI; it's just arbitrarily chosen by UX. Hence, it's
  // not derived from other matches' `indent` below.
  if (layout_style_ == LayoutStyle::IPH_SUGGESTION)
    return 2;

  // The entity, answer, and icon images are horizontally centered within their
  // bounds. So their center-line will be at `image_x+kImageBoundsWidth/2`. This
  // means their left x coordinate will depend on their actual sizes. This code
  // guarantees:
  // a) Entities' left x coordinate is 16.
  // b) Entities, answers, and icons continue to be center-aligned.
  // c) Regardless of the state of those other features and their widths.
  // This applies to both touch-UI and non-touch-UI.
  int indent = 16 + kUniformRowHeightIconSize / 2 - kImageBoundsWidth / 2;

  return indent;
}

int OmniboxMatchCellView::GetTextIndent() const {
  // Text indent is added to the `OmniboxMatchCellView::GetInsets()`. It is not
  // added to the image position & size.

  // Some IPH matches have no icons. They should be moved further left so the
  // gap between the IPH background and the start of the IPH text isn't jarring.
  // Non-IPH matches without icons (e.g. the 'no results found' tab match) don't
  // want to apply this left shift because their text needs to align with the
  // other matches' and the omnibox's texts. This number is independent of other
  // layout numbers; i.e., it's not meant to align with other UI; it's just
  // arbitrarily chosen by UX. Hence, it's not derived from other matches'
  // `indent` below.
  if (layout_style_ == LayoutStyle::IPH_SUGGESTION &&
      icon_view_->GetPreferredSize() == gfx::Size{}) {
    return kIphTextIndent;
  }

  // For normal matches, the gap between the left edge of this view and the
  // left edge of its favicon or answer image.
  int indent = 52;

  // The IPH row left inset is +`kIphOffset` from other suggestions, so the text
  // indent should be -`kIphOffset` to keep the text aligned. IPH matches seem
  // to have inner padding, so the gap between the left edge of this
  // `OmniboxMatchCellView` and the IPH icon/text is actually larger than
  // `indent`.
  if (layout_style_ == LayoutStyle::IPH_SUGGESTION)
    indent -= kIphOffset;

  return indent;
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

BEGIN_METADATA(OmniboxMatchCellView)
END_METADATA
