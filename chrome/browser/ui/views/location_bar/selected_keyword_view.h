// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SELECTED_KEYWORD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SELECTED_KEYWORD_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

class Profile;
class TemplateURLService;

namespace gfx {
class FontList;
class Size;
}  // namespace gfx

// SelectedKeywordView displays the tab-to-search UI in the location bar view.
class SelectedKeywordView : public IconLabelBubbleView {
  METADATA_HEADER(SelectedKeywordView, IconLabelBubbleView)

 public:
  struct KeywordLabelNames {
    std::u16string short_name;
    std::u16string full_name;
  };
  // Returns the short and long names that can be used to describe keyword
  // behavior, e.g. "Search google.com" or an equivalent translation, with
  // consideration for bidirectional text safety using |service|. Empty
  // names are returned if service is null.
  static KeywordLabelNames GetKeywordLabelNames(
      const std::u16string& keyword,
      const TemplateURLService* service);

  SelectedKeywordView(IconLabelBubbleView::Delegate* delegate,
                      Profile* profile,
                      const gfx::FontList& font_list);
  SelectedKeywordView(const SelectedKeywordView&) = delete;
  SelectedKeywordView& operator=(const SelectedKeywordView&) = delete;
  ~SelectedKeywordView() override;

  // Sets the icon for this chip to |image|.  If there is no custom image (i.e.
  // |image| is empty), resets the icon for this chip to its default.
  void SetCustomImage(const gfx::Image& image);

  // IconLabelBubbleView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  void OnThemeChanged() override;
  SkColor GetForegroundColor() const override;

  // The current keyword, or an empty string if no keyword is displayed.
  void SetKeyword(const std::u16string& keyword);
  const std::u16string& GetKeyword() const;

  using IconLabelBubbleView::label;

 private:
  // IconLabelBubbleView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  int GetExtraInternalSpacing() const override;

  void SetLabelForCurrentWidth();

  // May be nullptr in tests.
  const raw_ptr<Profile> profile_;

  // The keyword we're showing. If empty, no keyword is selected.
  // NOTE: we don't cache the TemplateURL as it is possible for it to get
  // deleted out from under us.
  std::u16string keyword_;

  // These labels are never visible.  They are used to size the view.  One
  // label contains the complete description of the keyword, the second
  // contains a truncated version of the description, for if there is not
  // enough room to display the complete description.
  views::Label full_label_;
  views::Label partial_label_;

  // True when the chip icon has been changed via SetCustomImage().
  bool using_custom_image_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SELECTED_KEYWORD_VIEW_H_
