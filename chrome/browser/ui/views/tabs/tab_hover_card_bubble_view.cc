// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <ios>
#include <limits>
#include <memory>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/cxx17_backports.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/selection_model.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Maximum number of lines that a title label occupies.
constexpr int kHoverCardTitleMaxLines = 2;

constexpr int kHorizontalMargin = 18;
constexpr int kVerticalMargin = 10;
constexpr int kFootnoteVerticalMargin = 8;
constexpr auto kTitleMargins =
    gfx::Insets::VH(kVerticalMargin, kHorizontalMargin);
constexpr auto kAlertMargins =
    gfx::Insets::VH(kFootnoteVerticalMargin, kHorizontalMargin);

std::unique_ptr<views::Label> CreateAlertView(const TabAlertState& state) {
  auto alert_state_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  alert_state_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  alert_state_label->SetMultiLine(true);
  alert_state_label->SetVisible(true);
  alert_state_label->SetText(chrome::GetTabAlertStateText(state));
  return alert_state_label;
}

// Calculates an appropriate size to display a preview image in the hover card.
// For the vast majority of images, the |preferred_size| is used, but extremely
// tall or wide images use the image size instead, centering in the available
// space.
gfx::Size GetPreviewImageSize(gfx::Size preview_size,
                              gfx::Size preferred_size) {
  DCHECK(!preferred_size.IsEmpty());
  if (preview_size.IsEmpty())
    return preview_size;
  const float preview_aspect_ratio =
      static_cast<float>(preview_size.width()) / preview_size.height();
  const float preferred_aspect_ratio =
      static_cast<float>(preferred_size.width()) / preferred_size.height();
  const float ratio = preview_aspect_ratio / preferred_aspect_ratio;
  // Images between 2/3 and 3/2 of the target aspect ratio use the preferred
  // size, stretching the image. Only images outside this range get centered.
  // Since this is a corner case most users will never see, the specific cutoffs
  // just need to be reasonable and don't need to be precise values (that is,
  // there is no "correct" value; if the results are not aesthetic they can be
  // tuned).
  constexpr float kMinStretchRatio = 0.667f;
  constexpr float kMaxStretchRatio = 1.5f;
  if (ratio >= kMinStretchRatio && ratio <= kMaxStretchRatio)
    return preferred_size;
  return preview_size;
}

bool UseAlternateHoverCardFormat() {
  static const int use_alternate_format =
      base::GetFieldTrialParamByFeatureAsInt(
          features::kTabHoverCardImages, features::kTabHoverCardAlternateFormat,
          0);
  return use_alternate_format != 0;
}

// Label that renders its background in a solid color. Placed in front of a
// normal label either by being later in the draw order or on a layer, it can
// be used to animate a fade-out.
class SolidLabel : public views::Label {
 public:
  METADATA_HEADER(SolidLabel);
  using Label::Label;
  SolidLabel() = default;
  ~SolidLabel() override = default;

 protected:
  // views::Label:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    canvas->DrawColor(GetBackgroundColor());
  }
};

BEGIN_METADATA(SolidLabel, views::Label)
END_METADATA

// Label that exposes the CreateRenderText() method, so that we can use
// TabHoverCardBubbleView::FilenameElider to do a two-line elision of
// filenames.
class RenderTextFactoryLabel : public views::Label {
 public:
  using Label::CreateRenderText;
  using Label::Label;
};

}  // namespace

// TabHoverCardBubbleView::FilenameElider:
// ----------------------------------------------------------

TabHoverCardBubbleView::FilenameElider::FilenameElider(
    std::unique_ptr<gfx::RenderText> render_text)
    : render_text_(std::move(render_text)) {}

TabHoverCardBubbleView::FilenameElider::~FilenameElider() = default;

std::u16string TabHoverCardBubbleView::FilenameElider::Elide(
    const std::u16string& text,
    const gfx::Rect& display_rect) const {
  render_text_->SetText(text);
  return ElideImpl(GetLineLengths(display_rect));
}

