// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TEXT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TEXT_VIEW_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "ui/gfx/font_list.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
class RenderText;
}  // namespace gfx

// A view containing a render text styled via search results. This differs from
// the general purpose views::Label class by having less general features (such
// as selection) and more specific features (such as suggestion answer styling).
class OmniboxTextView : public views::View {
 public:
  explicit OmniboxTextView(OmniboxResultView* result_view);
  ~OmniboxTextView() override;

  // views::View.
  gfx::Size CalculatePreferredSize() const override;
  bool CanProcessEventsWithinSubtree() const override;
  const char* GetClassName() const override;
  int GetHeightForWidth(int width) const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // Applies given part's theme color to underlying render text. Using
  // OmniboxPart::RESULTS_TEXT_DIMMED gives the gray used by Dim() in the past.
  // This is called Apply* instead of Set* because the only state kept is in
  // render_text, so call this after setting text with methods below.
  void ApplyTextColor(OmniboxPart part);

  // Returns the render text, or an empty string if there is none.
  const base::string16& text() const;

  // Sets the render text with default rendering for the given |new_text|. The
  // |classifications| are used to style the text. An ImageLine incorporates
  // both the text and the styling.
  // |deemphasize| specifies whether to use a slightly smaller font than normal.
  void SetText(const base::string16& new_text, bool deemphasize = false);
  void SetText(const base::string16& new_text,
               const ACMatchClassifications& classifications,
               bool deemphasize = false);
  void SetText(const SuggestionAnswer::ImageLine& line,
               bool deemphasize = false);

  // Adds the "additional" and "status" text from |line|, if any.
  void AppendExtraText(const SuggestionAnswer::ImageLine& line);

  // Get the height of one line of text.  This is handy if the view might have
  // multiple lines.
  int GetLineHeight() const;

  // Reapplies text styling to the results text, based on the types of the match
  // parts.
  void ReapplyStyling();

  // Creates a platform-approriate RenderText, sets its format to that of
  // a suggestion and inserts (renders) the provided |text|.
  std::unique_ptr<gfx::RenderText> CreateRenderText(
      const base::string16& text) const;

 private:
  // Adds text from an answer field to the render text using appropriate style.
  // A prefix (such as separating space) may also be prepended to field text.
  void AppendText(const SuggestionAnswer::TextField& field,
                  const base::string16& prefix);

  // Updates the cached maximum line height and recomputes the preferred size.
  void OnStyleChanged();

  // To get color values.
  OmniboxResultView* result_view_;

  // Font settings for this view.
  int font_height_ = 0;

  // Whether to apply deemphasized font instead of primary omnibox font.
  // TODO(orinj): Use a more general ChromeTextContext for flexibility, or
  // otherwise clean up & unify the different ways of selecting fonts & styles.
  bool use_deemphasized_font_ = false;

  // Whether to wrap lines if the width is too narrow for the whole string.
  bool wrap_text_lines_ = false;

  // The primary data for this class.
  std::unique_ptr<gfx::RenderText> render_text_;
  // The classifications most recently passed to SetText. Used to exit
  // early instead of setting text when the text and classifications
  // match the current state of the view.
  std::unique_ptr<ACMatchClassifications> cached_classifications_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxTextView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TEXT_VIEW_H_
