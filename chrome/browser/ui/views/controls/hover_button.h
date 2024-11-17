// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_HOVER_BUTTON_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/pass_key.h"
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

class HoverButtonTest;
class HoverButtonController;
class PageInfoBubbleViewBrowserTest;

// A button taking the full width of its parent that shows a background color
// when hovered over.
class HoverButton : public views::LabelButton {
  METADATA_HEADER(HoverButton, views::LabelButton)

 public:
  enum Style { STYLE_PROMINENT, STYLE_ERROR };

  // Creates a single line hover button with no icon.
  HoverButton(PressedCallback callback, const std::u16string& text);

  // Creates a single line hover button with an icon.
  HoverButton(PressedCallback callback,
              const ui::ImageModel& icon,
              const std::u16string& text);

  // Creates a HoverButton with custom subviews. |icon_view| replaces the
  // LabelButton icon, and titles appear on separate rows. An empty |subtitle|
  // and |footer| will vertically center |title|. |footer| will be shown below
  // |title| and |subtitle|. |secondary_view|, when set, is shown on the
  // opposite side of the button from |icon_view|. When
  // |add_vertical_label_spacing| is false it will not add vertical spacing to
  // the label wrapper. Warning: |icon_view| must have a fixed size and be
  // correctly set during its constructor for the HoverButton to layout
  // correctly.
  HoverButton(PressedCallback callback,
              std::unique_ptr<views::View> icon_view,
              const std::u16string& title,
              const std::u16string& subtitle = std::u16string(),
              std::unique_ptr<views::View> secondary_view = nullptr,
              bool add_vertical_label_spacing = true,
              const std::u16string& footer = std::u16string());

  HoverButton(const HoverButton&) = delete;
  HoverButton& operator=(const HoverButton&) = delete;
  ~HoverButton() override;

  // views::LabelButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void SetBorder(std::unique_ptr<views::Border> b) override;
  void PreferredSizeChanged() override;
  void OnViewBoundsChanged(View* observed_view) override;

  // Sets the text style of the title considering the color of the background.
  // Passing |background_color| makes sure that the text color will not be
  // changed to a color that is not readable on the specified background.
  // Sets the title's enabled color to |color_id|, if present.
  void SetTitleTextStyle(views::style::TextStyle text_style,
                         SkColor background_color,
                         std::optional<ui::ColorId> color_id);

  // Set the text context and style of the subtitle.
  void SetSubtitleTextStyle(int text_context,
                            views::style::TextStyle text_style);

  // Set the text context and style of the footer.
  void SetFooterTextStyle(int text_context, views::style::TextStyle text_style);

  void SetIconHorizontalMargins(int left, int right);

  PressedCallback& callback(base::PassKey<HoverButtonController>) {
    return callback_;
  }

  views::StyledLabel* title() { return title_; }
  const views::StyledLabel* title() const { return title_; }

 protected:
  // views::MenuButton:
  KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event) override;
  void StateChanged(ButtonState old_state) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

  views::Label* subtitle() const { return subtitle_; }
  views::Label* footer() const { return footer_; }
  views::View* icon_view() const { return icon_view_; }
  views::View* secondary_view() const { return secondary_view_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(media_router::CastDialogSinkButtonTest,
                           SetTitleLabel);
  FRIEND_TEST_ALL_PREFIXES(media_router::CastDialogSinkButtonTest,
                           SetStatusLabel);
  friend class AccountSelectionViewTestBase;
  friend class HoverButtonTest;
  friend class PageInfoBubbleViewBrowserTest;

  // Updates the accessible name and tooltip of the button if necessary based on
  // `title_` and `subtitle_` labels.
  void UpdateTooltipAndAccessibleName();

  void OnPressed(const ui::Event& event);

  // Create the label for subtitle or footer.
  std::unique_ptr<views::Label> CreateSecondaryLabel(
      const std::u16string& text);

  PressedCallback callback_;

  raw_ptr<views::StyledLabel> title_ = nullptr;
  raw_ptr<views::View> icon_wrapper_ = nullptr;
  raw_ptr<views::View> label_wrapper_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
  raw_ptr<views::Label> footer_ = nullptr;
  raw_ptr<views::View> icon_view_ = nullptr;
  raw_ptr<views::View> secondary_view_ = nullptr;

  std::vector<base::CallbackListSubscription> text_changed_subscriptions_;

  base::ScopedObservation<views::View, views::ViewObserver> label_observation_{
      this};
};

BEGIN_VIEW_BUILDER(, HoverButton, views::LabelButton)
VIEW_BUILDER_METHOD(SetTitleTextStyle,
                    views::style::TextStyle,
                    SkColor,
                    std::optional<ui::ColorId>)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, HoverButton)

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_HOVER_BUTTON_H_
