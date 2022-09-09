// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_HOVER_BUTTON_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/hover_button.h"

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class Label;
class StyledLabel;
class View;
}  // namespace views

class PageInfoBubbleViewBrowserTest;

// Hoverable button containing icon, styled title, and (multi-line) subtitle.
// 'PageInfoHoverButton' inherits the interaction behavior from 'HoverButton'
// but sets up its own layout and content.
class PageInfoHoverButton : public HoverButton {
 public:
  METADATA_HEADER(PageInfoHoverButton);

  // Creates a hoverable button that formats the string given by
  // |title_resource_id| with |secondary_text| and displays the latter part in
  // the secondary text color. The |subtitle_text| is shown below the title text
  // in secondary text color. |tooltip_text| is used for the tooltip shown on
  // hovering over the button.
  // *-------------------------------------------------------------------------*
  // | Icon | |title_resource_id| + |secondary_text|                           |
  // |-------------------------------------------------------------------------|
  // |      | |subtitle_text|                                                  |
  // *-------------------------------------------------------------------------*
  // If flag PageInfoV2Desktop is enabled, the button will look different.
  // Optional |action_image_icom| is shown on right side. |secondary_text| isn't
  // concatenated with the |title_resource_id|, it is shown separately on right
  // side before the |action_image_icon|.
  // *-------------------------------------------------------------------------*
  // | Icon | |title_resource_id|               |secondary_text| | Action icon |
  // |-------------------------------------------------------------------------|
  // |      | |subtitle_text|                                                  |
  // *-------------------------------------------------------------------------*
  PageInfoHoverButton(
      views::Button::PressedCallback callback,
      const ui::ImageModel& main_image_icon,
      int title_resource_id,
      const std::u16string& secondary_text,
      int click_target_id,
      const std::u16string& tooltip_text,
      const std::u16string& subtitle_text,
      absl::optional<ui::ImageModel> action_image_icon = absl::nullopt);

  PageInfoHoverButton(const PageInfoHoverButton&) = delete;
  PageInfoHoverButton& operator=(const PageInfoHoverButton&) = delete;

  ~PageInfoHoverButton() override = default;

  // Updates the title text, and applies the secondary style to the secondary
  // text portion, if present.
  void SetTitleText(int title_resource_id,
                    const std::u16string& secondary_text);

  void SetTitleText(const std::u16string& title_text);

  void SetSubtitleText(const std::u16string& subtitle_text);

  void SetSubtitleMultiline(bool is_multiline);

 protected:
  views::StyledLabel* title() const { return title_; }
  views::Label* subtitle() const { return subtitle_; }
  // HoverButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int w) const override;

 private:
  friend class PageInfoBubbleViewBrowserTest;
  friend class test::PageInfoBubbleViewTestApi;

  void UpdateAccessibleName();

  raw_ptr<views::StyledLabel> title_ = nullptr;
  // Shows secondary text on right side. Used for page info v2 only.
  raw_ptr<views::Label> secondary_label_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_HOVER_BUTTON_H_
