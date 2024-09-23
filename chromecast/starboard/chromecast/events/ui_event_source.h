// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_CHROMECAST_EVENTS_UI_EVENT_SOURCE_H_
#define CHROMECAST_STARBOARD_CHROMECAST_EVENTS_UI_EVENT_SOURCE_H_

#include <memory>

#include "ui/events/event.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace chromecast {

// Represents events originating from some source. For example, if the user
// presses a directional button on the TV remote, that may trigger an event.
//
// A UiEventSource should be created via UiEventSource::Create.
// Platforms must define this function.
class UiEventSource {
 public:
  // Creates a UiEventSource that dispatches events to |delegate|.
  static std::unique_ptr<UiEventSource> Create(
      ui::PlatformWindowDelegate* delegate);

  virtual ~UiEventSource();

  // Returns true if the event should be dispatched.
  virtual bool ShouldDispatchEvent(const ui::Event& event) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_CHROMECAST_EVENTS_UI_EVENT_SOURCE_H_
