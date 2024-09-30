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
class Link;
}  // namespace views

struct AutocompleteMatch;
class OmniboxResultView;
class OmniboxTextView;

class OmniboxMatchCellView : public views::View {
  METADATA_HEADER(OmniboxMatchCellView, views::View)

 public:
  // Constants used in layout. Exposed so other views can coordinate margins.

  // The gap between the popup's left edge (not the focus indicator's edge) and
  // `OmniboxMatchCellView`.
  static constexpr int kMarginLeft = 4;
  // Probably intended to be the gap between the popup's right edge (assuming no
  // buttons) and the text cut off. But this isn't used by
  // `OmniboxMatchCellView`. `OmniboxMatchCellView::GetInsets()` hardcodes 7; so
  // 8 here is probably wrong.
  static constexpr int kMarginRight = 8;
  // The width of icon, answer, and entity image bounds. These images are
  // smaller than this bounds; they'll be centered within the bounds.
  static constexpr int kImageBoundsWidth = 40;
  // For IPH matches, `OmniboxMatchCellView` is inset from the left & right by
  // `kIphOffset`.
  static constexpr int kIphOffset = 16;

  // Computes the maximum width, in pixels, that can be allocated for the two
  // parts of an autocomplete result, i.e. the contents and the description.
  //
  // When |allow_shrinking_contents| is true, and the contents and description
  // are together on a line without enough space for both, the code tries to
  // divide the available space equally between the two, unless this would make
  // one or both too narrow. Otherwise, the contents is given as much space as
  // it wants and the description gets the remainder.
  static void ComputeMatchMaxWidths(int contents_width,
                                    int separator_width,
                                    int description_width,
                                    int iph_link_width,
                                    int available_width,
                                    bool allow_shrinking_contents,
                                    int* contents_max_width,
                                    int* description_max_width,
                                    int* iph_link_max_width);

  explicit OmniboxMatchCellView(OmniboxResultView* result_view);
  OmniboxMatchCellView(const OmniboxMatchCellView&) = delete;
  OmniboxMatchCellView& operator=(const OmniboxMatchCellView&) = delete;
  ~OmniboxMatchCellView() override;

  views::ImageView* icon() { return icon_view_; }
  OmniboxTextView* content() { return content_view_; }
  OmniboxTextView* description() { return description_view_; }
  OmniboxTextView* separator() { return separator_view_; }
  views::Link* iph_link_view() { return iph_link_view_; }

  // Determines if `match` should display an answer, calculator, or entity
  // image.
  static bool ShouldDisplayImage(const AutocompleteMatch& match);

  void OnMatchUpdate(const OmniboxResultView* result_view,
                     const AutocompleteMatch& match);

  // Set's the `icon_view_` image, possibly with a rounded square background.
  void SetIcon(const gfx::ImageSkia& image, const AutocompleteMatch& match);

  // Clears the `icon_view_` image. Useful for suggestions that don't need icons
  // e.g., tail suggestions. Can't simply set the icon to an empty icon,
  // because doing so would still draw a background behind the icon.
  void ClearIcon();

  // Sets the answer image and, if the image is not square, sets the answer size
  // proportional to the image size to preserve its aspect ratio. `match`
  // correspond to the match for this view and is used to detect if this is a
  // weather answer (weather answers handle images differently).
  void SetImage(const gfx::ImageSkia& image, const AutocompleteMatch& match);

  // views::View:
  gfx::Insets GetInsets() const override;
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  enum class LayoutStyle {
    DEFAULT_NON_SEARCH_SUGGESTION,
    SEARCH_SUGGESTION,
    SEARCH_SUGGESTION_WITH_IMAGE,
    IPH_SUGGESTION,
  };

  // How far to indent the icon, entity, or answer image from the left side of
  // this view. Images are positioned ignoring `GetInsets()`; i.e., this
  // measures from the visual left edge of the popup.
  int GetImageIndent() const;

  // How far to indent the text from the left side of this view. Texts are
  // positioned considering `GetInsets()` but ignoring the width and positioning
  // of images; i.e., this measures from the visual left edge of the popup +
  // `kMarginLeft`. IPH matches are externally inset as well, so this will
  // measure from the left edge of the IPH background + `kMarginLeft`.
  int GetTextIndent() const;

  void SetTailSuggestCommonPrefixWidth(const std::u16string& common_prefix);

  LayoutStyle layout_style_ = LayoutStyle::DEFAULT_NON_SEARCH_SUGGESTION;

  // Weak pointers for easy reference.
  // An icon representing the type or content.
  raw_ptr<views::ImageView> icon_view_;
  // The image for answers in suggest and rich entity suggestions.
  raw_ptr<views::ImageView> answer_image_view_;
  raw_ptr<OmniboxTextView> tail_suggest_ellipse_view_;
  raw_ptr<OmniboxTextView> content_view_;
  raw_ptr<OmniboxTextView> description_view_;
  raw_ptr<OmniboxTextView> separator_view_;
  // Some IPH matches have a link users can click to learn more or take action.
  raw_ptr<views::Link> iph_link_view_;

  // This holds the rendered width of the common prefix of a set of tail
  // suggestions so that it doesn't have to be re-calculated if the prefix
  // doesn't change.
  int tail_suggest_common_prefix_width_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_MATCH_CELL_VIEW_H_
