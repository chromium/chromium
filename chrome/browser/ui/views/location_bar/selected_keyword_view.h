// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SELECTED_KEYWORD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SELECTED_KEYWORD_VIEW_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/label.h"

class LocationBarView;
class Profile;
class TemplateURLService;

namespace gfx {
class FontList;
class Size;
}  // namespace gfx

// SelectedKeywordView displays the tab-to-search UI in the location bar view.
class SelectedKeywordView : public IconLabelBubbleView {
 public:
  struct KeywordLabelNames {
    base::string16 short_name;
    base::string16 full_name;
  };
  // Returns the short and long names that can be used to describe keyword
  // behavior, e.g. "Search google.com" or an equivalent translation, with
  // consideration for bidirectional text safety using |service|. Empty
  // names are returned if service is null.
  static KeywordLabelNames GetKeywordLabelNames(const base::string16& keyword,
                                                TemplateURLService* service);

  SelectedKeywordView(LocationBarView* location_bar,
                      const gfx::FontList& font_list);
  ~SelectedKeywordView() override;

  // Sets the icon for this chip to |image|.  If there is no custom image (i.e.
  // |image| is empty), resets the icon for this chip to its default.
  void SetCustomImage(const gfx::Image& image);

  // IconLabelBubbleView:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  void OnThemeChanged() override;
  SkColor GetForegroundColor() const override;

  // The current keyword, or an empty string if no keyword is displayed.
  void SetKeyword(const base::string16& keyword, Profile* profile);
  const base::string16& keyword() const { return keyword_; }

  using IconLabelBubbleView::label;

 private:
  // IconLabelBubbleView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  int GetExtraInternalSpacing() const override;
  const char* GetClassName() const override;

  void SetLabelForCurrentWidth();

  LocationBarView* location_bar_;

  // The keyword we're showing. If empty, no keyword is selected.
  // NOTE: we don't cache the TemplateURL as it is possible for it to get
  // deleted out from under us.
  base::string16 keyword_;

  // These labels are never visible.  They are used to size the view.  One
  // label contains the complete description of the keyword, the second
  // contains a truncated version of the description, for if there is not
  // enough room to display the complete description.
  views::Label full_label_;
  views::Label partial_label_;

  // True when the chip icon has been changed via SetCustomImage().
  bool using_custom_image_ = false;

  DISALLOW_COPY_AND_ASSIGN(SelectedKeywordView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SELECTED_KEYWORD_VIEW_H_
