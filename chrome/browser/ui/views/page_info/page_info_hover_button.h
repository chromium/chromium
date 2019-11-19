// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_HOVER_BUTTON_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/hover_button.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

namespace views {
class ButtonListener;
class Label;
class StyledLabel;
class View;
}  // namespace views

class PageInfoBubbleViewBrowserTest;

// Hoverable button containing icon, styled title, and (multi-line) subtitle.
// PageInfoHoverButton inherits the interaction behavior from HoverButton but
// sets up its own layout and content.
class PageInfoHoverButton : public HoverButton {
 public:
  METADATA_HEADER(PageInfoHoverButton);

  // Creates a hoverable button that formats the string given by
  // |title_resource_id| with |secondary_text| and displays the latter part in
  // the secondary text color. The |subtitle_text| is shown below the title text
  // in secondary text color. |tooltip_text| is used for the tooltip shown on
  // hovering over the button.
  // *-----------------------------------------------------------------*
  // | Icon | Title |title_resource_id| string + |secondary_text|      |
  // |-----------------------------------------------------------------|
  // |      | |subtitle_text|                                          |
  // *-----------------------------------------------------------------*
  PageInfoHoverButton(views::ButtonListener* listener,
                      const gfx::ImageSkia& image_icon,
                      int title_resource_id,
                      const base::string16& secondary_text,
                      int click_target_id,
                      const base::string16& tooltip_text,
                      const base::string16& subtitle_text);
  ~PageInfoHoverButton() override {}

  // Updates the title text, and applies the secondary style to the secondary
  // text portion, if present.
  void SetTitleText(int title_resource_id,
                    const base::string16& secondary_text);

 protected:
  views::StyledLabel* title() const { return title_; }
  views::Label* subtitle() const { return subtitle_; }
  views::View* icon_view() const { return icon_view_; }
  // HoverButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int w) const override;

 private:
  friend class PageInfoBubbleViewBrowserTest;
  friend class test::PageInfoBubbleViewTestApi;

  void UpdateAccessibleName();

  views::StyledLabel* title_ = nullptr;
  views::Label* subtitle_ = nullptr;
  views::View* icon_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PageInfoHoverButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_HOVER_BUTTON_H_
