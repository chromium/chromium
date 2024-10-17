// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"

#include <limits.h>

#include <algorithm>
#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

namespace {

// Use the primary style for everything. TextStyle sometimes controls color, but
// we use OmniboxTheme for that.
constexpr int kTextStyle = views::style::STYLE_PRIMARY;

// Indicates to use CONTEXT_OMNIBOX_PRIMARY when picking a font size in legacy
// code paths.
constexpr int kInherit = INT_MIN;

// The vertical padding to provide each RenderText in addition to the height
// of the font. Where possible, RenderText uses this additional space to
// vertically center the cap height of the font instead of centering the
// entire font.
static constexpr int kVerticalPadding = 3;

struct TextStyle {
  OmniboxPart part;

  // The legacy size delta, relative to the ui::ResourceBundle BaseFont, or
  // kInherit to use CONTEXT_OMNIBOX_PRIMARY, to match the omnibox font.
  // Note: the actual font size may differ due to |baseline| altering the size.
  int legacy_size_delta = kInherit;

  // The size delta from the Touchable chrome spec. This is always relative to
  // CONTEXT_OMNIBOX_PRIMARY, which defaults to 15pt under touch. Only negative
  // deltas are supported correctly (the line height will not increase to fit).
  int touchable_size_delta = 0;

  // The baseline shift. Ignored under touch (text is always baseline-aligned).
  gfx::BaselineStyle baseline = gfx::BaselineStyle::kNormalBaseline;
};

void ApplyTextStyleFromColorType(
    const std::optional<omnibox::FormattedString::ColorType>& color_type,
    OmniboxResultView* result_view,
    gfx::RenderText* render_text,
    const gfx::Range& range) {
  render_text->ApplyWeight(gfx::Font::Weight::NORMAL, range);
  render_text->ApplyBaselineStyle(gfx::BaselineStyle::kNormalBaseline, range);
  const bool selected =
      result_view->GetThemeState() == OmniboxPartState::SELECTED;
  ui::ColorId id;
  if (color_type.value() ==
      omnibox::FormattedString::COLOR_ON_SURFACE_POSITIVE) {
    id = selected ? kColorOmniboxResultsTextPositiveSelected
                  : kColorOmniboxResultsTextPositive;
  } else if (color_type.value() ==
             omnibox::FormattedString::COLOR_ON_SURFACE_NEGATIVE) {
    id = selected ? kColorOmniboxResultsTextNegativeSelected
                  : kColorOmniboxResultsTextNegative;
  } else {
    return;
  }
  render_text->ApplyColor(result_view->GetColorProvider()->GetColor(id), range);
}

// Dictionary and translation answers have a max number of lines > 1.
bool AnswerHasDefinedMaxLines(omnibox::AnswerType answer_type) {
  return answer_type == omnibox::ANSWER_TYPE_DICTIONARY ||
         answer_type == omnibox::ANSWER_TYPE_TRANSLATION;
}

}  // namespace

OmniboxTextView::OmniboxTextView(OmniboxResultView* result_view)
    : result_view_(result_view) {}

OmniboxTextView::~OmniboxTextView() = default;

gfx::Size OmniboxTextView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (!render_text_) {
    return gfx::Size();
  }

  if (!available_size.width().is_bounded()) {
    render_text_->SetDisplayRect(gfx::Rect(gfx::Size(INT_MAX, 0)));
    return render_text_->GetStringSize();
  }

  int width = available_size.width().value();
  if (!wrap_text_lines_) {
    return gfx::Size(width, GetLineHeight());
  }

  if (cached_available_size_ != available_size ||
      cached_calculate_preferred_size_.IsEmpty()) {
    cached_available_size_ = available_size;
    render_text_->SetDisplayRect(gfx::Rect(width, 0));
    gfx::Size string_size = render_text_->GetStringSize();
    string_size.Enlarge(0, kVerticalPadding);
    cached_calculate_preferred_size_ = string_size;
  }
  return cached_calculate_preferred_size_;
}

bool OmniboxTextView::GetCanProcessEventsWithinSubtree() const {
  return false;
}

void OmniboxTextView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (!render_text_)
    return;
  render_text_->SetDisplayRect(GetContentsBounds());
  render_text_->Draw(canvas);
}

void OmniboxTextView::ApplyTextColor(ui::ColorId id) {
  if (GetText().empty())
    return;
  render_text_->SetColor(GetColorProvider()->GetColor(id));
  SchedulePaint();
}

const std::u16string& OmniboxTextView::GetText() const {
  return render_text_ ? render_text_->text() : base::EmptyString16();
}

void OmniboxTextView::SetText(const std::u16string& new_text) {
  if (cached_classifications_) {
    cached_classifications_.reset();
  } else if (GetText() == new_text) {
    // Only exit early if |cached_classifications_| was empty,
    // i.e. the last time text was set was through this method.
    return;
  }

  render_text_ = CreateRenderText(new_text);

  OnStyleChanged();
}

void OmniboxTextView::SetTextWithStyling(
    const std::u16string& new_text,
    const ACMatchClassifications& classifications) {
  if (GetText() == new_text && cached_classifications_ &&
      classifications == *cached_classifications_)
    return;

  cached_classifications_ =
      std::make_unique<ACMatchClassifications>(classifications);
  render_text_ = CreateRenderText(new_text);

  // `ReapplyStyling()` will update the preferred size and request a repaint.
  ReapplyStyling();
}

