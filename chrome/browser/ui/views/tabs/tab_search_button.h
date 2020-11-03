// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_

#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Canvas;
}

namespace views {
class Widget;
}

class TabStrip;

// TabSearchButton should leverage the look and feel of the existing
// NewTabButton for sizing and appropriate theming. This class updates the
// NewTabButton with the appropriate icon and will be used to anchor the
// Tab Search bubble.
//
// TODO(tluk): Break away common code from the NewTabButton and the
// TabSearchButton into a TabStripControlButton or similar.
class TabSearchButton : public NewTabButton,
                        public views::WidgetObserver {
 public:
  explicit TabSearchButton(TabStrip* tab_strip);
  TabSearchButton(const TabSearchButton&) = delete;
  TabSearchButton& operator=(const TabSearchButton&) = delete;
  ~TabSearchButton() override;

  // NewTabButton:
  void NotifyClick(const ui::Event& event) final;
  void FrameColorsChanged() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // When this is called the bubble may already be showing or be loading in.
  // This returns true if the method call results in the creation of a new Tab
  // Search bubble.
  bool ShowTabSearchBubble();

  bool IsBubbleVisible() const;

  views::Widget* bubble_for_testing() { return bubble_; }

 protected:
  // NewTabButton:
  void PaintIcon(gfx::Canvas* canvas) override;

 private:
  void ButtonPressed(const ui::Event& event);

  views::MenuButtonController* menu_button_controller_ = nullptr;

  // A lock to keep the TabSearchButton pressed while |bubble_| is showing or
  // in the process of being shown.
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  // |bubble_| is non-null while the tab search bubble is active.
  views::Widget* bubble_ = nullptr;

  ScopedObserver<views::Widget, views::WidgetObserver> observed_bubble_widget_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_
