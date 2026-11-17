// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_HANDLER_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_HANDLER_UTILS_H_

#include <string>

namespace base {
class Time;
}  // namespace base

namespace organizer_panel {

// Returns a short localized elapsed time string since `last_active_time`, or
// an empty string if `last_active_time` is null.
std::string GetLastActiveElapsedText(const base::Time& last_active_time);

}  // namespace organizer_panel

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_HANDLER_UTILS_H_
