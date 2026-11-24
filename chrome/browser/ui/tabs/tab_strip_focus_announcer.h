// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_FOCUS_ANNOUNCER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_FOCUS_ANNOUNCER_H_

#include <string>

#include "base/scoped_observation.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;
class TabGroup;

namespace views {
class Widget;
}  // namespace views

// Announces tab strip focus mode transitions to screen readers for a browser
// window.
class TabStripFocusAnnouncer : public views::WidgetObserver {
 public:
  DECLARE_USER_DATA(TabStripFocusAnnouncer);

  explicit TabStripFocusAnnouncer(BrowserWindowInterface* browser);
  TabStripFocusAnnouncer(const TabStripFocusAnnouncer&) = delete;
  TabStripFocusAnnouncer& operator=(const TabStripFocusAnnouncer&) = delete;
  ~TabStripFocusAnnouncer() override;

  static TabStripFocusAnnouncer* From(BrowserWindowInterface* browser);

  void AnnounceFocusEntered(const TabGroup& group);
  void AnnounceFocusExited(const TabGroup& group);
  void CancelPendingAnnouncement();

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void Announce(std::u16string announcement);
  void FlushAnnouncement(views::Widget* widget);

  bool is_restoring_focused_group_ = false;
  std::u16string pending_announcement_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  ui::ScopedUnownedUserData<TabStripFocusAnnouncer> scoped_data_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_FOCUS_ANNOUNCER_H_
