// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_MATCH_CELL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_MATCH_CELL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}

struct AutocompleteMatch;
class OmniboxResultView;
class OmniboxTextView;

class OmniboxMatchCellView : public views::View {
 public:
  METADATA_HEADER(OmniboxMatchCellView);

  // Constants used in layout. Exposed so other views can coordinate margins.
  static constexpr int kMarginLeft = 4;
  static constexpr int kMarginRight = 8;
  static constexpr int kImageBoundsWidth = 40;

  // Computes the maximum width, in pixels, that can be allocated for the two
  // parts of an autocomplete result, i.e. the contents and the description.
  //
  // When |description_on_separate_line| is true, the caller will be displaying
  // two separate lines of text, so both contents and description can take up
  // the full available width. Otherwise, the contents and description are
  // assumed to be on the same line, with a separator between them.
  //
  // When |allow_shrinking_contents| is true, and the contents and description
  // are together on a line without enough space for both, the code tries to
  // divide the available space equally between the two, unless this would make
  // one or both too narrow. Otherwise, the contents is given as much space as
  // it wants and the description gets the remainder.
  static void ComputeMatchMaxWidths(int contents_width,
                                    int separator_width,
                                    int description_width,
                                    int available_width,
                                    bool description_on_separate_line,
                                    bool allow_shrinking_contents,
                                    int* contents_max_width,
                                    int* description_max_width);

  explicit OmniboxMatchCellView(OmniboxResultView* result_view);
  OmniboxMatchCellView(const OmniboxMatchCellView&) = delete;
  OmniboxMatchCellView& operator=(const OmniboxMatchCellView&) = delete;
  ~OmniboxMatchCellView() override;

  views::ImageView* icon() { return icon_view_; }
  OmniboxTextView* content() { return content_view_; }
  OmniboxTextView* description() { return description_view_; }
  OmniboxTextView* separator() { return separator_view_; }

  static int GetTextIndent();

  // Determines if `match` should display an answer, calculator, or entity
  // image.
  // If #omnibox-uniform-suggestion-height experiment flag is disabled, also
  // determines whether `match` should be displayed on 1 or 2 lines.
  static bool ShouldDisplayImage(const AutocompleteMatch& match);

  void OnMatchUpdate(const OmniboxResultView* result_view,
                     const AutocompleteMatch& match);

  // Sets the answer image and, if the image is not square, sets the answer size
  // proportional to the image size to preserve its aspect ratio.
  void SetImage(const gfx::ImageSkia& image);

  // views::View:
  gfx::Insets GetInsets() const override;
  void Layout() override;
  bool GetCanProcessEventsWithinSubtree() const override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  enum class LayoutStyle {
    ONE_LINE_SUGGESTION,
    TWO_LINE_SUGGESTION,
  };

  void SetTailSuggestCommonPrefixWidth(const std::u16string& common_prefix);

  bool is_search_type_ = false;
  bool has_image_ = false;
  LayoutStyle layout_style_ = LayoutStyle::ONE_LINE_SUGGESTION;

  // Weak pointers for easy reference.
  // An icon representing the type or content.
  raw_ptr<views::ImageView> icon_view_;
  // The image for answers in suggest and rich entity suggestions.
  raw_ptr<views::ImageView> answer_image_view_;
  raw_ptr<OmniboxTextView> tail_suggest_ellipse_view_;
  raw_ptr<OmniboxTextView> content_view_;
  raw_ptr<OmniboxTextView> description_view_;
  raw_ptr<OmniboxTextView> separator_view_;

  // This holds the rendered width of the common prefix of a set of tail
  // suggestions so that it doesn't have to be re-calculated if the prefix
  // doesn't change.
  int tail_suggest_common_prefix_width_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_MATCH_CELL_VIEW_H_
