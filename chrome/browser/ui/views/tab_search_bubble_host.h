// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_BUBBLE_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_BUBBLE_HOST_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_utils.h"

namespace views {
class Widget;
}

class Profile;

// TabSearchBubbleHost assumes responsibility for configuring its button,
// showing / hiding the tab search bubble and handling metrics collection.
class TabSearchBubbleHost : public views::WidgetObserver {
 public:
  TabSearchBubbleHost(views::Button* button, Profile* profile);
  TabSearchBubbleHost(const TabSearchBubbleHost&) = delete;
  TabSearchBubbleHost& operator=(const TabSearchBubbleHost&) = delete;
  ~TabSearchBubbleHost() override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // When this is called the bubble may already be showing or be loading in.
  // This returns true if the method call results in the creation of a new Tab
  // Search bubble.
  bool ShowTabSearchBubble(bool triggered_by_keyboard_shortcut = false);
  void CloseTabSearchBubble();

  views::View* button() { return button_; }

  WebUIBubbleManager* webui_bubble_manager_for_testing() {
    return &webui_bubble_manager_;
  }
  const absl::optional<base::TimeTicks>& bubble_created_time_for_testing()
      const {
    return bubble_created_time_;
  }

 private:
  void ButtonPressed(const ui::Event& event);

  // The anchor button for the tab search bubble.
  const raw_ptr<views::Button> button_;

  const raw_ptr<Profile> profile_;

  WebUIBubbleManagerT<TabSearchUI> webui_bubble_manager_;

  views::WidgetOpenTimer widget_open_timer_;

  // Timestamp for when the current bubble was created.
  absl::optional<base::TimeTicks> bubble_created_time_;

  raw_ptr<views::MenuButtonController> menu_button_controller_ = nullptr;

  // A lock to keep its `button_` pressed while |bubble_| is showing or in the
  // process of being shown.
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_BUBBLE_HOST_H_