// static
std::u16string::size_type
TabHoverCardBubbleView::FilenameElider::FindImageDimensions(
    const std::u16string& text) {
  // We don't have regexes in Chrome, but we can still do a rough evaluation of
  // the line to see if it ends with the expected pattern:
  //
  // title[ (width×height)]
  //
  // We'll look for the open parenthesis, then the rest of the size. Note that
  // we don't have to worry about graphemes or combining characters because any
  // character that's not of the expected type means there is no dimension.

  // Find the start of the extension.
  const auto paren_pos = text.find_last_of(u'(');
  if (paren_pos == 0 || paren_pos == std::u16string::npos ||
      text[paren_pos - 1] != u' ') {
    return std::u16string::npos;
  }

  // Fast forward to the unicode character following the paren.
  base::i18n::UTF16CharIterator it(
      base::StringPiece16(text).substr(paren_pos + 1));

  // Look for the image width.
  if (!std::isdigit(it.get()))
    return std::u16string::npos;
  while (it.Advance() && std::isdigit(it.get())) {
    // empty loop
  }

  // Look for the × character and the height.
  constexpr char16_t kMultiplicationSymbol = u'\u00D7';
  if (it.end() || it.get() != kMultiplicationSymbol || !it.Advance() ||
      !std::isdigit(it.get())) {
    return std::u16string::npos;
  }
  while (it.Advance() && std::isdigit(it.get())) {
    // empty loop
  }

  // Look for the closing parenthesis and make sure we've hit the end of the
  // string.
  if (it.end() || it.get() != u')')
    return std::u16string::npos;
  it.Advance();
  return it.end() ? paren_pos : std::u16string::npos;
}

TabHoverCardBubbleView::FilenameElider::LineLengths
TabHoverCardBubbleView::FilenameElider::GetLineLengths(
    const gfx::Rect& display_rect) const {
  const std::u16string text = render_text_->text();
  render_text_->SetMaxLines(0);
  render_text_->SetMultiline(false);
  render_text_->SetWhitespaceElision(true);
  render_text_->SetDisplayRect(display_rect);

  // Set our temporary RenderText to the unelided text and elide the start of
  // the string to give us a guess at where the second line of the label
  // should start.
  render_text_->SetElideBehavior(gfx::ElideBehavior::ELIDE_HEAD);
  const std::u16string tentative_second_line = render_text_->GetDisplayText();

  // If there is no elision, then the text will fit on a single line and
  // there's nothing to do.
  if (tentative_second_line == text)
    return LineLengths(text.length(), text.length());

  // If there's not enough space to display even a single character, there is
  // also nothing to do; the result needs to be empty.
  if (tentative_second_line.empty())
    return LineLengths(0, 0);

  LineLengths result;

  // Since we truncated, expect the string to start with ellipsis, then
  // calculate the length of the string sans ellipsis.
  DCHECK_EQ(gfx::kEllipsisUTF16[0], tentative_second_line[0]);

  // TODO(crbug.com/1239317): Elision is still a little flaky, so we'll make
  // sure we didn't stop in the middle of a grapheme. The +1 is to move past
  // the ellipsis which is not part of the original string.
  size_t pos = text.length() - tentative_second_line.length() + 1;
  if (!render_text_->IsGraphemeBoundary(pos))
    pos = render_text_->IndexOfAdjacentGrapheme(pos, gfx::CURSOR_FORWARD);
  result.second = text.length() - pos;

  // Calculate the first line by aggressively truncating the text. This may
  // cut the string somewhere other than a word boundary, but for very long
  // filenames, it's probably best to fit as much of the name on the card as
  // possible, even if we sacrifice a small amount of readability.
  render_text_->SetElideBehavior(gfx::ElideBehavior::TRUNCATE);
  result.first = render_text_->GetDisplayText().length();

  // TOOD(crbug.com/1239317) Handle the case where we ended up in the middle
  // of a grapheme.
  if (!render_text_->IsGraphemeBoundary(result.first)) {
    result.first = render_text_->IndexOfAdjacentGrapheme(result.first,
                                                         gfx::CURSOR_BACKWARD);
  }

  return result;
}

