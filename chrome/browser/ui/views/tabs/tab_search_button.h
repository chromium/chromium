// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_

#include "base/time/time.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_utils.h"

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
  METADATA_HEADER(TabSearchButton);
  explicit TabSearchButton(TabStrip* tab_strip);
  TabSearchButton(const TabSearchButton&) = delete;
  TabSearchButton& operator=(const TabSearchButton&) = delete;
  ~TabSearchButton() override;

  // NewTabButton:
  void NotifyClick(const ui::Event& event) final;
  void FrameColorsChanged() override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // When this is called the bubble may already be showing or be loading in.
  // This returns true if the method call results in the creation of a new Tab
  // Search bubble.
  bool ShowTabSearchBubble(bool triggered_by_keyboard_shortcut = false);
  void CloseTabSearchBubble();

  WebUIBubbleManager* webui_bubble_manager_for_testing() {
    return &webui_bubble_manager_;
  }
  const base::Optional<base::TimeTicks>& bubble_created_time_for_testing()
      const {
    return bubble_created_time_;
  }

 protected:
  // NewTabButton:
  void PaintIcon(gfx::Canvas* canvas) override;

 private:
  void ButtonPressed(const ui::Event& event);

  WebUIBubbleManagerT<TabSearchUI> webui_bubble_manager_;

  views::WidgetOpenTimer widget_open_timer_;

  // Timestamp for when the current bubble was created.
  base::Optional<base::TimeTicks> bubble_created_time_;

  views::MenuButtonController* menu_button_controller_ = nullptr;

  // A lock to keep the TabSearchButton pressed while |bubble_| is showing or
  // in the process of being shown.
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_
