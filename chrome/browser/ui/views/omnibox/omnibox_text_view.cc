// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>

#include <algorithm>
#include <memory>

#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"

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
  gfx::BaselineStyle baseline = gfx::NORMAL_BASELINE;
};

// Returns the styles that should be applied to the specified answer text type.
//
// Note that the font value is only consulted for the first text type that
// appears on an answer line, because RenderText does not yet support multiple
// font sizes. Subsequent text types on the same line will share the text size
// of the first type, while the color and baseline styles specified here will
// always apply. The gfx::INFERIOR baseline style is used as a workaround to
// produce smaller text on the same line. The way this is used in the current
// set of answers is that the small types (TOP_ALIGNED, DESCRIPTION_NEGATIVE,
// DESCRIPTION_POSITIVE and SUGGESTION_SECONDARY_TEXT_SMALL) only ever appear
// following LargeFont text, so for consistency they specify LargeFont for the
// first value even though this is not actually used (since they're not the
// first value).
TextStyle GetTextStyle(int text_type) {
  // The size delta for large fonts in the legacy spec (per comment above, the
  // result is usually smaller due to the baseline style).
  constexpr int kLarge = ui::ResourceBundle::kLargeFontDelta;

  // The size delta for the smaller of font size in the touchable style. This
  // will always use the same baseline style.
  constexpr int kTouchableSmall = -3;

  switch (text_type) {
    case SuggestionAnswer::TOP_ALIGNED:
      return {OmniboxPart::RESULTS_TEXT_DIMMED, kLarge, kTouchableSmall,
              gfx::SUPERIOR};

    case SuggestionAnswer::DESCRIPTION_NEGATIVE:
      return {OmniboxPart::RESULTS_TEXT_NEGATIVE, kLarge, kTouchableSmall,
              gfx::INFERIOR};

    case SuggestionAnswer::DESCRIPTION_POSITIVE:
      return {OmniboxPart::RESULTS_TEXT_POSITIVE, kLarge, kTouchableSmall,
              gfx::INFERIOR};

    case SuggestionAnswer::ANSWER_TEXT_MEDIUM:
      return {OmniboxPart::RESULTS_TEXT_DIMMED};

    case SuggestionAnswer::ANSWER_TEXT_LARGE:
      // Note: There is no large font in the touchable spec.
      return {OmniboxPart::RESULTS_TEXT_DIMMED, kLarge};

    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_SMALL:
      return {OmniboxPart::RESULTS_TEXT_DIMMED, kLarge, kTouchableSmall,
              gfx::INFERIOR};

    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_MEDIUM:
      return {OmniboxPart::RESULTS_TEXT_DIMMED};

    case SuggestionAnswer::PERSONALIZED_SUGGESTION:
    case SuggestionAnswer::SUGGESTION:  // Fall through.
    default:
      return {OmniboxPart::RESULTS_TEXT_DEFAULT};
  }
}

const gfx::FontList& GetFontForType(int text_type) {
  const gfx::FontList& omnibox_font =
      views::style::GetFont(CONTEXT_OMNIBOX_PRIMARY, kTextStyle);
  if (ui::MaterialDesignController::touch_ui()) {
    int delta = GetTextStyle(text_type).touchable_size_delta;
    if (delta == 0)
      return omnibox_font;

    // Use the cache in ResourceBundle (gfx::FontList::Derive() is slow and
    // doesn't return a reference).
    return ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
        omnibox_font.GetFontSize() - gfx::FontList().GetFontSize() + delta);
  }

  int delta = GetTextStyle(text_type).legacy_size_delta;
  if (delta == kInherit)
    return omnibox_font;

  return ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(delta);
}

// The new answer layout has separate and different treatment of text styles,
// and as of writing both styling approaches need to be supported.  When old
// answer styles are deprecated, the above TextStyle structure and related
// logic can be removed, and this used exclusively.  This utility function
// applies new answer text styling for given text_type over range on render_text
// using result_view as a source for omnibox part colors.
void ApplyTextStyleForType(SuggestionAnswer::TextStyle text_style,
                           OmniboxResultView* result_view,
                           gfx::RenderText* render_text,
                           const gfx::Range& range) {
  struct TextStyleNewAnswers {
    void Apply(gfx::RenderText* render_text, const gfx::Range& range) {
      render_text->ApplyWeight(weight, range);
      render_text->ApplyBaselineStyle(baseline, range);
      render_text->ApplyColor(color, range);
    }
    SkColor color;
    gfx::BaselineStyle baseline = gfx::NORMAL_BASELINE;
    gfx::Font::Weight weight = gfx::Font::Weight::NORMAL;
  };

  TextStyleNewAnswers style;
  const SkColor part_color = result_view->GetColor(
      (text_style == SuggestionAnswer::TextStyle::NORMAL_DIM)
          ? OmniboxPart::RESULTS_TEXT_DIMMED
          : OmniboxPart::RESULTS_TEXT_DEFAULT);
  switch (text_style) {
    case SuggestionAnswer::TextStyle::SECONDARY:
      style = {gfx::kGoogleGrey600};
      break;
    case SuggestionAnswer::TextStyle::POSITIVE:
      style = {gfx::kGoogleGreen600};
      break;
    case SuggestionAnswer::TextStyle::NEGATIVE:
      style = {gfx::kGoogleRed600};
      break;
    case SuggestionAnswer::TextStyle::SUPERIOR:
      style = {part_color, .baseline = gfx::SUPERIOR};
      break;
    case SuggestionAnswer::TextStyle::BOLD:
      style = {part_color, .baseline = gfx::NORMAL_BASELINE,
               .weight = gfx::Font::Weight::BOLD};
      break;
    case SuggestionAnswer::TextStyle::NORMAL:
    case SuggestionAnswer::TextStyle::NORMAL_DIM:
    default:
      style = {part_color};
      break;
  }
  style.Apply(render_text, range);
}

}  // namespace

