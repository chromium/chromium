// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_TIMER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_TIMER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/time/time.h"

namespace ash::assistant {

// Represents the current state of an Assistant timer.
enum class AssistantTimerState {
  kUnknown,

  // The timer is scheduled to fire at some future date.
  kScheduled,

  // The timer will not fire but is kept in the queue of scheduled events;
  // it can be resumed after which it will fire in |remaining_time|.
  kPaused,

  // The timer has fired. In the simplest case this means the timer has
  // begun ringing.
  kFired,
};

// Models an Assistant timer.
struct COMPONENT_EXPORT(LIBASSISTANT_PUBLIC_STRUCTS) AssistantTimer {
  AssistantTimer();
  AssistantTimer(const AssistantTimer&);
  AssistantTimer& operator=(const AssistantTimer&);
  ~AssistantTimer();

  // Returns whether |this| is considered equal in LibAssistant to |other|.
  // NOTE: this *only* checks against fields which are set by LibAssistant.
  bool IsEqualInLibAssistantTo(const AssistantTimer& other) const;

  // These fields are set *only* by LibAssistant.
  std::string id;
  std::string label;
  base::Time fire_time;
  base::TimeDelta original_duration;

  // These fields are set *only* by Chrome.
  std::optional<base::Time> creation_time;
  base::TimeDelta remaining_time;

  // This field is set *only* by LibAssistant *except* in timers v2 where we may
  // update a timer from |kScheduled| to |kFired| state in Chrome in order to
  // reduce UI jank that would otherwise occur if we waited for LibAssistant to
  // notify us of the state change.
  AssistantTimerState state{AssistantTimerState::kUnknown};
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_TIMER_H_
