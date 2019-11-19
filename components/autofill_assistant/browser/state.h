// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STATE_H_

#include <ostream>

namespace autofill_assistant {

// High-level states the Autofill Assistant can be in.
//
// A typical run, when started from CCT, autostarts a script, then displays a
// prompt and continues until a script sends the Stop action:
//
// INACTIVE -> STARTING -> RUNNING -> PROMPT -> RUNNING -> .. -> STOPPED
//
// A typical run, when started from a direct action, goes into tracking mode,
// execute a script, the goes back to tracking mode:
//
// INACTIVE -> TRACKING -> RUNNING -> TRACKING -> ... -> STOPPED
//
// See the individual state for possible state transitions.
enum class AutofillAssistantState {
  // Autofill assistant is not doing or showing anything.
  //
  // Initial state.
  // Next states: STARTING, TRACKING, STOPPED
  INACTIVE = 0,

  // Autofill assistant is keeping track of script availability.
  //
  // In this mode, no UI is shown and scripts are not autostarted. User
  // actions might be available.
  //
  // Note that it is possible to go from TRACKING to STARTING to trigger
  // whatever autostartable scripts is defined for a page.
  //
  // Next states: STARTING, RUNNING, STOPPED
  TRACKING,

  // Autofill assistant is waiting for an autostart script.
  //
  // Status message, progress and details are initialized to useful values.
  //
  // Next states: RUNNING, AUTOSTART_FALLBACK_PROMPT, STOPPED
  STARTING,

  // Autofill assistant is manipulating the website.
  //
  // Status message, progress and details kept up-to-date by the running
  // script.
  //
  // Next states: PROMPT, MODAL_DIALOG, TRACKING, STARTING, STOPPED
  RUNNING,

  // Autofill assistant is waiting for the user to make a choice.
  //
  // Status message is initialized to a useful value. Chips are set and might be
  // empty. A touchable area must be configured. The user might be filling in
  // the data for a payment request.
  //
  // Next states: RUNNING, TRACKING, STOPPED
  PROMPT,

  // Autofill assistant is waiting for the user to make the first choice.
  //
  // When autostartable scripts are expected, this is only triggered as a
  // fallback if there are non-autostartable scripts to choose from instead.
  //
  // Next states: RUNNING, STOPPED
  AUTOSTART_FALLBACK_PROMPT,

  // Autofill assistant is expecting a modal dialog, such as the one asking for
  // CVC.
  //
  // Next states: RUNNING
  MODAL_DIALOG,

  // Autofill assistant is stopped, but the controller is still available.
  //
  // This is a final state for the UI, which, when entering this state, detaches
  // itself from the controller and lets the user read  the message.
  //
  // In that scenario, the status message at the time of transition to STOPPED
  // is supposed to contain the final message.
  //
  // Next states: TRACKING
  STOPPED,
};

inline std::ostream& operator<<(std::ostream& out,
                                const AutofillAssistantState& state) {
#ifdef NDEBUG
  // Non-debugging builds write the enum number.
  out << static_cast<int>(state);
  return out;
#else
  // Debugging builds write a string representation of |state|.
  switch (state) {
    case AutofillAssistantState::INACTIVE:
      out << "INACTIVE";
      break;
    case AutofillAssistantState::TRACKING:
      out << "TRACKING";
      break;
    case AutofillAssistantState::STARTING:
      out << "STARTING";
      break;
    case AutofillAssistantState::RUNNING:
      out << "RUNNING";
      break;
    case AutofillAssistantState::PROMPT:
      out << "PROMPT";
      break;
    case AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT:
      out << "AUTOSTART_FALLBACK_PROMPT";
      break;
    case AutofillAssistantState::MODAL_DIALOG:
      out << "MODAL_DIALOG";
      break;
    case AutofillAssistantState::STOPPED:
      out << "STOPPED";
      break;
      // Intentionally no default case to make compilation fail if a new value
      // was added to the enum but not to this list.
  }
  return out;
#endif  // NDEBUG
}

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STATE_H_