OmniboxTextView::OmniboxTextView(OmniboxResultView* result_view)
    : result_view_(result_view),
      font_height_(0),
      use_deemphasized_font_(false),
      wrap_text_lines_(false) {}

OmniboxTextView::~OmniboxTextView() {}

gfx::Size OmniboxTextView::CalculatePreferredSize() const {
  if (!render_text_)
    return gfx::Size();
  return render_text_->GetStringSize();
}

bool OmniboxTextView::CanProcessEventsWithinSubtree() const {
  return false;
}

const char* OmniboxTextView::GetClassName() const {
  return "OmniboxTextView";
}

int OmniboxTextView::GetHeightForWidth(int width) const {
  if (!render_text_)
    return 0;
  // If text wrapping is not called for we can simply return the font height.
  if (!wrap_text_lines_) {
    return GetLineHeight();
  }
  render_text_->SetDisplayRect(gfx::Rect(width, 0));
  gfx::Size string_size = render_text_->GetStringSize();
  return string_size.height() + kVerticalPadding;
}

void OmniboxTextView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (!render_text_) {
    return;
  }
  render_text_->SetDisplayRect(GetContentsBounds());
  render_text_->Draw(canvas);
}

void OmniboxTextView::ApplyTextColor(OmniboxPart part) {
  render_text_->SetColor(result_view_->GetColor(part));
}

const base::string16& OmniboxTextView::text() const {
  static const base::string16 kEmptyString;
  if (!render_text_)
    return kEmptyString;
  return render_text_->text();
}

void OmniboxTextView::SetText(const base::string16& text, bool deemphasize) {
  if (cached_classifications_) {
    cached_classifications_.reset();
  } else if (render_text_ && render_text_->text() == text &&
             deemphasize == use_deemphasized_font_) {
    // Only exit early if |cached_classifications_| was empty,
    // i.e. the last time text was set was through this method.
    return;
  }

  use_deemphasized_font_ = deemphasize;
  render_text_.reset();
  render_text_ = CreateRenderText(text);
  UpdateLineHeight();
  SetPreferredSize(CalculatePreferredSize());
}

void OmniboxTextView::SetText(const base::string16& text,
                              const ACMatchClassifications& classifications,
                              bool deemphasize) {
  if (render_text_ && render_text_->text() == text && cached_classifications_ &&
      classifications == *cached_classifications_ &&
      deemphasize == use_deemphasized_font_)
    return;

  use_deemphasized_font_ = deemphasize;

  cached_classifications_ =
      std::make_unique<ACMatchClassifications>(classifications);
  render_text_ = CreateRenderText(text);

  ReapplyStyling();
}

void OmniboxTextView::SetText(const SuggestionAnswer::ImageLine& line,
                              bool deemphasize) {
  use_deemphasized_font_ = deemphasize;
  cached_classifications_.reset();
  wrap_text_lines_ = line.num_text_lines() > 1;
  render_text_.reset();
  render_text_ = CreateRenderText(base::string16());

  if (!OmniboxFieldTrial::IsNewAnswerLayoutEnabled()) {
    // This assumes that the first text type in the line can be used to specify
    // the font for all the text fields in the line.  For now this works but
    // eventually it may be necessary to get RenderText to support multiple font
    // sizes or use multiple RenderTexts.
    render_text_->SetFontList(GetFontForType(line.text_fields()[0].type()));
  }

  for (const SuggestionAnswer::TextField& text_field : line.text_fields())
    AppendText(text_field, base::string16());
  if (!line.text_fields().empty()) {
    constexpr int kMaxDisplayLines = 3;
    const SuggestionAnswer::TextField& first_field = line.text_fields().front();
    if (first_field.has_num_lines() && first_field.num_lines() > 1 &&
        render_text_->MultilineSupported()) {
      render_text_->SetMultiline(true);
      render_text_->SetMaxLines(
          std::min(kMaxDisplayLines, first_field.num_lines()));
    }
  }

  // Add the "additional" and "status" text from |line|, if any.
  // Also updates preferred size.
  AppendExtraText(line);

  UpdateLineHeight();
}