void OmniboxTextView::AppendTextWithStyling(
    const omnibox::FormattedString& formatted_string,
    size_t fragment_index,
    const omnibox::AnswerType& answer_type) {
  cached_classifications_.reset();
  wrap_text_lines_ = AnswerHasDefinedMaxLines(answer_type);
  for (size_t i = fragment_index;
       i < static_cast<size_t>(formatted_string.fragments_size()); i++) {
    const std::u16string space_separator = i == 0u ? u"" : u" ";
    const std::u16string append_text =
        space_separator +
        base::UTF8ToUTF16(formatted_string.fragments(i).text());
    size_t offset = render_text_ ? render_text_->text().length() : 0u;
    gfx::Range range(offset, offset + append_text.length());
    render_text_->AppendText(append_text);
    ApplyTextStyleFromColorType(formatted_string.fragments(i).color(),
                                result_view_, render_text_.get(), range);
  }
  OnStyleChanged();
}

void OmniboxTextView::SetMultilineText(
    const omnibox::FormattedString& formatted_string,
    const omnibox::AnswerType& answer_type) {
  render_text_ = CreateRenderText(u"");
  if (formatted_string.fragments_size() > 0 &&
      AnswerHasDefinedMaxLines(answer_type)) {
    render_text_->SetMultiline(true);
    render_text_->SetMaxLines(1);
  }
  AppendTextWithStyling(formatted_string, /*fragment_index=*/0u, answer_type);
}

void OmniboxTextView::SetMultilineText(const std::u16string& text) {
  // This is a "dirty" check to check if this call is a no-op and no state
  // change is required. It works only because `SetMultilineText()` is the only
  // place to set max lines to 3. It's not 100% correct because some of the
  // other setters don't reset max lines back to 1. Ideally, all these setter
  // methods would have a clean and robust way to verify no state change is
  // required and early exit. See comment at `wrap_text_lines_`'s declaration.
  if (!cached_classifications_ && GetText() == text && render_text_ &&
      render_text_->max_lines() == 3) {
    return;
  }
  cached_classifications_.reset();
  render_text_ = CreateRenderText(text);
  render_text_->SetColor(result_view_->GetColorProvider()->GetColor(
      kColorOmniboxResultsTextAnswer));
  render_text_->SetMultiline(true);
  render_text_->SetMaxLines(3);
  wrap_text_lines_ = true;
  OnStyleChanged();
}

int OmniboxTextView::GetLineHeight() const {
  return font_height_;
}

void OmniboxTextView::ReapplyStyling() {
  // No work required if there are no preexisting styles.
  if (!cached_classifications_)
    return;

  const size_t text_length = GetText().length();
  for (size_t i = 0; i < cached_classifications_->size(); ++i) {
    const size_t text_start = (*cached_classifications_)[i].offset;
    if (text_start >= text_length)
      break;

    const size_t text_end =
        (i < (cached_classifications_->size() - 1))
            ? std::min((*cached_classifications_)[i + 1].offset, text_length)
            : text_length;
    const gfx::Range current_range(text_start, text_end);

    // Calculate style-related data.
    if ((*cached_classifications_)[i].style & ACMatchClassification::MATCH)
      render_text_->ApplyWeight(gfx::Font::Weight::BOLD, current_range);

    const bool selected =
        result_view_->GetThemeState() == OmniboxPartState::SELECTED;
    ui::ColorId id =
        selected ? kColorOmniboxResultsTextSelected : kColorOmniboxText;
    if ((*cached_classifications_)[i].style & ACMatchClassification::URL) {
      id = selected ? kColorOmniboxResultsUrlSelected : kColorOmniboxResultsUrl;
      render_text_->SetDirectionalityMode(gfx::DIRECTIONALITY_AS_URL);
    } else if ((*cached_classifications_)[i].style &
               ACMatchClassification::DIM) {
      id = selected ? kColorOmniboxResultsTextDimmedSelected
                    : kColorOmniboxResultsTextDimmed;
    }
    render_text_->ApplyColor(GetColorProvider()->GetColor(id), current_range);
  }

  OnStyleChanged();
}

std::unique_ptr<gfx::RenderText> OmniboxTextView::CreateRenderText(
    const std::u16string& text) const {
  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  render_text->SetDisplayRect(gfx::Rect(gfx::Size(INT_MAX, 0)));
  render_text->SetCursorEnabled(false);
  render_text->SetElideBehavior(gfx::ELIDE_TAIL);
  const gfx::FontList& font = views::TypographyProvider::Get().GetFont(
      CONTEXT_OMNIBOX_POPUP, kTextStyle);
  render_text->SetFontList(font);
  render_text->SetText(text);
  // Increase space between lines for multiline texts.
  render_text->SetMinLineHeight(19);
  return render_text;
}

void OmniboxTextView::OnStyleChanged() {
  const int height_normal = render_text_->font_list().GetHeight();
  const int size_delta =
      render_text_->font_list().GetFontSize() - gfx::FontList().GetFontSize();
  const int height_bold =
      ui::ResourceBundle::GetSharedInstance()
          .GetFontListForDetails(ui::ResourceBundle::FontDetails(
              std::string(), size_delta, gfx::Font::Weight::BOLD))
          .GetHeight();
  font_height_ = std::max(height_normal, height_bold);
  font_height_ += kVerticalPadding;

  // Cache preferred size for 1-line matches only since their heights don't
  // depend on the available width.
  if (!wrap_text_lines_) {
    SetPreferredSize(CalculatePreferredSize({}));
  } else {
    SetPreferredSize({});
    cached_calculate_preferred_size_.SetSize(0, 0);
  }
  SchedulePaint();
}

BEGIN_METADATA(OmniboxTextView)
ADD_PROPERTY_METADATA(std::u16string, Text)
ADD_READONLY_PROPERTY_METADATA(int, LineHeight)
END_METADATA
