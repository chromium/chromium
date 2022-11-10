// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/hover_button.h"

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

// Hoverable button containing icon, styled title, and (multi-line) subtitle.
// 'RichHoverButton' inherits the interaction behavior from 'HoverButton'
// but sets up its own layout and content.
class RichHoverButton : public HoverButton {
 public:
  METADATA_HEADER(RichHoverButton);

  // Creates a hoverable button that displays the string given by
  // |title_resource_id| and |secondary_text| and displays the latter part in
  // the secondary text color. Optional |action_image_icon| is shown on right
  // side. |secondary_text| is shown on right side before the
  // |action_image_icon|. |tooltip_text| is used for the tooltip shown on
  // hovering over the button.
  // *-------------------------------------------------------------------------*
  // | Icon | |title_resource_id|   |secondary_text| State image | Action icon |
  // |-------------------------------------------------------------------------|
  // |      | |subtitle_text|                                                  |
  // *-------------------------------------------------------------------------*
  RichHoverButton(
      views::Button::PressedCallback callback,
      const ui::ImageModel& main_image_icon,
      int title_resource_id,
      const std::u16string& secondary_text,
      int click_target_id,
      const std::u16string& tooltip_text,
      const std::u16string& subtitle_text,
      absl::optional<ui::ImageModel> action_image_icon = absl::nullopt,
      absl::optional<ui::ImageModel> state_icon = absl::nullopt);

  RichHoverButton(const RichHoverButton&) = delete;
  RichHoverButton& operator=(const RichHoverButton&) = delete;

  ~RichHoverButton() override = default;

  // Updates the title text, and applies the secondary style to the secondary
  // text portion, if present.
  void SetTitleText(int title_resource_id,
                    const std::u16string& secondary_text);

  void SetTitleText(const std::u16string& title_text);

  void SetSubtitleText(const std::u16string& subtitle_text);

  void SetSubtitleMultiline(bool is_multiline);

  const views::StyledLabel* GetTitleViewForTesting() const;
  const views::Label* GetSubTitleViewForTesting() const;

 protected:
  views::StyledLabel* title() const { return title_; }
  views::Label* subtitle() const { return subtitle_; }
  // HoverButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int w) const override;

 private:
  void UpdateAccessibleName();

  raw_ptr<views::StyledLabel> title_ = nullptr;
  raw_ptr<views::Label> secondary_label_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_
