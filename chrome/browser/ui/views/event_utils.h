// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EVENT_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_EVENT_UTILS_H_

#include "chrome/browser/ui/views/chrome_views_export.h"

namespace ui {
class Event;
}

namespace event_utils {

// Returns true if the specified event may have a
// WindowOptionDisposition.
CHROME_VIEWS_EXPORT bool IsPossibleDispositionEvent(const ui::Event& event);

}  // namespace event_utils

#endif  // CHROME_BROWSER_UI_VIEWS_EVENT_UTILS_H_
