// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_MATCH_CELL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_MATCH_CELL_VIEW_H_

#include "ui/views/view.h"

namespace views {
class ImageView;
}

struct AutocompleteMatch;
class OmniboxResultView;
class OmniboxTextView;

class OmniboxMatchCellView : public views::View {
 public:
  // The right-hand margin used for rows.
  static constexpr int kMarginRight = 8;

  explicit OmniboxMatchCellView(OmniboxResultView* result_view);
  ~OmniboxMatchCellView() override;

  views::ImageView* icon() { return icon_view_; }
  OmniboxTextView* content() { return content_view_; }
  OmniboxTextView* description() { return description_view_; }
  OmniboxTextView* separator() { return separator_view_; }

  static int GetTextIndent();

  void OnMatchUpdate(const OmniboxResultView* result_view,
                     const AutocompleteMatch& match);

  // Sets the answer image and, if the image is not square, sets the answer size
  // proportional to the image size to preserve its aspect ratio.
  void SetImage(const gfx::ImageSkia& image);

  // views::View:
  const char* GetClassName() const override;
  gfx::Insets GetInsets() const override;
  void Layout() override;
  bool CanProcessEventsWithinSubtree() const override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  enum class LayoutStyle {
    ONE_LINE_SUGGESTION,
    TWO_LINE_SUGGESTION,
  };

  void SetTailSuggestCommonPrefixWidth(const base::string16& common_prefix);

  bool is_rich_suggestion_ = false;
  bool is_search_type_ = false;
  LayoutStyle layout_style_ = LayoutStyle::ONE_LINE_SUGGESTION;

  // Weak pointers for easy reference.
  // An icon representing the type or content.
  views::ImageView* icon_view_;
  // The image for answers in suggest and rich entity suggestions.
  views::ImageView* answer_image_view_;
  OmniboxTextView* content_view_;
  OmniboxTextView* description_view_;
  OmniboxTextView* separator_view_;

  // This (permanently) holds the rendered width of
  // AutocompleteMatch::kEllipsis so that we don't have to keep calculating
  // it.
  int ellipsis_width_ = 0;

  // This holds the rendered width of the common prefix of a set of tail
  // suggestions so that it doesn't have to be re-calculated if the prefix
  // doesn't change.
  int tail_suggest_common_prefix_width_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OmniboxMatchCellView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_MATCH_CELL_VIEW_H_