void OmniboxTextView::AppendExtraText(const SuggestionAnswer::ImageLine& line) {
  const base::string16 space(1, base::char16(' '));
  const auto* text_field = line.additional_text();
  if (text_field) {
    AppendText(*text_field, space);
  }
  text_field = line.status_text();
  if (text_field) {
    AppendText(*text_field, space);
  }
  SetPreferredSize(CalculatePreferredSize());
}

int OmniboxTextView::GetLineHeight() const {
  return font_height_;
}

void OmniboxTextView::ReapplyStyling() {
  const ACMatchClassifications& classifications = *cached_classifications_;
  const size_t text_length = render_text_->text().length();
  for (size_t i = 0; i < classifications.size(); ++i) {
    const size_t text_start = classifications[i].offset;
    if (text_start >= text_length)
      break;

    const size_t text_end =
        (i < (classifications.size() - 1))
            ? std::min(classifications[i + 1].offset, text_length)
            : text_length;
    const gfx::Range current_range(text_start, text_end);

    // Calculate style-related data.
    if (classifications[i].style & ACMatchClassification::MATCH)
      render_text_->ApplyWeight(gfx::Font::Weight::BOLD, current_range);

    OmniboxPart part = OmniboxPart::RESULTS_TEXT_DEFAULT;
    if (classifications[i].style & ACMatchClassification::URL) {
      part = OmniboxPart::RESULTS_TEXT_URL;
      render_text_->SetDirectionalityMode(gfx::DIRECTIONALITY_AS_URL);
    } else if (classifications[i].style & ACMatchClassification::DIM) {
      part = OmniboxPart::RESULTS_TEXT_DIMMED;
    } else if (classifications[i].style & ACMatchClassification::INVISIBLE) {
      part = OmniboxPart::RESULTS_TEXT_INVISIBLE;
    }
    render_text_->ApplyColor(result_view_->GetColor(part), current_range);
  }

  UpdateLineHeight();
  SetPreferredSize(CalculatePreferredSize());
}

std::unique_ptr<gfx::RenderText> OmniboxTextView::CreateRenderText(
    const base::string16& text) const {
  auto render_text = gfx::RenderText::CreateHarfBuzzInstance();
  render_text->SetDisplayRect(gfx::Rect(gfx::Size(INT_MAX, 0)));
  render_text->SetCursorEnabled(false);
  render_text->SetElideBehavior(gfx::ELIDE_TAIL);
  const gfx::FontList& font = views::style::GetFont(
      (use_deemphasized_font_ ? CONTEXT_OMNIBOX_DEEMPHASIZED
                              : CONTEXT_OMNIBOX_PRIMARY),
      kTextStyle);
  render_text->SetFontList(font);
  render_text->SetText(text);
  return render_text;
}

void OmniboxTextView::AppendText(const SuggestionAnswer::TextField& field,
                                 const base::string16& prefix) {
  const base::string16& text =
      prefix.empty() ? field.text() : (prefix + field.text());
  if (text.empty())
    return;
  int offset = render_text_->text().length();
  gfx::Range range(offset, offset + text.length());
  render_text_->AppendText(text);
  if (OmniboxFieldTrial::IsNewAnswerLayoutEnabled()) {
    ApplyTextStyleForType(field.style(), result_view_, render_text_.get(),
                          range);
  } else {
    const int text_type = field.type();
    const TextStyle& text_style = GetTextStyle(text_type);
    // TODO(dschuyler): follow up on the problem of different font sizes within
    // one RenderText.  Maybe with render_text_->SetFontList(...).
    render_text_->ApplyWeight(gfx::Font::Weight::NORMAL, range);
    render_text_->ApplyColor(result_view_->GetColor(text_style.part), range);

    // Baselines are always aligned under the touch UI. Font sizes change
    // instead.
    if (!ui::MaterialDesignController::touch_ui()) {
      render_text_->ApplyBaselineStyle(text_style.baseline, range);
    } else if (text_style.touchable_size_delta != 0) {
      render_text_->ApplyFontSizeOverride(
          GetFontForType(text_type).GetFontSize(), range);
    }
  }
}

void OmniboxTextView::UpdateLineHeight() {
  const int height_normal = render_text_->font_list().GetHeight();
  const int height_bold =
      ui::ResourceBundle::GetSharedInstance()
          .GetFontListWithDelta(render_text_->font_list().GetFontSize() -
                                    gfx::FontList().GetFontSize(),
                                gfx::Font::NORMAL, gfx::Font::Weight::BOLD)
          .GetHeight();
  font_height_ = std::max(height_normal, height_bold);
  font_height_ += kVerticalPadding;
}
