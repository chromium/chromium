// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EVENT_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_EVENT_UTILS_H_

#include <optional>

#include "chrome/browser/ui/views/chrome_views_export.h"

namespace ui {
class Event;
class KeyEvent;
}

namespace event_utils {

// Returns true if the specified event may have a
// WindowOptionDisposition.
CHROME_VIEWS_EXPORT bool IsPossibleDispositionEvent(const ui::Event& event);

enum class ReorderDirection {
  // The item should move to a previous entry in an ordered list (this
  // corresponds to "left" in a left-to-right UI).
  kPrevious,
  // The item should move to a further entry in an ordered list (this
  // corresponds to "right" in a left-to-right UI).
  kNext,
};

// Some keyboard commands are used to reorder items, such as tabs in the
// tabstrip or actions in the toolbar. If the event corresponds to a reorder
// command, returns the direction of the reorder. If the event does not
// correspond to a reorder, returns nullopt.
CHROME_VIEWS_EXPORT std::optional<ReorderDirection>
GetReorderCommandForKeyboardEvent(const ui::KeyEvent& event);

}  // namespace event_utils

#endif  // CHROME_BROWSER_UI_VIEWS_EVENT_UTILS_H_