std::u16string TabHoverCardBubbleView::FilenameElider::ElideImpl(
    TabHoverCardBubbleView::FilenameElider::LineLengths line_lengths) const {
  const std::u16string& text = render_text_->text();

  // Validate the inputs. All of these are base assumptions.
  DCHECK_LE(line_lengths.first, text.length());
  DCHECK_LE(line_lengths.second, text.length());
  DCHECK(render_text_->IsGraphemeBoundary(line_lengths.first));
  DCHECK(render_text_->IsGraphemeBoundary(text.length() - line_lengths.second));

  // If the entire text fits on a single line, use it as-is.
  if (line_lengths.first == text.length() ||
      line_lengths.second == text.length()) {
    return text;
  }

  // If no characters will fit on one of the lines, return an empty string.
  if (line_lengths.first == 0 || line_lengths.second == 0)
    return std::u16string();

  // Let's figure out where to actually start the second line. Strings that
  // are too long for one line but fit on two lines tend to create some
  // overlap between the first and second line, so take the maximum of the
  // second line cut and the end of the first line.
  const size_t second_line_cut = text.length() - line_lengths.second;
  size_t cut_point = std::max(second_line_cut, line_lengths.first);

  // We got the whole line if the cut point is the character immediately
  // after the first line cuts off (otherwise we've truncated and need to
  // show an ellipsis in the final string).
  const bool is_whole_string = (cut_point == line_lengths.first);

  // If there is some flexibility in where we make our cut point (that is, the
  // potential first and second lines overlap), there are a few specific places
  // we preferentially want to separate the lines.
  bool adjusted_cut_point = false;
  if (is_whole_string && cut_point >= second_line_cut) {
    // First, if there are image dimensions, preferentially put those on the
    // second line.
    const auto paren_pos = FindImageDimensions(text);
    if (paren_pos != std::u16string::npos && paren_pos >= second_line_cut &&
        paren_pos <= cut_point) {
      cut_point = paren_pos;
      adjusted_cut_point = true;
    }

    // Second, we can break at the start of the file extension.
    if (!adjusted_cut_point) {
      const size_t dot_pos = text.find_last_of(u'.');
      if (dot_pos != std::u16string::npos && dot_pos >= second_line_cut &&
          dot_pos <= cut_point) {
        cut_point = dot_pos;
        adjusted_cut_point = true;
      }
    }
  }

  // TODO(dfried): possibly handle the case where we chop a section with bidi
  // delimiters out or split it between lines.

  // If we didn't put the extension on its own line, eliminate whitespace
  // from the start of the second line (it looks weird).
  if (!adjusted_cut_point) {
    cut_point =
        gfx::FindValidBoundaryAfter(text, cut_point, /*trim_whitespace =*/true);
  }

  // Reassemble the string. Start with the first line up to `cut_point` or the
  // end of the line, whichever comes sooner.
  std::u16string result =
      text.substr(0, std::min(line_lengths.first, cut_point));
  result.push_back(u'\n');

  // If we're starting the second line with a file extension hint that the
  // directionality of the text might change by using an FSI mark. Allowing
  // the renderer to re-infer RTL-ness produces much better results in text
  // rendering when an RTL filename has an ASCII extension.
  //
  // TODO(dfried): Currently we do put an FSI before an ellipsis; this
  // results in the ellipsis being placed with the text that immediately
  // follows it (making the point of elision more obvious). If the text
  // following the cut is LTR it goes on the left, and if the text is RTL it
  // goes on the right. Reconsider if/how we should set text direction
  // following an ellipsis:
  // - No FSI would cause the ellipsis to align with the preceding rather
  //   than the following text. It would provide a bit more visual continuity
  //   between lines, but might be confusing as to where the text picks back
  //   up (as the next character might be on the opposite side of the line).
  // - We could preserve elided directionality markers, but they could end up
  //   aligning the ellipsis with text that is not present at all on the
  //   label.
  // - We could also force direction to match the start of the first line for
  //   consistency but that could result in an ellipsis that matches neither
  //   the preceding nor following text.
  //
  // TODO(dfried): move these declarations to rtl.h alongside e.g.
  // base::i18n::kRightToLeftMark
  constexpr char16_t kFirstStrongIsolateMark = u'\u2068';
  constexpr char16_t kPopDirectionalIsolateMark = u'\u2069';
  if (adjusted_cut_point || !is_whole_string)
    result += kFirstStrongIsolateMark;
  if (!is_whole_string)
    result.push_back(gfx::kEllipsisUTF16[0]);
  result.append(text.substr(cut_point));
  // If we added an FSI, we should bracket it with a PDI.
  if (adjusted_cut_point || !is_whole_string)
    result += kPopDirectionalIsolateMark;
  return result;
}

// TabHoverCardBubbleView::FadeLabel:
// ----------------------------------------------------------

