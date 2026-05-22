// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_STATES_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_STATES_H_

namespace tabs_api::types {

struct TabStates {
  bool is_active;
  bool is_selected;
};

}  // namespace tabs_api::types

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_STATES_H_
