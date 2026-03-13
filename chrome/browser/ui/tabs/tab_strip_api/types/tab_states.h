// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TYPES_TAB_STATES_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TYPES_TAB_STATES_H_

namespace tabs_api::types {

struct TabStates {
  bool is_active;
  bool is_selected;
};

}  // namespace tabs_api::types

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TYPES_TAB_STATES_H_