// This view overlays and fades out an old version of the text of a label,
// while displaying the new text underneath. It is used to fade out the old
// value of the title and domain labels on the hover card when the tab switches
// or the tab title changes.
class TabHoverCardBubbleView::FadeLabel : public views::View {
 public:
  FadeLabel(int context, int num_lines) {
    primary_label_ = AddChildView(std::make_unique<RenderTextFactoryLabel>(
        std::u16string(), context, views::style::STYLE_PRIMARY));
    primary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    primary_label_->SetVerticalAlignment(gfx::ALIGN_TOP);
    primary_label_->SetMultiLine(num_lines > 1);
    if (num_lines > 1)
      primary_label_->SetMaxLines(num_lines);

    label_fading_out_ = AddChildView(std::make_unique<SolidLabel>(
        std::u16string(), context, views::style::STYLE_PRIMARY));
    label_fading_out_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_fading_out_->SetVerticalAlignment(gfx::ALIGN_TOP);
    label_fading_out_->SetMultiLine(num_lines > 1);
    if (num_lines > 1)
      label_fading_out_->SetMaxLines(num_lines);
    label_fading_out_->GetViewAccessibility().OverrideIsIgnored(true);

    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  ~FadeLabel() override = default;

  void SetText(std::u16string text, absl::optional<bool> is_filename) {
    if (was_filename_.has_value())
      SetMultilineParams(label_fading_out_, was_filename_.value());
    label_fading_out_->SetText(primary_label_->GetText());
    if (is_filename.has_value())
      SetMultilineParams(primary_label_, is_filename.value());
    was_filename_ = is_filename;
    primary_label_->SetText(text);
  }

  // Sets the fade-out of the label as |percent| in the range [0, 1]. Since
  // FadeLabel is designed to mask new text with the old and then fade away, the
  // higher the percentage the less opaque the label.
  void SetFade(double percent) {
    percent_ = std::min(1.0, percent);
    if (percent_ == 1.0)
      label_fading_out_->SetText(std::u16string());
    const SkAlpha alpha = base::saturated_cast<SkAlpha>(
        std::numeric_limits<SkAlpha>::max() * (1.0 - percent_));
    label_fading_out_->SetBackgroundColor(
        SkColorSetA(label_fading_out_->GetBackgroundColor(), alpha));
    label_fading_out_->SetEnabledColor(
        SkColorSetA(label_fading_out_->GetEnabledColor(), alpha));
  }

  std::u16string GetText() const { return primary_label_->GetText(); }

  // Returns a version of the text that's middle-elided on two lines.
  std::u16string TruncateFilenameToTwoLines(const std::u16string& text) const {
    FilenameElider elider(primary_label_->CreateRenderText());
    gfx::Rect text_rect = primary_label_->GetContentsBounds();
    text_rect.Inset(-gfx::ShadowValue::GetMargin(primary_label_->GetShadows()));
    return elider.Elide(text, text_rect);
  }

 protected:
  // views::View:
  gfx::Size GetMaximumSize() const override {
    return gfx::Tween::SizeValueBetween(percent_,
                                        label_fading_out_->GetPreferredSize(),
                                        primary_label_->GetPreferredSize());
  }

  gfx::Size CalculatePreferredSize() const override {
    return primary_label_->GetPreferredSize();
  }

  gfx::Size GetMinimumSize() const override {
    return primary_label_->GetMinimumSize();
  }

  int GetHeightForWidth(int width) const override {
    return primary_label_->GetHeightForWidth(width);
  }

 private:
  static void SetMultilineParams(views::Label* label, bool is_filename) {
    label->SetElideBehavior(is_filename ? gfx::NO_ELIDE : gfx::ELIDE_TAIL);
  }

  raw_ptr<RenderTextFactoryLabel> primary_label_;
  raw_ptr<SolidLabel> label_fading_out_;
  absl::optional<bool> was_filename_;
  double percent_ = 1.0;
};

// TabHoverCardBubbleView::ThumbnailView:
// ----------------------------------------------------------

// Represents the preview image on the hover card. Allows for a new image to be
// faded in over the old image.
class TabHoverCardBubbleView::ThumbnailView
    : public views::View,
      public views::AnimationDelegateViews {
 public:
  // Specifies which (if any) of the corners of the preview image will be
  // rounded. See SetRoundedCorners() below for more information.
  enum class RoundedCorners { kNone, kTopCorners, kBottomCorners };

  explicit ThumbnailView(TabHoverCardBubbleView* bubble_view)
      : AnimationDelegateViews(this),
        bubble_view_(bubble_view),
        image_transition_animation_(this) {
    constexpr base::TimeDelta kImageTransitionDuration =
        kHoverCardSlideDuration;
    image_transition_animation_.SetDuration(kImageTransitionDuration);

    // Set a reasonable preview size so that ThumbnailView() is given an
    // appropriate amount of space in the layout.
    target_tab_image_ = AddChildView(CreateImageView());
    image_fading_out_ = AddChildView(CreateImageView());
    image_fading_out_->SetPaintToLayer();
    image_fading_out_->layer()->SetOpacity(0.0f);

    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  // Sets the appropriate rounded corners for the preview image, for platforms
  // where layers must be explicitly clipped (because they are not clipped by
  // the widget). Set `rounded_corners` to kTopCorners if the preview image is
  // the topmost view in the widget (including header); set kBottomCorners if
  // the preview is the bottom-most view (including footer). If neither, use
  // kNone.
  void SetRoundedCorners(RoundedCorners rounded_corners, float radius) {
    gfx::RoundedCornersF corners;
    switch (rounded_corners) {
      case RoundedCorners::kNone:
        corners = {0, 0, 0, 0};
        break;
      case RoundedCorners::kTopCorners:
        corners = {radius, radius, 0, 0};
        break;
      case RoundedCorners::kBottomCorners:
        corners = {0, 0, radius, radius};
        break;
    }
    image_fading_out_->layer()->SetRoundedCornerRadius(corners);
  }

  // Sets the new preview image. The old image will be faded out.
  void SetTargetTabImage(gfx::ImageSkia preview_image) {
    StartFadeOut();
    SetImage(target_tab_image_, preview_image, ImageType::kThumbnail);
    image_type_ = ImageType::kThumbnail;
  }

  // Clears the preview image and replaces it with a placeholder image. The old
  // image will be faded out.
  void SetPlaceholderImage() {
    if (image_type_ == ImageType::kPlaceholder)
      return;

    // Color provider may be null if there is no associated widget. In that case
    // there is nothing to render, and we can't get default colors to render
    // with anyway, so bail out.
    const auto* const color_provider = GetColorProvider();
    if (!color_provider)
      return;

    StartFadeOut();

    // Check the no-preview color and size to see if it needs to be
    // regenerated. DPI or theme change can cause a regeneration.
    const SkColor foreground_color =
        color_provider->GetColor(kColorTabHoverCardForeground);

    // Set the no-preview placeholder image. All sizes are in DIPs.
    // gfx::CreateVectorIcon() caches its result so there's no need to store
    // images here; if a particular size/color combination has already been
    // requested it will be low-cost to request it again.
    constexpr gfx::Size kNoPreviewImageSize{64, 64};
    const gfx::ImageSkia no_preview_image = gfx::CreateVectorIcon(
        kGlobeIcon, kNoPreviewImageSize.width(), foreground_color);
    SetImage(target_tab_image_, no_preview_image, ImageType::kPlaceholder);
    image_type_ = ImageType::kPlaceholder;
  }

  void ClearImage() {
    if (image_type_ == ImageType::kNone)
      return;

    StartFadeOut();
    SetImage(target_tab_image_, gfx::ImageSkia(), ImageType::kNone);
    image_type_ = ImageType::kNone;
  }

  void SetWaitingForImage() {
    if (image_type_ == ImageType::kNone) {
      image_type_ = ImageType::kNoneButWaiting;
      InvalidateLayout();
    }
  }

 private:
  enum class ImageType { kNone, kNoneButWaiting, kPlaceholder, kThumbnail };

  // Creates an image view with the appropriate default properties.
  static std::unique_ptr<views::ImageView> CreateImageView() {
    auto image_view = std::make_unique<views::ImageView>();
    image_view->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
    return image_view;
  }

  // Sets `image` on `image_view_`, configuring the image appropriately based
  // on whether it's a placeholder or not.
  void SetImage(views::ImageView* image_view,
                gfx::ImageSkia image,
                ImageType image_type) {
    image_view->SetImage(image);
    switch (image_type) {
      case ImageType::kNone:
      case ImageType::kNoneButWaiting:
        image_view->SetBackground(
            views::CreateSolidBackground(bubble_view_->color()));
        break;
      case ImageType::kPlaceholder:
        image_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
        image_view->SetImageSize(image.size());
        image_view->SetBackground(views::CreateSolidBackground(
            image_view->GetColorProvider()->GetColor(
                kColorTabHoverCardBackground)));
        break;
      case ImageType::kThumbnail:
        image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
        image_view->SetImageSize(
            GetPreviewImageSize(image.size(), TabStyle::GetPreviewImageSize()));
        image_view->SetBackground(nullptr);
        break;
    }
  }

  // views::View:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  gfx::Size CalculatePreferredSize() const override {
    return image_type_ == ImageType::kNone ? gfx::Size()
                                           : TabStyle::GetPreviewImageSize();
  }

  gfx::Size GetMaximumSize() const override {
    return TabStyle::GetPreviewImageSize();
  }

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    image_fading_out_->layer()->SetOpacity(1.0 - animation->GetCurrentValue());
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    image_fading_out_->layer()->SetOpacity(0.0f);
    SetImage(image_fading_out_, gfx::ImageSkia(), ImageType::kNone);
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  // Begins fading out the existing image, which it reads from
  // `target_tab_image_`. Does a smart three-way crossfade if an image is
  // already fading out.
  void StartFadeOut() {
    // If we aren't visible, don't have a widget, or our widget is being
    // destructed and has no theme provider, skip trying to fade out since a
    // ColorProvider is needed for fading out placeholder images. (Note that
    // GetColorProvider() returns nullptr if there is no widget.)
    // See: crbug.com/1246914
    if (!GetVisible() || !GetColorProvider())
      return;

    if (!GetPreviewImageCrossfadeStart().has_value())
      return;

    gfx::ImageSkia old_image = target_tab_image_->GetImage();

    if (image_transition_animation_.is_animating()) {
      // If we're already animating and we've barely faded out the previous old
      // image, keep fading out the old one and just swap the new one
      // underneath.
      const double current_value =
          image_transition_animation_.GetCurrentValue();
      if (current_value <= 0.5)
        return;

      // Currently we have:
      //  - old preview at `current_value` opacity
      //  - previous old preview at 1.0 - `current_value` opacity
      // We will discard the previous old preview and move the old preview into
      // the occluding view. However, since the opacity of this view is
      // inversely proportional to animation progress, to keep the same opacity
      // (while we swap the new image in behind) we have to rewind the
      // animation.
      image_transition_animation_.SetCurrentValue(1.0 - current_value);
      SetImage(image_fading_out_, old_image, image_type_);
      AnimationProgressed(&image_transition_animation_);

    } else {
      SetImage(image_fading_out_, old_image, image_type_);
      image_fading_out_->layer()->SetOpacity(1.0f);
      image_transition_animation_.Start();
    }
  }

  const raw_ptr<TabHoverCardBubbleView> bubble_view_;

  // Displays the image that we are trying to display for the target/current
  // tab. Placed under `image_fading_out_` so that it is revealed as the
  // previous image fades out.
  raw_ptr<views::ImageView> target_tab_image_ = nullptr;

  // Displays the previous image as it's fading out. Rendered over
  // `target_tab_image_` and has its alpha animated from 1 to 0.
  raw_ptr<views::ImageView> image_fading_out_ = nullptr;

  // Provides a smooth fade out for `image_fading_out_`. We do not use a
  // LayerAnimation because we need to rewind the transparency at various
  // times (it's not necessarily a single smooth animation).
  gfx::LinearAnimation image_transition_animation_;

  // Records what type of image `target_tab_image_` is showing. Used to
  // configure `image_fading_out_` when the target image becomes the previous
  // image and fades out.
  ImageType image_type_ = ImageType::kNone;
};

// TabHoverCardBubbleView:
// ----------------------------------------------------------

// static
constexpr base::TimeDelta TabHoverCardBubbleView::kHoverCardSlideDuration;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabHoverCardBubbleView,
                                      kHoverCardBubbleElementId);

