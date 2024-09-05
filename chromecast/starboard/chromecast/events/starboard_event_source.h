// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_CHROMECAST_EVENTS_STARBOARD_EVENT_SOURCE_H_
#define CHROMECAST_STARBOARD_CHROMECAST_EVENTS_STARBOARD_EVENT_SOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/starboard/chromecast/events/ui_event_source.h"
#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"
#include "ui/events/event.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace chromecast {

// Subscribes to |SbEvent| provided by
// |CastStarboardApiAdapter| and translates them to |ui::Event| before
// dispatching them to a |ui::PlatformWindowDelegate|.
//
// This class is used both to convert starboard events to ui::Event values, and
// to filter out events that did not originate from starboard.
class StarboardEventSource : public UiEventSource {
 public:
  // Creates an event source which will dispatch events to |delegate|.
  StarboardEventSource(ui::PlatformWindowDelegate* delegate);
  ~StarboardEventSource() override;

  // UiEventSource implementation:
  bool ShouldDispatchEvent(const ui::Event& event) override;

 private:
  // Receives |event| from Starboard and sends it to |context|, which represents
  // an instance of this class.
  static void SbEventHandle(void* context, const SbEvent* event);

  // Receives |event| from |SbEventHandle| and translates the event to a
  // |ui::Event|.
  void SbEventHandleInternal(const SbEvent* event);

  // Dispatches an |event| to the |delegate_| on the original |task_runner_| on
  // which |this| was created.
  void DispatchUiEvent(std::unique_ptr<ui::Event> event);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ui::PlatformWindowDelegate* delegate_;

  base::WeakPtrFactory<StarboardEventSource> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_CHROMECAST_EVENTS_STARBOARD_EVENT_SOURCE_H_
