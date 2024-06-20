// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_HELP_BUBBLE_EVENT_RELAY_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_HELP_BUBBLE_EVENT_RELAY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/user_education/views/help_bubble_event_relay.h"
#include "components/user_education/views/help_bubble_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if USE_AURA
#include "ui/events/event_handler.h"
#endif

// Observes the primary window when a help bubble is anchored to a non-focusable
// secondary window that goes away with user interaction (such as an autofill
// bubble or a hover card), and allows the help bubble to properly handle events
// without itself becoming focused. Not necessary for bubbles that disappear on
// loss of focus because help bubbles already pin their anchor widgets in place.
class WindowHelpBubbleEventRelay : public user_education::HelpBubbleEventRelay,
                                   public views::WidgetObserver {
 public:
  explicit WindowHelpBubbleEventRelay(views::Widget* source_widget);
  ~WindowHelpBubbleEventRelay() override;

  // HelpBubbleEventRelay:
  bool ShouldHelpBubbleProcessEvents() const override;
  bool ShouldUnHoverOnMouseExit() const override;

 protected:
  // Called on destruction and when the target widget goes away.
  // Call in derived destructors; safe to call multiple times.
  virtual void Release();

  views::Widget* source_widget() { return source_widget_; }

 private:
  // ui::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  raw_ptr<views::Widget> source_widget_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

#if USE_AURA

// Aura implementation of WindowHelpBubbleEventRelay.
class WindowHelpBubbleEventRelayAura : public WindowHelpBubbleEventRelay,
                                       public ui::EventHandler {
 public:
  explicit WindowHelpBubbleEventRelayAura(views::Widget* source_widget);
  ~WindowHelpBubbleEventRelayAura() override;

 private:
  // ui::EventHandler:
  void OnEvent(ui::Event* event) override;

  // WindowHelpBubbleEventHandler:
  void Release() override;
};

#elif BUILDFLAG(IS_MAC)

// Aura implementation of WindowHelpBubbleEventRelay.
class WindowHelpBubbleEventRelayMac : public WindowHelpBubbleEventRelay {
 public:
  explicit WindowHelpBubbleEventRelayMac(views::Widget* source_widget);
  ~WindowHelpBubbleEventRelayMac() override;

  // WindowHelpBubbleEventRelay:
  void Release() override;

 private:
  class Delegate;

  std::unique_ptr<Delegate> delegate_;
};

#endif  // BUILDFLAG(IS_MAC)

// Creates a platform-appropriate help bubble event processor for a help bubble
// that should be forwarded events from the widget behind it.
std::unique_ptr<WindowHelpBubbleEventRelay> CreateWindowHelpBubbleEventRelay(
    views::Widget* source_widget);

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_HELP_BUBBLE_EVENT_RELAY_H_