TabHoverCardBubbleView::TabHoverCardBubbleView(Tab* tab)
    : BubbleDialogDelegateView(tab,
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::STANDARD_SHADOW) {
  SetButtons(ui::DIALOG_BUTTON_NONE);

  // Remove the accessible role so that hover cards are not read when they
  // appear because tabs handle accessibility text.
  SetAccessibleWindowRole(ax::mojom::Role::kNone);

  // We'll do all of our own layout inside the bubble, so no need to inset this
  // view inside the client view.
  set_margins(gfx::Insets());

  // Set so that when hovering over a tab in a inactive window that window will
  // not become active. Setting this to false creates the need to explicitly
  // hide the hovercard on press, touch, and keyboard events.
  SetCanActivate(false);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  set_accept_events(false);
#endif

  // Set so that the tab hover card is not focus traversable when keyboard
  // navigating through the tab strip.
  set_focus_traversable_from_anchor_view(false);

  title_label_ = AddChildView(std::make_unique<FadeLabel>(
      CONTEXT_TAB_HOVER_CARD_TITLE, kHoverCardTitleMaxLines));
  domain_label_ = AddChildView(
      std::make_unique<FadeLabel>(views::style::CONTEXT_DIALOG_BODY_TEXT, 1));

  if (TabHoverCardController::AreHoverCardImagesEnabled()) {
    if (UseAlternateHoverCardFormat()) {
      thumbnail_view_ =
          AddChildViewAt(std::make_unique<ThumbnailView>(this), 0);
      thumbnail_view_->SetRoundedCorners(
          ThumbnailView::RoundedCorners::kTopCorners, corner_radius_);
    } else {
      thumbnail_view_ = AddChildView(std::make_unique<ThumbnailView>(this));
      thumbnail_view_->SetRoundedCorners(
          ThumbnailView::RoundedCorners::kBottomCorners, corner_radius_);
    }
  }

  // Set up layout.

  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout->SetCollapseMargins(true);

  // In some browser types (e.g. ChromeOS terminal app) we hide the domain
  // label. In those cases, we need to adjust the bottom margin of the title
  // element because it is no longer above another text element and needs a
  // bottom margin.
  const bool show_domain = tab->controller()->ShowDomainInHoverCards();
  gfx::Insets title_margins = kTitleMargins;
  domain_label_->SetVisible(show_domain);
  if (show_domain) {
    title_margins.set_bottom(0);
    domain_label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, kHorizontalMargin, kVerticalMargin,
                          kHorizontalMargin));
  }

  title_label_->SetProperty(views::kMarginsKey, title_margins);
  title_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kScaleToMaximum)
          .WithOrder(2));
  if (thumbnail_view_) {
    thumbnail_view_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kScaleToMaximum)
            .WithOrder(1));
  }

  // Set up widget.

  views::BubbleDialogDelegateView::CreateBubble(this);
  set_adjust_if_offscreen(true);

  GetBubbleFrameView()->SetFootnoteMargins(kAlertMargins);
  GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  GetBubbleFrameView()->set_hit_test_transparent(true);

  GetBubbleFrameView()->SetCornerRadius(corner_radius_);

  // Placeholder image should be used when there is no image data for the
  // given tab. Otherwise don't flash the placeholder while we wait for the
  // existing thumbnail to be decompressed.
  //
  // Note that this code has to go after CreateBubble() above, since setting up
  // the placeholder image and background color require a ColorProvider, which
  // is only available once this View has been added to its widget.
  if (thumbnail_view_ &&
      (!tab->data().thumbnail || !tab->data().thumbnail->has_data()) &&
      !tab->IsActive()) {
    thumbnail_view_->SetPlaceholderImage();
  }

  // Start in the fully "faded-in" position so that whatever text we initially
  // display is visible.
  SetTextFade(1.0);

  SetProperty(views::kElementIdentifierKey, kHoverCardBubbleElementId);
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

