// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_HOVER_BUTTON_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/metadata/view_factory.h"

namespace media_router {
FORWARD_DECLARE_TEST(CastDialogSinkButtonTest, SetTitleLabel);
FORWARD_DECLARE_TEST(CastDialogSinkButtonTest, SetStatusLabel);
}  // namespace media_router

namespace ui {
class ImageModel;
}

namespace views {
class Label;
class StyledLabel;
class View;
}  // namespace views

class PageInfoBubbleViewBrowserTest;

// A button taking the full width of its parent that shows a background color
// when hovered over.
class HoverButton : public views::LabelButton {
 public:
  METADATA_HEADER(HoverButton);

  enum Style { STYLE_PROMINENT, STYLE_ERROR };

  // Creates a single line hover button with no icon.
  HoverButton(PressedCallback callback, const std::u16string& text);

  // Creates a single line hover button with an icon.
  HoverButton(PressedCallback callback,
              const ui::ImageModel& icon,
              const std::u16string& text);

  // Creates a HoverButton with custom subviews. |icon_view| replaces the
  // LabelButton icon, and titles appear on separate rows. An empty |subtitle|
  // will vertically center |title|. |secondary_view|, when set, is shown
  // on the opposite side of the button from |icon_view|.
  // When |resize_row_for_secondary_icon| is false, the button tries to
  // accommodate the view's preferred size by reducing the top and bottom
  // insets appropriately up to a value of 0.
  // Warning: |icon_view| must have a fixed size and be correctly set during its
  // constructor for the HoverButton to layout correctly.
  HoverButton(PressedCallback callback,
              std::unique_ptr<views::View> icon_view,
              const std::u16string& title,
              const std::u16string& subtitle = std::u16string(),
              std::unique_ptr<views::View> secondary_view = nullptr,
              bool resize_row_for_secondary_view = true,
              bool secondary_view_can_process_events = false);

  HoverButton(const HoverButton&) = delete;
  HoverButton& operator=(const HoverButton&) = delete;
  ~HoverButton() override;

  // views::LabelButton:
  void SetBorder(std::unique_ptr<views::Border> b) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void PreferredSizeChanged() override;
  void OnViewBoundsChanged(View* observed_view) override;

  // Sets the text style of the title considering the color of the background.
  // Passing |background_color| makes sure that the text color will not be
  // changed to a color that is not readable on the specified background.
  void SetTitleTextStyle(views::style::TextStyle text_style,
                         SkColor background_color);

  // Set the text context and style of the subtitle.
  void SetSubtitleTextStyle(int text_context,
                            views::style::TextStyle text_style);

  // Updates the accessible name and tooltip of the button if necessary based on
  // |title_| and |subtitle_| labels.
  void SetTooltipAndAccessibleName();

 protected:
  // views::MenuButton:
  KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event) override;
  void StateChanged(ButtonState old_state) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

  views::StyledLabel* title() const { return title_; }
  views::Label* subtitle() const { return subtitle_; }
  views::View* icon_view() const { return icon_view_; }
  views::View* secondary_view() const { return secondary_view_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(media_router::CastDialogSinkButtonTest,
                           SetTitleLabel);
  FRIEND_TEST_ALL_PREFIXES(media_router::CastDialogSinkButtonTest,
                           SetStatusLabel);
  FRIEND_TEST_ALL_PREFIXES(ExtensionsMenuItemViewTest,
                           NotifyClickExecutesAction);
  FRIEND_TEST_ALL_PREFIXES(ExtensionsMenuItemViewTest,
                           UpdatesToDisplayCorrectActionTitle);
  friend class AccountSelectionBubbleViewTest;
  friend class PageInfoBubbleViewBrowserTest;

  raw_ptr<views::StyledLabel> title_ = nullptr;
  raw_ptr<views::View> label_wrapper_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
  raw_ptr<views::View> icon_view_ = nullptr;
  raw_ptr<views::View> secondary_view_ = nullptr;

  base::ScopedObservation<views::View, views::ViewObserver> label_observation_{
      this};
};

BEGIN_VIEW_BUILDER(, HoverButton, views::LabelButton)
VIEW_BUILDER_METHOD(SetTitleTextStyle, views::style::TextStyle, SkColor)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, HoverButton)

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_HOVER_BUTTON_H_
