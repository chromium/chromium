// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FOCUS_STATE_H_
#define COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FOCUS_STATE_H_

// Omnibox focus state.
enum OmniboxFocusState {
  // Not focused.
  OMNIBOX_FOCUS_NONE,

  // Visibly focused.
  OMNIBOX_FOCUS_VISIBLE,

  // Invisibly focused, i.e. focused with a hidden caret.
  //
  // Omnibox will not look focused visibly but any user key strokes will go to
  // the omnibox. Only used on desktop by search providers supplying a custom
  // new tab page with a fakebox.
  OMNIBOX_FOCUS_INVISIBLE,

  OMNIBOX_FOCUS_STATE_LAST = OMNIBOX_FOCUS_INVISIBLE
};

// Reasons why the Omnibox focus state could change.
enum OmniboxFocusChangeReason {
  // Includes any explicit changes to focus. (e.g. user clicking to change
  // focus, user tabbing to change focus, any explicit calls to SetFocus,
  // etc.)
  OMNIBOX_FOCUS_CHANGE_EXPLICIT,

  // Focus changed to restore state from a tab the user switched to.
  OMNIBOX_FOCUS_CHANGE_TAB_SWITCH,

  // Focus changed because user started typing. This only happens when focus
  // state is INVISIBLE (and this results in a change to VISIBLE).
  OMNIBOX_FOCUS_CHANGE_TYPING,

  OMNIBOX_FOCUS_CHANGE_REASON_LAST = OMNIBOX_FOCUS_CHANGE_TYPING
};

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_FOCUS_STATE_H_