void TabHoverCardBubbleView::UpdateCardContent(const Tab* tab) {
  // Preview image is never visible for the active tab.
  if (thumbnail_view_) {
    if (tab->IsActive())
      thumbnail_view_->ClearImage();
    else
      thumbnail_view_->SetWaitingForImage();
  }

  std::u16string title;
  absl::optional<TabAlertState> old_alert_state = alert_state_;
  GURL domain_url;
  // Use committed URL to determine if no page has yet loaded, since the title
  // can be blank for some web pages.
  if (!tab->data().last_committed_url.is_valid()) {
    domain_url = tab->data().visible_url;
    title = tab->data().IsCrashed()
                ? l10n_util::GetStringUTF16(IDS_HOVER_CARD_CRASHED_TITLE)
                : l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
    alert_state_ = absl::nullopt;
  } else {
    domain_url = tab->data().last_committed_url;
    title = tab->data().title;
    alert_state_ = Tab::GetAlertStateToShow(tab->data().alert_state);
  }
  std::u16string domain;
  bool is_filename = false;
  if (domain_url.SchemeIsFile()) {
    is_filename = true;
    title = title_label_->TruncateFilenameToTwoLines(title);
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_FILE_URL_SOURCE);
  } else {
    if (domain_url.SchemeIsBlob()) {
      domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_BLOB_URL_SOURCE);
    } else {
      if (tab->data().should_display_url) {
        // Hide the domain when necessary. This leaves an empty space in the
        // card, but this scenario is very rare. Also, shrinking the card to
        // remove the space would result in visual noise, so we keep it simple.
        domain = url_formatter::FormatUrl(
            domain_url,
            url_formatter::kFormatUrlOmitDefaults |
                url_formatter::kFormatUrlOmitHTTPS |
                url_formatter::kFormatUrlOmitTrivialSubdomains |
                url_formatter::kFormatUrlTrimAfterHost,
            base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
      }

      // Most of the time we want our standard (tail-elided) formatting for web
      // pages, but when viewing an image in the browser, many users want to
      // view the image dimensions (see crbug.com/1222984) so for titles that
      // "look" like images (i.e. that end with a dimension) we instead switch
      // to middle-elide.
      if (FilenameElider::FindImageDimensions(title) != std::u16string::npos) {
        is_filename = true;
        title = title_label_->TruncateFilenameToTwoLines(title);
      }
    }
  }
  title_label_->SetText(title, is_filename);
  domain_label_->SetText(domain, absl::nullopt);

  const bool alternate_layout = UseAlternateHoverCardFormat();
  if (alert_state_ != old_alert_state) {
    std::unique_ptr<views::Label> alert_label =
        alert_state_.has_value() ? CreateAlertView(*alert_state_) : nullptr;
    if (alternate_layout) {
      if (alert_label) {
        // Simulate the same look as the footnote view.
        // TODO(dfried): should we add this as a variation of
        // FootnoteContainerView? Currently it's only used here.
        const auto* color_provider = GetColorProvider();
        alert_label->SetBackground(views::CreateSolidBackground(
            color_provider->GetColor(ui::kColorBubbleFooterBackground)));
        alert_label->SetBorder(views::CreatePaddedBorder(
            views::CreateSolidSidedBorder(
                gfx::Insets::TLBR(0, 0, 1, 0),
                color_provider->GetColor(ui::kColorBubbleFooterBorder)),
            kAlertMargins));
      }
      GetBubbleFrameView()->SetHeaderView(std::move(alert_label));
    } else {
      GetBubbleFrameView()->SetFootnoteView(std::move(alert_label));
    }

    if (thumbnail_view_) {
      // We only clip the corners of the fade image when there isn't a header
      // or footer.
      ThumbnailView::RoundedCorners corners =
          ThumbnailView::RoundedCorners::kNone;
      if (!alert_state_.has_value()) {
        corners = alternate_layout
                      ? ThumbnailView::RoundedCorners::kTopCorners
                      : ThumbnailView::RoundedCorners::kBottomCorners;
      }
      thumbnail_view_->SetRoundedCorners(corners, corner_radius_);
    }
  }
}

