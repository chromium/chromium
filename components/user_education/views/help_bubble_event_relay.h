// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_EVENT_RELAY_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_EVENT_RELAY_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace user_education {

class HelpBubbleView;

// Class that intercepts events in screen coordinates and determines whether
// they should be applied to a help bubble. Owned by the help bubble, though it
// can be created elsewhere.
class HelpBubbleEventRelay {
 public:
  HelpBubbleEventRelay(const HelpBubbleEventRelay&) = delete;
  void operator=(const HelpBubbleEventRelay&) = delete;
  virtual ~HelpBubbleEventRelay();

  // Perform the initialization and attach to the help bubble. Derived classes
  // should call the base class implementation first.
  virtual void Init(HelpBubbleView* help_bubble);

  // Returns whether the help bubble should process events normally (as opposed
  // to only via this object).
  virtual bool ShouldHelpBubbleProcessEvents() const = 0;

  // Returns whether buttons on the help bubble should un-hover when a "mouse
  // exit" type event is detected.
  virtual bool ShouldUnHoverOnMouseExit() const = 0;

 protected:
  // Used by derived classes. If `capture_mouse_exit` is specified then an exit
  // event will un-hover the help bubble.
  HelpBubbleEventRelay();

  HelpBubbleView* help_bubble() { return help_bubble_; }

  // Called by derived classes when `event` is received at `screen_coordinates`.
  // Returns whether the help bubble fully handles the event.
  bool OnEvent(const ui::LocatedEvent& event, const gfx::Point& screen_coords);

  // Notifies that the connection to the event source has been lost. If a click
  // has not already been forwarded to a button, this usually closes the help
  // bubble.
  void OnConnectionLost();

 private:
  // Returns the button at `screen_coords` in `help_bubble`, or null if none.
  views::Button* GetButtonAt(const gfx::Point& screen_coords) const;

  raw_ptr<HelpBubbleView> help_bubble_ = nullptr;
  raw_ptr<views::Button> hovered_button_ = nullptr;
  bool sent_click_ = false;
};

namespace internal {

// Because menus use event capture, a help bubble anchored to a menu cannot
// respond to events in the normal way. However, help bubbles are not
// complicated and only have buttons. When a help bubble is anchored to a menu,
// this object will monitor events that would be captured by the menu, and
// ensures that the buttons on the help bubble still behavior predictably.
class MenuHelpBubbleEventProcessor : public HelpBubbleEventRelay {
 public:
  explicit MenuHelpBubbleEventProcessor(views::MenuItemView* menu_item);
  ~MenuHelpBubbleEventProcessor() override;

  // HelpBubbleEventProcessor:
  bool ShouldHelpBubbleProcessEvents() const override;
  bool ShouldUnHoverOnMouseExit() const override;

 private:
  bool OnEvent(const ui::LocatedEvent& event);

  base::CallbackListSubscription callback_handle_;
};

}  // namespace internal

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_EVENT_RELAY_H_