void TabHoverCardBubbleView::SetTextFade(double percent) {
  title_label_->SetFade(percent);
  domain_label_->SetFade(percent);
}

void TabHoverCardBubbleView::SetTargetTabImage(gfx::ImageSkia preview_image) {
  DCHECK(thumbnail_view_)
      << "This method should only be called when preview images are enabled.";
  thumbnail_view_->SetTargetTabImage(preview_image);
}

void TabHoverCardBubbleView::SetPlaceholderImage() {
  DCHECK(thumbnail_view_)
      << "This method should only be called when preview images are enabled.";
  thumbnail_view_->SetPlaceholderImage();
}

std::u16string TabHoverCardBubbleView::GetTitleTextForTesting() const {
  return title_label_->GetText();
}

std::u16string TabHoverCardBubbleView::GetDomainTextForTesting() const {
  return domain_label_->GetText();
}

// static
absl::optional<double> TabHoverCardBubbleView::GetPreviewImageCrossfadeStart() {
  // For consistency, always bail out with a "don't crossfade" response if
  // animations are disabled.
  if (!TabHoverCardController::UseAnimations())
    return absl::nullopt;

  static const double start_percent = base::GetFieldTrialParamByFeatureAsDouble(
      features::kTabHoverCardImages,
      features::kTabHoverCardImagesCrossfadePreviewAtParameterName, 0.25);
  return start_percent >= 0.0
             ? absl::make_optional(base::clamp(start_percent, 0.0, 1.0))
             : absl::nullopt;
}

gfx::Size TabHoverCardBubbleView::CalculatePreferredSize() const {
  gfx::Size preferred_size = GetLayoutManager()->GetPreferredSize(this);
  preferred_size.set_width(TabStyle::GetPreviewImageSize().width());
  DCHECK(!preferred_size.IsEmpty());
  return preferred_size;
}

BEGIN_METADATA(TabHoverCardBubbleView, views::BubbleDialogDelegateView)
END_METADATA
